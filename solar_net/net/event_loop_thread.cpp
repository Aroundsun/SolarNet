#include "solar_net/net/event_loop_thread.h"

#include "solar_net/net/event_loop.h"
#include "solar_net/base/logger.h"
#include "solar_net/base/thread.h"

#include <cassert>
#include <format>

namespace solar_net {

EventLoopThread::EventLoopThread(std::string name, InitCallback callback)
    : m_name(std::move(name)), m_init_callback(std::move(callback)) {}

EventLoopThread::~EventLoopThread() {
    Stop();
}

EventLoop* EventLoopThread::Start() {
    std::unique_lock lock(m_mutex);
    if (m_started) {
        return m_loop;
    }

    m_started = true;
    m_stopped = false;
    m_thread = std::make_unique<Thread>([this] { ThreadFunc(); }, m_name);
    m_thread->Start();

    m_cond.wait(lock, [this] { return m_loop != nullptr; });
    return m_loop;
}

void EventLoopThread::Stop() {
    EventLoop* loop = nullptr;
    std::unique_ptr<Thread> thread;

    {
        std::lock_guard lock(m_mutex);
        if (!m_started || m_stopped) {
            return;
        }
        m_stopped = true;
        loop = m_loop;
        thread = std::move(m_thread);
    }

    if (loop != nullptr) {
        if (loop->IsInLoopThread()) {
            loop->Quit();
        } else {
            loop->RunInLoop([loop] { loop->Quit(); });
        }
    }

    if (thread != nullptr) {
        thread->Join();
    }

    std::lock_guard lock(m_mutex);
    m_thread.reset();
    m_loop = nullptr;
}

EventLoop* EventLoopThread::GetLoop() const {
    std::lock_guard lock(m_mutex);
    return m_loop;
}

const std::string& EventLoopThread::GetName() const noexcept {
    return m_name;
}

void EventLoopThread::ThreadFunc() {
    EventLoop loop;

    {
        std::lock_guard lock(m_mutex);
        m_loop = &loop;

        if (m_init_callback) {
            try {
                m_init_callback(&loop);
            } catch (const std::exception& e) {
                LOG_ERROR(std::format("EventLoopThread init callback threw: {}", e.what()));
            } catch (...) {
                LOG_ERROR("EventLoopThread init callback threw unknown exception");
            }
        }
    }
    m_cond.notify_one();

    loop.Loop();

    std::lock_guard lock(m_mutex);
    m_loop = nullptr;
}

} // namespace solar_net
