#pragma once

#include "solar_net/base/non_copyable.h"
#include "solar_net/base/thread.h"

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

namespace solar_net {

/**
 * @brief 固定大小线程池，带阻塞任务队列。
 *
 * 线程安全：Submit / Stop / Wait / 查询接口可并发调用（内部 mutex 保护）。
 */
class ThreadPool : NonCopyable {
public:
    using Task = std::function<void()>;

    /**
     * @brief 构造线程池（尚未启动）。
     * @param thread_count 工作线程数，0 时按 hardware_concurrency，检测失败则为 1。
     * @param name 工作线程名前缀。线程安全：否（构造阶段）。
     */
    explicit ThreadPool(size_t thread_count = std::thread::hardware_concurrency(),
                        std::string name = {});
    /** @brief 若仍在运行则 Stop 并 Wait。线程安全：否（析构阶段）。 */
    ~ThreadPool();

    /** @brief 启动全部工作线程，重复调用为 no-op。线程安全：是。 */
    void Start();

    /** @brief 通知 worker 在队列排空后退出，非阻塞。线程安全：是。 */
    void Stop();

    /** @brief 阻塞直到所有 worker 结束。线程安全：是。 */
    void Wait();

    /** @brief 提交任务；Stop 后返回 false。线程安全：是。 */
    bool Submit(Task task);

    /** @brief 返回配置的线程数。线程安全：是。 */
    [[nodiscard]] size_t ThreadCount() const noexcept;
    /** @brief 返回队列中待执行任务数。线程安全：是。 */
    [[nodiscard]] size_t PendingTaskCount() const noexcept;
    /** @brief 是否已 Start 且尚未 Stop。线程安全：是。 */
    [[nodiscard]] bool IsRunning() const noexcept;

private:
    void RunWorker(size_t index);

    size_t m_thread_count;
    std::string m_name;
    std::vector<std::unique_ptr<Thread>> m_threads;

    std::queue<Task> m_tasks;
    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    std::atomic<bool> m_started{false};
    std::atomic<bool> m_stopping{false};
    std::atomic<bool> m_stopped{false};
};

} // namespace solar_net
