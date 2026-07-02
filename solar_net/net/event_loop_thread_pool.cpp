#include "solar_net/net/event_loop_thread_pool.h"

#include "solar_net/base/logger.h"

#include <format>

namespace solar_net {

namespace {

constexpr size_t kMinThreadCount = 1;

} // namespace

EventLoopThreadPool::EventLoopThreadPool(size_t thread_count,
                                         std::string name,
                                         ThreadInitCallback callback)
    : m_thread_count(thread_count == 0 ? kMinThreadCount : thread_count)
    , m_name(std::move(name))
    , m_init_callback(std::move(callback)) {}

EventLoopThreadPool::~EventLoopThreadPool() {
    Stop();
}

void EventLoopThreadPool::Start() {
    bool expected = false;
    if (!m_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    m_threads.clear();
    m_loops.clear();
    m_next_index.store(0, std::memory_order_relaxed);

    m_threads.reserve(m_thread_count);
    m_loops.reserve(m_thread_count);

    for (size_t i = 0; i < m_thread_count; ++i) {
        auto thread = std::make_unique<EventLoopThread>(BuildThreadName(i), m_init_callback);
        EventLoop* loop = thread->Start();
        if (loop == nullptr) {
            LOG_ERROR(std::format("Failed to start EventLoopThread {}", i));
            continue;
        }
        m_loops.push_back(loop);
        m_threads.push_back(std::move(thread));
    }

    m_thread_count = m_threads.size();
}

void EventLoopThreadPool::Stop() {
    if (!m_started.exchange(false, std::memory_order_acq_rel)) {
        return;
    }

    for (auto& thread : m_threads) {
        if (thread != nullptr) {
            thread->Stop();
        }
    }

    m_threads.clear();
    m_loops.clear();
    m_thread_count = 0;
    m_next_index.store(0, std::memory_order_relaxed);
}

EventLoop* EventLoopThreadPool::GetNextLoop() {
    if (!m_started.load(std::memory_order_acquire) || m_loops.empty()) {
        return nullptr;
    }

    const size_t index = m_next_index.fetch_add(1, std::memory_order_relaxed) % m_loops.size();
    return m_loops[index];
}

EventLoop* EventLoopThreadPool::GetLoop(size_t index) const {
    if (!m_started.load(std::memory_order_acquire) || index >= m_loops.size()) {
        return nullptr;
    }
    return m_loops[index];
}

size_t EventLoopThreadPool::ThreadCount() const noexcept {
    return m_thread_count;
}

bool EventLoopThreadPool::IsRunning() const noexcept {
    return m_started.load(std::memory_order_acquire);
}

const std::string& EventLoopThreadPool::GetName() const noexcept {
    return m_name;
}

std::string EventLoopThreadPool::BuildThreadName(size_t index) const {
    if (m_name.empty()) {
        return std::format("io-{}", index);
    }
    return std::format("{}-{}", m_name, index);
}

} // namespace solar_net
