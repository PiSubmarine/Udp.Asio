#include "PiSubmarine/Udp/Asio/Socket.h"

#include "PiSubmarine/Error/Api/MakeError.h"
#include "PiSubmarine/Udp/Api/ErrorCode.h"

namespace PiSubmarine::Udp::Asio
{
    Socket::Socket(const std::size_t receiveQueueCapacity, const std::size_t maxDatagramSize)
        : m_Socket(m_IoContext)
        , m_ReceiveSlots(receiveQueueCapacity)
        , m_MaxDatagramSize(maxDatagramSize)
    {
        for (auto& slot : m_ReceiveSlots)
        {
            slot.Payload.reserve(m_MaxDatagramSize);
        }
    }

    Error::Api::Result<void> Socket::Bind(const Api::Endpoint& localEndpoint)
    {
        if (m_IsBound)
        {
            return std::unexpected(MakeApiError(
                Error::Api::ErrorCondition::ContractError,
                Api::ErrorCode::AlreadyBound));
        }

        const auto endpointResult = ToAsioEndpoint(localEndpoint, true);
        if (!endpointResult)
        {
            return std::unexpected(endpointResult.error());
        }

        boost::system::error_code errorCode;
        m_Socket.open(endpointResult->protocol(), errorCode);
        if (errorCode)
        {
            return std::unexpected(MakeSystemError(Error::Api::ErrorCondition::EnvironmentError, errorCode));
        }

        m_Socket.set_option(boost::asio::socket_base::reuse_address(true), errorCode);
        if (errorCode)
        {
            return std::unexpected(MakeSystemError(Error::Api::ErrorCondition::EnvironmentError, errorCode));
        }

        m_Socket.bind(*endpointResult, errorCode);
        if (errorCode)
        {
            return std::unexpected(MakeSystemError(Error::Api::ErrorCondition::EnvironmentError, errorCode));
        }

        m_Socket.non_blocking(true, errorCode);
        if (errorCode)
        {
            return std::unexpected(MakeSystemError(Error::Api::ErrorCondition::EnvironmentError, errorCode));
        }

        m_IsBound = true;
        return {};
    }

    Error::Api::Result<void> Socket::Poll()
    {
        if (!m_IsBound)
        {
            return std::unexpected(MakeApiError(
                Error::Api::ErrorCondition::ContractError,
                Api::ErrorCode::NotBound));
        }

        while (true)
        {
            if (m_ReceiveCount == m_ReceiveSlots.size())
            {
                return std::unexpected(MakeApiError(
                    Error::Api::ErrorCondition::TemporaryFailure,
                    Api::ErrorCode::ReceiveQueueOverflow));
            }

            auto& slot = m_ReceiveSlots[m_ReceiveHead];
            slot.Payload.resize(m_MaxDatagramSize);

            boost::asio::ip::udp::endpoint remoteEndpoint;
            boost::system::error_code errorCode;
            const auto receivedBytes = m_Socket.receive_from(
                boost::asio::buffer(slot.Payload.data(), slot.Payload.size()),
                remoteEndpoint,
                0,
                errorCode);

            if (errorCode == boost::asio::error::would_block
                || errorCode == boost::asio::error::try_again)
            {
                slot.Payload.clear();
                return {};
            }

            if (errorCode)
            {
                slot.Payload.clear();
                return std::unexpected(MakeSystemError(
                    Error::Api::ErrorCondition::EnvironmentError,
                    errorCode));
            }

            slot.Payload.resize(receivedBytes);
            slot.Peer = Api::Endpoint{
                .Address = remoteEndpoint.address().to_string(),
                .Port = remoteEndpoint.port()};

            m_ReceiveHead = (m_ReceiveHead + 1) % m_ReceiveSlots.size();
            ++m_ReceiveCount;
        }
    }

    Error::Api::Result<void> Socket::Send(const Api::Datagram& datagram)
    {
        if (!m_IsBound)
        {
            return std::unexpected(MakeApiError(
                Error::Api::ErrorCondition::ContractError,
                Api::ErrorCode::NotBound));
        }

        if (datagram.Payload.size() > m_MaxDatagramSize)
        {
            return std::unexpected(MakeApiError(
                Error::Api::ErrorCondition::ContractError,
                Api::ErrorCode::PayloadTooLarge));
        }

        const auto endpointResult = ToAsioEndpoint(datagram.Peer, false);
        if (!endpointResult)
        {
            return std::unexpected(endpointResult.error());
        }

        boost::system::error_code errorCode;
        m_Socket.send_to(
            boost::asio::buffer(datagram.Payload.data(), datagram.Payload.size()),
            *endpointResult,
            0,
            errorCode);

        if (errorCode)
        {
            return std::unexpected(MakeSystemError(
                Error::Api::ErrorCondition::EnvironmentError,
                errorCode));
        }

        return {};
    }

    Error::Api::Result<std::optional<Api::Datagram>> Socket::TryReceive()
    {
        if (!m_IsBound)
        {
            return std::unexpected(MakeApiError(
                Error::Api::ErrorCondition::ContractError,
                Api::ErrorCode::NotBound));
        }

        if (m_ReceiveCount == 0)
        {
            return std::optional<Api::Datagram>{std::nullopt};
        }

        auto& slot = m_ReceiveSlots[m_ReceiveTail];
        Api::Datagram datagram{
            .Peer = slot.Peer,
            .Payload = {}};
        datagram.Payload.swap(slot.Payload);
        slot.Payload.reserve(m_MaxDatagramSize);
        slot.Payload.clear();

        m_ReceiveTail = (m_ReceiveTail + 1) % m_ReceiveSlots.size();
        --m_ReceiveCount;

        return std::optional<Api::Datagram>{std::move(datagram)};
    }

    Error::Api::Error Socket::MakeApiError(
        const Error::Api::ErrorCondition condition,
        const Api::ErrorCode errorCode) noexcept
    {
        return Error::Api::MakeError(condition, make_error_code(errorCode));
    }

    Error::Api::Error Socket::MakeSystemError(
        const Error::Api::ErrorCondition condition,
        const boost::system::error_code& errorCode) noexcept
    {
        return Error::Api::MakeError(
            condition,
            std::error_code(errorCode.value(), std::system_category()));
    }

    Error::Api::Result<boost::asio::ip::udp::endpoint> Socket::ToAsioEndpoint(
        const Api::Endpoint& endpoint,
        const bool allowWildcardAddress)
    {
        if (endpoint.Address.empty())
        {
            if (!allowWildcardAddress)
            {
                return std::unexpected(MakeApiError(
                    Error::Api::ErrorCondition::ContractError,
                    Api::ErrorCode::InvalidEndpoint));
            }

            return boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), endpoint.Port);
        }

        boost::system::error_code errorCode;
        const auto address = boost::asio::ip::make_address(endpoint.Address, errorCode);
        if (errorCode)
        {
            return std::unexpected(MakeApiError(
                Error::Api::ErrorCondition::ContractError,
                Api::ErrorCode::InvalidEndpoint));
        }

        return boost::asio::ip::udp::endpoint(address, endpoint.Port);
    }
}
