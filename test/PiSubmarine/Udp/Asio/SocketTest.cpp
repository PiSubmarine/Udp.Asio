#include <gtest/gtest.h>

#include "PiSubmarine/Udp/Asio/Socket.h"

namespace PiSubmarine::Udp::Asio
{
    TEST(SocketTest, PollReturnsNoDatagramsWhenSocketIsIdle)
    {
        Socket socket(4);

        ASSERT_TRUE(socket.Bind(Api::Endpoint{"127.0.0.1", 0}).has_value());
        ASSERT_TRUE(socket.Poll().has_value());

        const auto receiveResult = socket.TryReceive();
        ASSERT_TRUE(receiveResult.has_value());
        EXPECT_FALSE(receiveResult->has_value());
    }

    TEST(SocketTest, SendsAndReceivesLoopbackDatagrams)
    {
        Socket receiver(4);
        Socket sender(4);

        ASSERT_TRUE(receiver.Bind(Api::Endpoint{"127.0.0.1", 31001}).has_value());
        ASSERT_TRUE(sender.Bind(Api::Endpoint{"127.0.0.1", 31002}).has_value());

        const Api::Datagram outgoing{
            .Peer = Api::Endpoint{"127.0.0.1", 31001},
            .Payload = {std::byte{0xCA}, std::byte{0xFE}}};

        ASSERT_TRUE(sender.Send(outgoing).has_value());
        ASSERT_TRUE(receiver.Poll().has_value());

        const auto receiveResult = receiver.TryReceive();
        ASSERT_TRUE(receiveResult.has_value());
        ASSERT_TRUE(receiveResult->has_value());
        EXPECT_EQ(receiveResult->value().Peer, Api::Endpoint{"127.0.0.1", 31002});
        EXPECT_EQ(receiveResult->value().Payload, outgoing.Payload);
    }
}
