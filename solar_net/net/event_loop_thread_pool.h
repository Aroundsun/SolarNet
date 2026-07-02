#pragma once

#include "solar_net/net/event_loop_thread.h"

#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace solar_net {

/**
 * @brief 多 Reactor 线程池，每个线程运行独立 EventLoop。
 *
 * GetNextLoop() 以 round-robin 分配 loop。
 * 线程安全：Start/Stop 由拥有者线程调用；GetNextLoop 可并发。
 */
class EventLoopThreadPool : NonCopyable {
public:
    using ThreadInitCallback = EventLoopThread::InitCallback;

    explicit EventLoopThreadPool(size_t thread_count = std::thread::hardware_concurrency(),
                                 std::string name = {},
                                 ThreadInitCallback callback = {});
    ~EventLoopThreadPool();

    EventLoopThreadPool(EventLoopThreadPool&&) = delete;
    EventLoopThreadPool& operator=(EventLoopThreadPool&&) = delete;

    /** @brief 启动全部 IO 线程并阻塞直到各 EventLoop 就绪。 */
    void Start();

    /** @brief 停止全部 loop 并 join 线程。 */
    void Stop();

    /** @brief round-robin 返回下一个 loop；未 Start 时返回 nullptr。 */
    EventLoop* GetNextLoop();

    /** @brief 按索引返回 loop；越界或未 Start 时返回 nullptr。 */
    [[nodiscard]] EventLoop* GetLoop(size_t index) const;

    [[nodiscard]] size_t ThreadCount() const noexcept;
    [[nodiscard]] bool IsRunning() const noexcept;
    [[nodiscard]] const std::string& GetName() const noexcept;

private:
    [[nodiscard]] std::string BuildThreadName(size_t index) const;

    size_t m_thread_count;
    std::string m_name;
    ThreadInitCallback m_init_callback;

    std::vector<std::unique_ptr<EventLoopThread>> m_threads;
    std::vector<EventLoop*> m_loops;

    std::atomic<size_t> m_next_index{0};
    std::atomic<bool> m_started{false};
};

} // namespace solar_net
