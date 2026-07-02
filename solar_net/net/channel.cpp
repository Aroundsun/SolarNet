#include "solar_net/net/channel.h"

#include "solar_net/net/event_loop.h"
#include "solar_net/base/logger.h"

#include <cassert>
#include <cerrno>
#include <format>
#include <sstream>

#include <poll.h>

namespace solar_net {

const int Channel::kNoneEvent = 0;
const int Channel::kReadEvent = POLLIN | POLLPRI;
const int Channel::kWriteEvent = POLLOUT;

namespace {

std::string EventsToStringImpl(int events) {
    std::ostringstream oss;
    oss << "[";
    if (events == 0) {
        oss << "none";
    } else {
        bool first = true;
        auto append = [&oss, &first](const char* name) {
            if (!first) {
                oss << " ";
            }
            first = false;
            oss << name;
        };
        if (events & POLLIN) {
            append("IN");
        }
        if (events & POLLPRI) {
            append("PRI");
        }
        if (events & POLLOUT) {
            append("OUT");
        }
        if (events & POLLHUP) {
            append("HUP");
        }
        if (events & POLLRDHUP) {
            append("RDHUP");
        }
        if (events & POLLERR) {
            append("ERR");
        }
        if (events & POLLNVAL) {
            append("NVAL");
        }
    }
    oss << "]";
    return oss.str();
}

} // namespace

Channel::Channel(EventLoop* loop, int fd)
    : m_loop(loop), m_fd(fd), m_events(kNoneEvent), m_revents(kNoneEvent), m_index(-1) {
    assert(loop != nullptr);
    assert(fd >= 0);
}

Channel::~Channel() {
    assert(IsNoneEvent());
    assert(m_index == -1);
}

void Channel::SetTie(const std::shared_ptr<void>& obj) {
    m_tie = obj;
    m_tied = true;
}

void Channel::Remove() {
    assert(IsNoneEvent());
    if (m_loop != nullptr) {
        m_loop->RemoveChannel(this);
    }
}

void Channel::HandleEvent(Time receive_time) {
    if (m_tied) {
        const std::shared_ptr<void> guard = m_tie.lock();
        if (!guard) {
            return;
        }
        HandleEventWithGuard(receive_time);
    } else {
        HandleEventWithGuard(receive_time);
    }
}

void Channel::HandleEventWithGuard(Time receive_time) {
    LOG_DEBUG(std::format("Channel {} HandleEvent {}", m_fd, EventsToStringImpl(m_revents)));

    if ((m_revents & POLLHUP) != 0 && (m_revents & POLLIN) == 0) {
        if (m_close_callback) {
            m_close_callback();
        }
    }

    if (m_revents & POLLNVAL) {
        LOG_WARN(std::format("Channel {} POLLNVAL", m_fd));
    }

    if ((m_revents & POLLERR) != 0 || (m_revents & POLLNVAL) != 0) {
        if (m_error_callback) {
            m_error_callback();
        }
    }

    if ((m_revents & POLLIN) != 0 || (m_revents & POLLPRI) != 0 ||
        (m_revents & POLLRDHUP) != 0) {
        if (m_read_callback) {
            m_read_callback(receive_time);
        }
    }

    if ((m_revents & POLLOUT) != 0) {
        if (m_write_callback) {
            m_write_callback();
        }
    }
}

void Channel::Update() {
    if (m_loop != nullptr) {
        m_loop->UpdateChannel(this);
    }
}

std::string Channel::EventsToString(int events) const {
    return EventsToStringImpl(events);
}

} // namespace solar_net
