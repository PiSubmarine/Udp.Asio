#pragma once

#include <cstddef>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>

#include "PiSubmarine/Udp/Api/ErrorCode.h"
#include "PiSubmarine/Udp/Api/ISocket.h"

namespace PiSubmarine::Udp::Asio
{
    class Socket final : public Api::ISocket
    {
    public:
        explicit Socket(
            std::size_t receiveQueueCapacity,
            std::size_t maxDatagramSize = 65507);

        [[nodiscard]] Error::Api::Result<void> Bind(const Api::Endpoint& localEndpoint) override;
        [[nodiscard]] Error::Api::Result<void> Poll() override;
        [[nodiscard]] Error::Api::Result<void> Send(const Api::Datagram& datagram) override;
        [[nodiscard]] Error::Api::Result<std::optional<Api::Datagram>> TryReceive() override;

    private:
        struct ReceiveSlot
        {
            Api::Endpoint Peer;
            std::vector<std::byte> Payload;
        };

        [[nodiscard]] static Error::Api::Error MakeApiError(
            Error::Api::ErrorCondition condition,
            Api::ErrorCode errorCode) noexcept;

        [[nodiscard]] static Error::Api::Error MakeSystemError(
            Error::Api::ErrorCondition condition,
            const boost::system::error_code& errorCode) noexcept;

        [[nodiscard]] static Error::Api::Result<boost::asio::ip::udp::endpoint> ToAsioEndpoint(
            const Api::Endpoint& endpoint,
            bool allowWildcardAddress);

        boost::asio::io_context m_IoContext;
        boost::asio::ip::udp::socket m_Socket;
        std::vector<ReceiveSlot> m_ReceiveSlots;
        std::size_t m_ReceiveHead = 0;
        std::size_t m_ReceiveTail = 0;
        std::size_t m_ReceiveCount = 0;
        std::size_t m_MaxDatagramSize = 0;
        bool m_IsBound = false;
    };
}
