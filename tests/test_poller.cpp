#include "solar_net/net/epoll_poller.h"

#include "solar_net/net/channel.h"
#include "solar_net/net/event_loop.h"

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <stdexcept>

#include <poll.h>
#include <unistd.h>

using namespace solar_net;

namespace {

class PipeGuard {
public:
    PipeGuard() {
        if (pipe(m_fds.data()) < 0) {
            throw std::runtime_error("pipe failed");
        }
    }

    ~PipeGuard() {
        Close(m_fds[0]);
        Close(m_fds[1]);
    }

    [[nodiscard]] int ReadFd() const noexcept { return m_fds[0]; }
    [[nodiscard]] int WriteFd() const noexcept { return m_fds[1]; }

    void WriteByte() const {
        const char c = 'x';
        (void)::write(m_fds[1], &c, sizeof(c));
    }

private:
    static void Close(int& fd) {
        if (fd >= 0) {
            ::close(fd);
            fd = -1;
        }
    }

    std::array<int, 2> m_fds{-1, -1};
};

EpollPoller& GetEpollPoller(EventLoop& loop) {
    return static_cast<EpollPoller&>(loop.GetPoller());
}

} // namespace

TEST(EpollPollerTest, PollReturnsReadableChannel) {
    EventLoop loop;
    EpollPoller& poller = GetEpollPoller(loop);
    PipeGuard pipe;

    std::atomic<bool> read_called{false};
    Channel channel(&loop, pipe.ReadFd());
    channel.SetReadCallback([&](Time) { read_called.store(true, std::memory_order_relaxed); });
    channel.EnableReading();

    pipe.WriteByte();

    Poller::ChannelList active;
    poller.Poll(0, &active);

    ASSERT_EQ(active.size(), 1);
    EXPECT_EQ(active[0], &channel);
    EXPECT_NE(channel.Revents() & POLLIN, 0);

    channel.HandleEvent(Time::Now());
    EXPECT_TRUE(read_called.load(std::memory_order_relaxed));

    channel.DisableAll();
    poller.RemoveChannel(&channel);
}

TEST(EpollPollerTest, PollReturnsWritableChannel) {
    EventLoop loop;
    EpollPoller& poller = GetEpollPoller(loop);
    PipeGuard pipe;

    std::atomic<bool> write_called{false};
    Channel channel(&loop, pipe.WriteFd());
    channel.SetWriteCallback([&] { write_called.store(true, std::memory_order_relaxed); });
    channel.EnableWriting();

    Poller::ChannelList active;
    poller.Poll(0, &active);

    ASSERT_EQ(active.size(), 1);
    EXPECT_EQ(active[0], &channel);
    EXPECT_NE(channel.Revents() & POLLOUT, 0);

    channel.HandleEvent(Time::Now());
    EXPECT_TRUE(write_called.load(std::memory_order_relaxed));

    channel.DisableAll();
    poller.RemoveChannel(&channel);
}

TEST(EpollPollerTest, HasChannelTracksRegistration) {
    EventLoop loop;
    EpollPoller& poller = GetEpollPoller(loop);
    PipeGuard pipe;

    Channel channel(&loop, pipe.ReadFd());
    EXPECT_FALSE(poller.HasChannel(&channel));

    channel.EnableReading();
    EXPECT_TRUE(poller.HasChannel(&channel));

    channel.DisableAll();
    EXPECT_TRUE(poller.HasChannel(&channel));

    channel.EnableReading();
    EXPECT_TRUE(poller.HasChannel(&channel));

    poller.RemoveChannel(&channel);
    EXPECT_FALSE(poller.HasChannel(&channel));
}

TEST(EpollPollerTest, PollWithoutEventsReturnsEmpty) {
    EventLoop loop;
    EpollPoller& poller = GetEpollPoller(loop);

    Poller::ChannelList active;
    poller.Poll(0, &active);

    EXPECT_TRUE(active.empty());
}
