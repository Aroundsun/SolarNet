#include "solar_net/net/channel.h"

#include "solar_net/net/event_loop.h"
#include "solar_net/base/time.h"

#include <gtest/gtest.h>

#include <memory>
#include <poll.h>

using namespace solar_net;

class ChannelTest : public testing::Test {
protected:
    EventLoop loop;
};

TEST_F(ChannelTest, ConstructWithFdAndLoop) {
    Channel channel(&loop, 42);
    EXPECT_EQ(channel.Fd(), 42);
    EXPECT_EQ(channel.Events(), Channel::kNoneEvent);
    EXPECT_TRUE(channel.IsNoneEvent());
    EXPECT_EQ(channel.Index(), -1);
}

TEST_F(ChannelTest, EnableReading) {
    Channel channel(&loop, 1);
    EXPECT_FALSE(channel.IsReading());
    channel.EnableReading();
    EXPECT_TRUE(channel.IsReading());
    EXPECT_FALSE(channel.IsWriting());
    channel.DisableAll();
    EXPECT_TRUE(channel.IsNoneEvent());
}

TEST_F(ChannelTest, EnableWriting) {
    Channel channel(&loop, 1);
    EXPECT_FALSE(channel.IsWriting());
    channel.EnableWriting();
    EXPECT_TRUE(channel.IsWriting());
    EXPECT_FALSE(channel.IsReading());
    channel.DisableAll();
    EXPECT_TRUE(channel.IsNoneEvent());
}

TEST_F(ChannelTest, ReadingAndWritingCanCoexist) {
    Channel channel(&loop, 1);
    channel.EnableReading();
    channel.EnableWriting();
    EXPECT_TRUE(channel.IsReading());
    EXPECT_TRUE(channel.IsWriting());

    channel.DisableReading();
    EXPECT_FALSE(channel.IsReading());
    EXPECT_TRUE(channel.IsWriting());

    channel.DisableWriting();
    EXPECT_FALSE(channel.IsWriting());
    EXPECT_TRUE(channel.IsNoneEvent());
}

TEST_F(ChannelTest, HandleEventInvokesReadCallback) {
    Channel channel(&loop, 1);
    bool called = false;
    channel.SetReadCallback([&](Time t) {
        (void)t;
        called = true;
    });
    channel.SetRevents(POLLIN);
    channel.HandleEvent(Time::Now());
    EXPECT_TRUE(called);
}

TEST_F(ChannelTest, HandleEventInvokesWriteCallback) {
    Channel channel(&loop, 1);
    bool called = false;
    channel.SetWriteCallback([&] { called = true; });
    channel.SetRevents(POLLOUT);
    channel.HandleEvent(Time::Now());
    EXPECT_TRUE(called);
}

TEST_F(ChannelTest, HandleEventInvokesCloseCallbackOnHupWithoutIn) {
    Channel channel(&loop, 1);
    bool read_called = false;
    bool close_called = false;
    channel.SetReadCallback([&](Time t) {
        (void)t;
        read_called = true;
    });
    channel.SetCloseCallback([&] { close_called = true; });

    channel.SetRevents(POLLHUP);
    channel.HandleEvent(Time::Now());

    EXPECT_TRUE(close_called);
    EXPECT_FALSE(read_called);
}

TEST_F(ChannelTest, HandleEventInvokesReadBeforeCloseWhenHupWithIn) {
    Channel channel(&loop, 1);
    bool read_called = false;
    bool close_called = false;
    channel.SetReadCallback([&](Time t) {
        (void)t;
        read_called = true;
    });
    channel.SetCloseCallback([&] { close_called = true; });

    channel.SetRevents(POLLIN | POLLHUP);
    channel.HandleEvent(Time::Now());

    EXPECT_TRUE(read_called);
    EXPECT_FALSE(close_called);
}

TEST_F(ChannelTest, HandleEventInvokesErrorCallbackOnPollErr) {
    Channel channel(&loop, 1);
    bool error_called = false;
    channel.SetErrorCallback([&] { error_called = true; });
    channel.SetRevents(POLLERR);
    channel.HandleEvent(Time::Now());
    EXPECT_TRUE(error_called);
}

TEST_F(ChannelTest, TiePreventsCallbackAfterOwnerDestroyed) {
    Channel channel(&loop, 1);
    channel.SetRevents(POLLIN);

    {
        auto owner = std::make_shared<int>(42);
        channel.SetTie(owner);
        channel.SetReadCallback([&](Time t) {
            (void)t;
            FAIL() << "Should not be called after owner is destroyed";
        });
    }

    channel.HandleEvent(Time::Now());
    SUCCEED();
}

TEST_F(ChannelTest, TieAllowsCallbackWhenOwnerAlive) {
    Channel channel(&loop, 1);
    channel.SetRevents(POLLIN);

    auto owner = std::make_shared<int>(42);
    channel.SetTie(owner);

    bool read_called = false;
    channel.SetReadCallback([&](Time t) {
        (void)t;
        read_called = true;
    });

    channel.HandleEvent(Time::Now());
    EXPECT_TRUE(read_called);
}

TEST_F(ChannelTest, RemoveResetsIndex) {
    Channel channel(&loop, 1);
    channel.SetIndex(1);
    channel.DisableAll();
    channel.Remove();
    EXPECT_EQ(channel.Index(), -1);
}
