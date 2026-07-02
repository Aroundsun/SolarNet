#include "solar_net/base/thread_pool.h"

#include "solar_net/base/logger.h"

#include <algorithm>
#include <format>

namespace solar_net {

namespace {

std::string BuildWorkerName(std::string_view base_name, size_t index) {
    if (base_name.empty()) {
        return std::format("worker-{}", index);
    }
    return std::format("{}-{}", base_name, index);
}

} // namespace

ThreadPool::ThreadPool(size_t thread_count, std::string name)
    : m_thread_count(thread_count == 0 ? 1 : thread_count), m_name(std::move(name)) {}

ThreadPool::~ThreadPool() {
    if (!m_stopped.load(std::memory_order_acquire)) {
        Stop();
        Wait();
    }
}

void ThreadPool::Start() {
    bool expected = false;
    if (!m_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    m_threads.reserve(m_thread_count);
    for (size_t i = 0; i < m_thread_count; ++i) {
        auto thread = std::make_unique<Thread>(
            [this, i] { RunWorker(i); }, BuildWorkerName(m_name, i));
        thread->Start();
        m_threads.push_back(std::move(thread));
    }
}

void ThreadPool::Stop() {
    bool expected = false;
    if (!m_stopping.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    {
        std::lock_guard lock(m_mutex);
    }
    m_cv.notify_all();
}

void ThreadPool::Wait() {
    for (auto& thread : m_threads) {
        if (thread) {
            thread->Join();
        }
    }
    m_stopped.store(true, std::memory_order_release);
}

bool ThreadPool::Submit(Task task) {
    if (!task) {
        return false;
    }

    {
        std::lock_guard lock(m_mutex);
        if (m_stopping.load(std::memory_order_acquire)) {
            return false;
        }
        m_tasks.push(std::move(task));
    }

    m_cv.notify_one();
    return true;
}

size_t ThreadPool::ThreadCount() const noexcept {
    return m_thread_count;
}

size_t ThreadPool::PendingTaskCount() const noexcept {
    std::lock_guard lock(m_mutex);
    return m_tasks.size();
}

bool ThreadPool::IsRunning() const noexcept {
    return m_started.load(std::memory_order_acquire) &&
           !m_stopping.load(std::memory_order_acquire);
}

void ThreadPool::RunWorker(size_t index) {
    (void)index;

    while (true) {
        Task task;
        {
            std::unique_lock lock(m_mutex);
            m_cv.wait(lock, [this] {
                return m_stopping.load(std::memory_order_acquire) || !m_tasks.empty();
            });

            if (m_tasks.empty()) {
                // Stopping was set and the queue is drained.
                return;
            }

            task = std::move(m_tasks.front());
            m_tasks.pop();
        }

        try {
            task();
        } catch (const std::exception& e) {
            LOG_ERROR(std::format("ThreadPool task threw exception: {}", e.what()));
        } catch (...) {
            LOG_ERROR("ThreadPool task threw unknown exception");
        }
    }
}

} // namespace solar_net
