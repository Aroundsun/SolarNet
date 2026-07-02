#include "solar_net/net/transport/tcp_connection.h"

#include "solar_net/net/channel.h"
#include "solar_net/net/event_loop.h"
#include "solar_net/base/logger.h"

#include <cassert>
#include <cerrno>
#include <cstring>
#include <format>
#include <unistd.h>

namespace solar_net {

TcpConnection::TcpConnection(EventLoop* loop,
                             std::string name,
                             int sockfd,
                             const InetAddress& local_addr,
                             const InetAddress& peer_addr)
    : m_loop(loop)
    , m_name(std::move(name))
    , m_socket(sockfd)
    , m_channel(std::make_unique<Channel>(loop, sockfd))
    , m_local_addr(local_addr)
    , m_peer_addr(peer_addr) {
    assert(loop != nullptr);
    assert(sockfd >= 0);

    m_channel->SetReadCallback([this](Time receive_time) { HandleRead(receive_time); });
    m_channel->SetWriteCallback([this] { HandleWrite(); });
    m_channel->SetCloseCallback([this] { HandleClose(); });
    m_channel->SetErrorCallback([this] { HandleError(); });

    m_socket.SetKeepAlive(true);
    m_socket.SetTcpNoDelay(true);
}

TcpConnection::~TcpConnection() {
    LOG_DEBUG(std::format("TcpConnection::~TcpConnection [{}] state={}", m_name, StateToString()));
    assert(m_state.load(std::memory_order_relaxed) == State::kDisconnected);
    if (m_channel != nullptr) {
        assert(m_channel->IsNoneEvent());
        assert(m_channel->Index() == -1);
    }
}

void TcpConnection::SetContext(std::any context) {
    if (m_loop->IsInLoopThread()) {
        m_context = std::move(context);
    } else {
        std::lock_guard lock(m_context_mutex);
        m_context = std::move(context);
    }
}

const std::any& TcpConnection::GetContext() const {
    if (m_loop->IsInLoopThread()) {
        return m_context;
    }
    std::lock_guard lock(m_context_mutex);
    return m_context;
}

void TcpConnection::SetHighWaterMarkCallback(HighWaterMarkCallback cb, size_t high_water_mark) {
    m_high_water_mark = high_water_mark;
    m_high_water_mark_callback = std::move(cb);
}

void TcpConnection::Send(std::string_view message) {
    if (message.empty()) {
        return;
    }
    if (m_loop->IsInLoopThread()) {
        SendInLoop(message);
    } else {
        m_loop->RunInLoop([this, guard = shared_from_this(), message = std::string(message)] {
            SendInLoop(message);
        });
    }
}

void TcpConnection::Send(const void* data, size_t len) {
    Send(std::string_view(static_cast<const char*>(data), len));
}

void TcpConnection::Send(Buffer* buffer) {
    if (buffer == nullptr || buffer->ReadableBytes() == 0) {
        return;
    }
    if (m_loop->IsInLoopThread()) {
        SendInLoop(buffer->ToStringView());
        buffer->RetrieveAll();
    } else {
        m_loop->RunInLoop([this, guard = shared_from_this(), message = buffer->RetrieveAllAsString()] {
            SendInLoop(message);
        });
    }
}

void TcpConnection::SendInLoop(std::string_view message) {
    m_loop->AssertInLoopThread();
    if (m_state.load(std::memory_order_relaxed) != State::kConnected) {
        LOG_WARN(std::format("TcpConnection::SendInLoop [{}] disconnected, drop {} bytes",
                             m_name, message.size()));
        return;
    }

    bool fault = false;
    ssize_t nwrote = 0;
    size_t remaining = message.size();

    if (!m_channel->IsWriting() && m_output_buffer.ReadableBytes() == 0) {
        nwrote = ::write(m_socket.Fd(), message.data(), message.size());
        if (nwrote >= 0) {
            remaining = message.size() - static_cast<size_t>(nwrote);
            if (remaining == 0 && m_write_complete_callback) {
                m_loop->QueueInLoop([guard = shared_from_this(), cb = m_write_complete_callback] {
                    cb(guard);
                });
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                LOG_ERROR(std::format("TcpConnection::SendInLoop [{}] write failed: {}",
                                      m_name, std::strerror(errno)));
                if (errno == EPIPE || errno == ECONNRESET) {
                    fault = true;
                }
            }
        }
    }

    if (!fault && remaining > 0) {
        const size_t old_len = m_output_buffer.ReadableBytes();
        const size_t new_len = old_len + remaining;
        m_output_buffer.Append(message.substr(static_cast<size_t>(nwrote)));
        if (old_len < m_high_water_mark && new_len >= m_high_water_mark && m_high_water_mark_callback) {
            m_loop->QueueInLoop([guard = shared_from_this(), cb = m_high_water_mark_callback, new_len] {
                cb(guard, new_len);
            });
        }
        if (!m_channel->IsWriting()) {
            m_channel->EnableWriting();
        }
    }
}

void TcpConnection::Shutdown() {
    if (m_state.load(std::memory_order_relaxed) == State::kConnected ||
        m_state.load(std::memory_order_relaxed) == State::kDisconnecting) {
        m_loop->RunInLoop([this, guard = shared_from_this()] { ShutdownInLoop(); });
    }
}

void TcpConnection::ShutdownInLoop() {
    m_loop->AssertInLoopThread();
    if (m_state.load(std::memory_order_relaxed) == State::kConnected) {
        SetState(State::kDisconnecting);
    }
    if (!m_channel->IsWriting()) {
        m_socket.ShutdownWrite();
    }
}

void TcpConnection::ForceClose() {
    if (m_state.load(std::memory_order_relaxed) == State::kConnected ||
        m_state.load(std::memory_order_relaxed) == State::kDisconnecting) {
        m_loop->RunInLoop([this, guard = shared_from_this()] { ForceCloseInLoop(); });
    }
}

void TcpConnection::ForceCloseInLoop() {
    m_loop->AssertInLoopThread();
    if (m_state.load(std::memory_order_relaxed) == State::kConnected ||
        m_state.load(std::memory_order_relaxed) == State::kDisconnecting) {
        HandleClose();
    }
}

void TcpConnection::ConnectEstablished() {
    m_loop->AssertInLoopThread();
    assert(m_state.load(std::memory_order_relaxed) == State::kConnecting);
    SetState(State::kConnected);
    m_channel->SetTie(shared_from_this());
    m_channel->EnableReading();

    if (m_connection_callback) {
        m_connection_callback(shared_from_this());
    }
}

void TcpConnection::ConnectDestroyed() {
    m_loop->AssertInLoopThread();
    if (m_state.load(std::memory_order_relaxed) == State::kConnected) {
        SetState(State::kDisconnected);
        m_channel->DisableAll();
        if (m_connection_callback) {
            m_connection_callback(shared_from_this());
        }
    }
    if (m_channel != nullptr && m_channel->Index() != -1) {
        m_channel->Remove();
    }
}

void TcpConnection::HandleRead(Time receive_time) {
    m_loop->AssertInLoopThread();
    int saved_errno = 0;
    const ssize_t n = m_input_buffer.ReadFd(m_socket.Fd(), &saved_errno);
    if (n > 0) {
        if (m_message_callback) {
            m_message_callback(shared_from_this(), &m_input_buffer, receive_time);
        }
    } else if (n == 0) {
        HandleClose();
    } else {
        LOG_ERROR(std::format("TcpConnection::HandleRead [{}] read failed: {}",
                              m_name, std::strerror(saved_errno)));
        if (saved_errno != EAGAIN && saved_errno != EWOULDBLOCK) {
            HandleError();
        }
    }
}

void TcpConnection::HandleWrite() {
    m_loop->AssertInLoopThread();
    if (!m_channel->IsWriting()) {
        LOG_WARN(std::format("TcpConnection::HandleWrite [{}] writing disabled", m_name));
        return;
    }

    int saved_errno = 0;
    const ssize_t n = m_output_buffer.WriteFd(m_socket.Fd(), &saved_errno);
    if (n > 0) {
        m_output_buffer.Retrieve(static_cast<size_t>(n));
        if (m_output_buffer.ReadableBytes() == 0) {
            m_channel->DisableWriting();
            if (m_write_complete_callback) {
                m_loop->QueueInLoop([guard = shared_from_this(), cb = m_write_complete_callback] {
                    cb(guard);
                });
            }
            if (m_state.load(std::memory_order_relaxed) == State::kDisconnecting) {
                ShutdownInLoop();
            }
        }
    } else {
        LOG_ERROR(std::format("TcpConnection::HandleWrite [{}] write failed: {}",
                              m_name, std::strerror(saved_errno)));
    }
}

void TcpConnection::HandleClose() {
    m_loop->AssertInLoopThread();
    LOG_DEBUG(std::format("TcpConnection::HandleClose [{}] state={}", m_name, StateToString()));
    assert(m_state.load(std::memory_order_relaxed) == State::kConnected ||
           m_state.load(std::memory_order_relaxed) == State::kDisconnecting);
    SetState(State::kDisconnected);
    m_channel->DisableAll();

    const TcpConnectionPtr guard(shared_from_this());
    if (m_close_callback) {
        m_close_callback(guard);
    }
    if (m_connection_callback) {
        m_connection_callback(guard);
    }
}

void TcpConnection::HandleError() {
    m_loop->AssertInLoopThread();
    int err = 0;
    socklen_t err_len = sizeof(err);
    if (::getsockopt(m_socket.Fd(), SOL_SOCKET, SO_ERROR, &err, &err_len) == 0) {
        LOG_ERROR(std::format("TcpConnection::HandleError [{}] SO_ERROR={}", m_name, err));
    } else {
        LOG_ERROR(std::format("TcpConnection::HandleError [{}] getsockopt failed: {}",
                              m_name, std::strerror(errno)));
    }
}

std::string TcpConnection::StateToString() const {
    switch (m_state.load(std::memory_order_relaxed)) {
    case State::kConnecting:
        return "connecting";
    case State::kConnected:
        return "connected";
    case State::kDisconnecting:
        return "disconnecting";
    case State::kDisconnected:
        return "disconnected";
    default:
        return "unknown";
    }
}

} // namespace solar_net
