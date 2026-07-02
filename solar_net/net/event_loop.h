#pragma once

#include "solar_net/base/non_copyable.h"
#include "solar_net/base/time.h"

#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace solar_net {

class Channel;
class Poller;

/**
 * @brief Reactor 事件循环，驱动 Poller 与跨线程任务调度。
 *
 * 每个 EventLoop 绑定一个线程；Channel/Poller 操作须在 loop 线程执行。
 * 线程安全：QueueInLoop / Quit 可从其他线程调用。
 */
class EventLoop : NonCopyable {
public:
    using Functor = std::function<void()>;

    EventLoop();
    ~EventLoop();

    /** @brief 运行事件循环直至 Quit。须在构造线程调用。 */
    void Loop();
    /** @brief 请求退出循环。线程安全。 */
    void Quit();

    /** @brief 是否在 EventLoop 所属线程。 */
    [[nodiscard]] bool IsInLoopThread() const noexcept;
    /** @brief 断言当前为 loop 线程。 */
    void AssertInLoopThread() const;

    /** @brief 在 loop 线程执行；若已在 loop 线程则立即执行。 */
    void RunInLoop(Functor cb);
    /** @brief 将任务排队到 loop 线程执行。线程安全。 */
    void QueueInLoop(Functor cb);

    /** @brief 注册或更新 Channel。须在 loop 线程调用。 */
    void UpdateChannel(Channel* channel);
    /** @brief 从 Poller 移除 Channel。须在 loop 线程调用。 */
    void RemoveChannel(Channel* channel);

    /** @brief 返回底层 Poller（测试/调试用途）。 */
    [[nodiscard]] Poller& GetPoller() noexcept { return *m_poller; }

private:
    void Wakeup();
    void HandleRead();
    void DoPendingFunctors();

    std::atomic<bool> m_looping{false};
    std::atomic<bool> m_quit{false};
    std::atomic<bool> m_event_handling{false};
    std::atomic<bool> m_calling_pending_functors{false};

    const std::thread::id m_thread_id;
    std::unique_ptr<Poller> m_poller;

    int m_wakeup_fd{-1};
    std::unique_ptr<Channel> m_wakeup_channel;

    std::vector<Functor> m_pending_functors;
    std::mutex m_mutex;
};

} // namespace solar_net
