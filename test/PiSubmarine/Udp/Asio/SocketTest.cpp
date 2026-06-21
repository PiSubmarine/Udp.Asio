#include <gtest/gtest.h>

#include "PiSubmarine/Udp/Asio/Socket.h"

namespace PiSubmarine::Udp::Asio
{
    TEST(SocketTest, TickBuffersNoDatagramsWhenSocketIsIdle)
    {
        Socket socket(4);

        ASSERT_TRUE(socket.Bind(Api::Endpoint{"127.0.0.1", 0}).has_value());
        socket.Tick(
            std::chrono::nanoseconds(std::chrono::milliseconds(100)),
            std::chrono::nanoseconds(std::chrono::milliseconds(10)));

        const auto receiveResult = socket.TryReceive();
        ASSERT_TRUE(receiveResult.has_value());
        EXPECT_FALSE(receiveResult->has_value());
    }

    TEST(SocketTest, SendsAndReceivesLoopbackDatagrams)
    {
        Socket receiver(4);
        Socket sender(4);
        const Api::Endpoint expectedPeer{"127.0.0.1", 31002};

        ASSERT_TRUE(receiver.Bind(Api::Endpoint{"127.0.0.1", 31001}).has_value());
        ASSERT_TRUE(sender.Bind(Api::Endpoint{"127.0.0.1", 31002}).has_value());

        const Api::Datagram outgoing{
            .Peer = Api::Endpoint{"127.0.0.1", 31001},
            .Payload = {std::byte{0xCA}, std::byte{0xFE}}};

        ASSERT_TRUE(sender.Send(outgoing).has_value());
        receiver.Tick(
            std::chrono::nanoseconds(std::chrono::milliseconds(100)),
            std::chrono::nanoseconds(std::chrono::milliseconds(10)));

        const auto receiveResult = receiver.TryReceive();
        ASSERT_TRUE(receiveResult.has_value());
        ASSERT_TRUE(receiveResult->has_value());
        EXPECT_EQ(receiveResult->value().Peer, expectedPeer);
        EXPECT_EQ(receiveResult->value().Payload, outgoing.Payload);
    }

    TEST(SocketTest, IgnoresUnreachablePeerReceiveReset)
    {
        Socket socket(4);

        ASSERT_TRUE(socket.Bind(Api::Endpoint{"127.0.0.1", 31021}).has_value());
        ASSERT_TRUE(socket.Send(Api::Datagram{
            .Peer = Api::Endpoint{"127.0.0.1", 31022},
            .Payload = {std::byte{0x01}}})
                        .has_value());

        EXPECT_NO_THROW(socket.Tick(
            std::chrono::nanoseconds(std::chrono::milliseconds(100)),
            std::chrono::nanoseconds(std::chrono::milliseconds(10))));

        const auto receiveResult = socket.TryReceive();
        ASSERT_TRUE(receiveResult.has_value());
        EXPECT_FALSE(receiveResult->has_value());
    }

    TEST(SocketTest, DropsOldestBufferedDatagramWhenReceiveQueueIsFull)
    {
        Socket receiver(1);
        Socket sender(4);

        ASSERT_TRUE(receiver.Bind(Api::Endpoint{"127.0.0.1", 31011}).has_value());
        ASSERT_TRUE(sender.Bind(Api::Endpoint{"127.0.0.1", 31012}).has_value());

        const Api::Datagram first{
            .Peer = Api::Endpoint{"127.0.0.1", 31011},
            .Payload = {std::byte{0x01}}};
        const Api::Datagram second{
            .Peer = Api::Endpoint{"127.0.0.1", 31011},
            .Payload = {std::byte{0x02}}};

        ASSERT_TRUE(sender.Send(first).has_value());
        ASSERT_TRUE(sender.Send(second).has_value());

        receiver.Tick(
            std::chrono::nanoseconds(std::chrono::milliseconds(100)),
            std::chrono::nanoseconds(std::chrono::milliseconds(10)));

        const auto receiveResult = receiver.TryReceive();
        ASSERT_TRUE(receiveResult.has_value());
        ASSERT_TRUE(receiveResult->has_value());
        EXPECT_EQ(receiveResult->value().Payload, second.Payload);

        const auto nextReceiveResult = receiver.TryReceive();
        ASSERT_TRUE(nextReceiveResult.has_value());
        EXPECT_FALSE(nextReceiveResult->has_value());
    }
}
