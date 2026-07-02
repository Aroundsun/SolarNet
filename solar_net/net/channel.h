#pragma once

#include "solar_net/base/non_copyable.h"
#include "solar_net/base/time.h"

#include <functional>
#include <memory>
#include <string>

namespace solar_net {

class EventLoop;

/**
 * @brief 可选择的 I/O 事件通道，连接 Poller/EventLoop 与上层 IO 对象。
 *
 * 不拥有 fd，仅负责事件注册与回调分发。
 * 线程安全：否，仅能在所属 EventLoop 线程访问。
 */
class Channel : NonCopyable {
public:
    using EventCallback = std::function<void()>;
    using ReadEventCallback = std::function<void(Time)>;

    /** @brief 绑定 EventLoop 与文件描述符。线程安全：否。 */
    Channel(EventLoop* loop, int fd);
    /** @brief 析构前须 DisableAll 且已从 Poller 移除（index == -1）。线程安全：否。 */
    ~Channel();

    /** @brief 返回绑定的 fd。线程安全：否。 */
    [[nodiscard]] int Fd() const noexcept { return m_fd; }
    /** @brief 返回关注的事件掩码。线程安全：否。 */
    [[nodiscard]] int Events() const noexcept { return m_events; }

    /** @brief 设置 Poller 返回的就绪事件，HandleEvent 前调用。线程安全：否。 */
    void SetRevents(int revents) noexcept { m_revents = revents; }
    /** @brief 返回就绪事件掩码。线程安全：否。 */
    [[nodiscard]] int Revents() const noexcept { return m_revents; }

    /** @brief 是否未关注任何事件。线程安全：否。 */
    [[nodiscard]] bool IsNoneEvent() const noexcept { return m_events == kNoneEvent; }
    /** @brief 是否关注可读。线程安全：否。 */
    [[nodiscard]] bool IsReading() const noexcept { return (m_events & kReadEvent) != 0; }
    /** @brief 是否关注可写。线程安全：否。 */
    [[nodiscard]] bool IsWriting() const noexcept { return (m_events & kWriteEvent) != 0; }

    /** @brief 启用读事件并通知 EventLoop 更新。线程安全：否。 */
    void EnableReading() { m_events |= kReadEvent; Update(); }
    /** @brief 禁用读事件。线程安全：否。 */
    void DisableReading() { m_events &= ~kReadEvent; Update(); }
    /** @brief 启用写事件。线程安全：否。 */
    void EnableWriting() { m_events |= kWriteEvent; Update(); }
    /** @brief 禁用写事件。线程安全：否。 */
    void DisableWriting() { m_events &= ~kWriteEvent; Update(); }
    /** @brief 禁用全部事件。线程安全：否。 */
    void DisableAll() { m_events = kNoneEvent; Update(); }

    /** @brief 从 EventLoop 移除（须已无关注事件）。线程安全：否。 */
    void Remove();

    /** @brief 根据 revents 分发读/写/关闭/错误回调。线程安全：否。 */
    void HandleEvent(Time receive_time);

    /** @brief 设置读回调（参数为事件到达时间）。线程安全：否。 */
    void SetReadCallback(ReadEventCallback cb) { m_read_callback = std::move(cb); }
    /** @brief 设置写回调。线程安全：否。 */
    void SetWriteCallback(EventCallback cb) { m_write_callback = std::move(cb); }
    /** @brief 设置关闭回调。线程安全：否。 */
    void SetCloseCallback(EventCallback cb) { m_close_callback = std::move(cb); }
    /** @brief 设置错误回调。线程安全：否。 */
    void SetErrorCallback(EventCallback cb) { m_error_callback = std::move(cb); }

    /**
     * @brief 将 Channel 生命周期绑定到外部对象。
     *
     * 若对象已销毁则 HandleEvent 不再调用回调，避免悬空引用。
     * 线程安全：否。
     */
    void SetTie(const std::shared_ptr<void>& obj);

    /** @brief Poller 内部索引（-1 新建，1 已添加，2 待删除）。线程安全：否。 */
    [[nodiscard]] int Index() const noexcept { return m_index; }
    void SetIndex(int index) noexcept { m_index = index; }

    static const int kNoneEvent;  ///< 无事件
    static const int kReadEvent;  ///< POLLIN | POLLPRI
    static const int kWriteEvent; ///< POLLOUT

private:
    void Update();
    void HandleEventWithGuard(Time receive_time);

    [[nodiscard]] std::string EventsToString(int events) const;

    EventLoop* m_loop;
    int m_fd;
    int m_events;
    int m_revents;
    int m_index;

    bool m_tied{false};
    std::weak_ptr<void> m_tie;

    ReadEventCallback m_read_callback;
    EventCallback m_write_callback;
    EventCallback m_close_callback;
    EventCallback m_error_callback;
};

} // namespace solar_net
