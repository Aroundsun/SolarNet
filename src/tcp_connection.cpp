#include "tcp_connection.h"
#include "channel.h"
#include "event_loop.h"
#include "socket.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <cstring>

namespace solar_net {

TcpConnection::TcpConnection(EventLoop* loop,
                               const std::string& name,
                               int fd,
                               const ::sockaddr_in& local_addr,
                               const ::sockaddr_in& peer_addr)
    : loop_(loop)
    , name_(name)
    , state_(State::kConnecting)
    , socket_(std::make_unique<Socket>(fd))
    , channel_(std::make_unique<Channel>(loop, fd))
    , local_addr_(local_addr)
    , peer_addr_(peer_addr) {
    // 设置通道回调
    channel_->set_read_callback(
        [this]() { handle_read(0); }
    );
    channel_->set_write_callback(
        [this]() { handle_write(); }
    );
    channel_->set_close_callback(
        [this]() { handle_close(); }
    );
    channel_->set_error_callback(
        [this]() { handle_error(); }
    );
}

TcpConnection::~TcpConnection() = default;

int TcpConnection::fd() const {
    return socket_ ? socket_->fd() : -1;
}

void TcpConnection::send(const void* data, std::size_t len) {
    if (state_ == State::kConnected) {
        if (loop_->is_in_loop_thread()) {
            send_in_loop(data, len);
        } else {
            // 复制数据并排队发送
            std::string msg(static_cast<const char*>(data), len);
            loop_->run_in_loop([this, msg]() {
                send_in_loop(msg);
            });
        }
    }
}

void TcpConnection::send(const std::string& message) {
    if (state_ == State::kConnected) {
        if (loop_->is_in_loop_thread()) {
            send_in_loop(message);
        } else {
            loop_->run_in_loop([this, message]() {
                send_in_loop(message);
            });
        }
    }
}

void TcpConnection::send(Buffer* buffer) {
    if (state_ == State::kConnected) {
        if (loop_->is_in_loop_thread()) {
            send_in_loop(buffer->data(), buffer->readable_bytes());
            buffer->retrieve_all();
        } else {
            std::string msg = buffer->retrieve_all_as_string();
            loop_->run_in_loop([this, msg]() {
                send_in_loop(msg);
            });
        }
    }
}

void TcpConnection::shutdown() {
    if (state_ == State::kConnected) {
        state_ = State::kDisconnecting;
        loop_->run_in_loop([this]() { shutdown_in_loop(); });
    }
}

void TcpConnection::force_close() {
    if (state_ == State::kConnected || state_ == State::kDisconnecting) {
        state_ = State::kDisconnecting;
        loop_->queue_in_loop([this]() { force_close_in_loop(); });
    }
}

void TcpConnection::set_tcp_no_delay(bool /*on*/) {
    if (socket_) {
        Socket::set_tcp_no_delay(socket_->fd());
    }
}

void TcpConnection::connection_established() {
    loop_->assert_in_loop_thread();
    assert(state_ == State::kConnecting);
    state_ = State::kConnected;

    // 绑定通道到这个连接的 shared_ptr
    channel_->tie(shared_from_this());
    channel_->enable_reading();

    if (connection_cb_) {
        connection_cb_(shared_from_this());
    }
}

void TcpConnection::connection_destroyed() {
    loop_->assert_in_loop_thread();
    if (state_ == State::kConnected) {
        state_ = State::kDisconnected;
        channel_->disable_all();
    }
    channel_->remove();

    if (close_cb_) {
        close_cb_(shared_from_this());
    }
}

void TcpConnection::handle_read(int64_t receive_time) {
    loop_->assert_in_loop_thread();

    ssize_t n = input_buffer_.read_from_fd(socket_->fd());
    if (n > 0) {
        if (message_cb_) {
            message_cb_(shared_from_this(), &input_buffer_, receive_time);
        }
    } else if (n == 0) {
        // 对端关闭连接
        handle_close();
    } else {

        handle_error();
    }
}

void TcpConnection::handle_write() {
    loop_->assert_in_loop_thread();

    if (channel_->is_none_event() || !(channel_->events() & EPOLLOUT)) {
        // 非关注事件 — 什么都不做
        return;
    }

    if (output_buffer_.readable_bytes() > 0) {
        ssize_t n = ::write(socket_->fd(),
                             output_buffer_.data(),
                             output_buffer_.readable_bytes());
        if (n > 0) {
            output_buffer_.retrieve(static_cast<std::size_t>(n));
            if (output_buffer_.readable_bytes() == 0) {
                // 所有数据已写入 — 关闭写兴趣
                channel_->disable_writing();

                if (write_complete_cb_) {
                    write_complete_cb_(shared_from_this());
                }

                if (state_ == State::kDisconnecting) {
                    shutdown_in_loop();
                }
            }
        } else {
            // 写入错误
            handle_error();
        }
    }
}

void TcpConnection::handle_close() {
    loop_->assert_in_loop_thread();

    assert(state_ == State::kConnected || state_ == State::kDisconnecting);
    state_ = State::kDisconnected;
    channel_->disable_all();

    if (connection_cb_) {
        connection_cb_(shared_from_this());
    }

    if (close_cb_) {
        close_cb_(shared_from_this());
    }
}

void TcpConnection::handle_error() {
    loop_->assert_in_loop_thread();
    // 在生产环境中，记录套接字错误
    handle_close();
}

void TcpConnection::send_in_loop(const void* data, std::size_t len) {
    loop_->assert_in_loop_thread();

    ssize_t nwrote = 0;
    std::size_t remaining = len;

    // 如果输出缓冲区为空且没有写兴趣，尝试直接写入
    if (!channel_->is_none_event() && !(channel_->events() & EPOLLOUT) &&
        output_buffer_.readable_bytes() == 0) {
        nwrote = ::write(socket_->fd(), data, len);
        if (nwrote >= 0) {
            remaining = len - static_cast<std::size_t>(nwrote);
            if (remaining == 0 && write_complete_cb_) {
                write_complete_cb_(shared_from_this());
            }
        } else {
            nwrote = 0;
            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                handle_error();
                return;
            }
            remaining = len;
        }
    }

    // 如果有剩余数据，追加到输出缓冲区
    if (remaining > 0) {
        std::size_t old_len = output_buffer_.readable_bytes();
        if (old_len + remaining >= high_water_mark_ &&
            old_len < high_water_mark_ &&
            high_water_mark_cb_) {
            high_water_mark_cb_(shared_from_this(), old_len + remaining);
        }

        output_buffer_.append(static_cast<const uint8_t*>(data) + nwrote, remaining);
        if (!channel_->is_none_event() && !(channel_->events() & EPOLLOUT)) {
            channel_->enable_writing();
        }
    }
}

void TcpConnection::send_in_loop(const std::string& message) {
    send_in_loop(message.data(), message.size());
}

void TcpConnection::shutdown_in_loop() {
    loop_->assert_in_loop_thread();
    if (output_buffer_.readable_bytes() == 0) {
        // 所有数据已发送 — 关闭写端
        ::shutdown(socket_->fd(), SHUT_WR);
    }
    // 如果输出缓冲区中还有数据，handle_write 将调用 shutdown
    // 在所有数据刷新后调用
}

void TcpConnection::force_close_in_loop() {
    loop_->assert_in_loop_thread();
    if (state_ == State::kConnected || state_ == State::kDisconnecting) {
        handle_close();
    }
}

} // namespace solar_net
