#pragma once

#include "solar_net/base/non_copyable.h"

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace solar_net {

class EventLoop;
class Thread;

/**
 * @brief 在后台线程运行 EventLoop 的封装。
 *
 * 生命周期：Construct → Start() → Stop()/析构。
 * 线程安全：Start/Stop 由拥有者线程调用；GetLoop() 可在任意线程读指针。
 */
class EventLoopThread : NonCopyable {
public:
    using InitCallback = std::function<void(EventLoop*)>;

    explicit EventLoopThread(std::string name = {}, InitCallback callback = {});
    ~EventLoopThread();

    EventLoopThread(EventLoopThread&&) = delete;
    EventLoopThread& operator=(EventLoopThread&&) = delete;

    /** @brief 启动后台线程并阻塞直到 EventLoop 就绪；返回 loop 指针。 */
    EventLoop* Start();

    /** @brief Start 前或 Stop 后为 nullptr。 */
    [[nodiscard]] EventLoop* GetLoop() const;

    /** @brief 退出 loop 并 join 后台线程。 */
    void Stop();

    [[nodiscard]] const std::string& GetName() const noexcept;

private:
    void ThreadFunc();

    std::string m_name;
    InitCallback m_init_callback;

    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    EventLoop* m_loop{nullptr};

    std::unique_ptr<Thread> m_thread;
    bool m_started{false};
    bool m_stopped{false};
};

} // namespace solar_net
