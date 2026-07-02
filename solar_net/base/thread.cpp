#include "solar_net/base/thread.h"

#include <array>
#include <cerrno>
#include <cstring>

#include <pthread.h>
#include <unistd.h>

namespace solar_net {

namespace {

constexpr size_t kMaxThreadNameLength = 15;

std::string TruncateThreadName(std::string_view name) {
    if (name.empty()) {
        return {};
    }
    if (name.size() > kMaxThreadNameLength) {
        return std::string{name.substr(0, kMaxThreadNameLength)};
    }
    return std::string{name};
}

} // namespace

Thread::Thread(ThreadFunc func, std::string name)
    : m_func(std::move(func)), m_name(std::move(name)) {}

Thread::~Thread() {
    if (m_started.load(std::memory_order_acquire) && !m_joined.load(std::memory_order_acquire) &&
        !m_detached.load(std::memory_order_acquire)) {
        Join();
    }
}

void Thread::Start() {
    bool expected = false;
    if (!m_started.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        return;
    }

    m_thread = std::thread([this] {
        const std::string thread_name = TruncateThreadName(m_name);
        if (!thread_name.empty()) {
            SetCurrentThreadName(thread_name);
        }

        if (m_func) {
            m_func();
        }
    });
}

void Thread::Join() {
    bool expected = false;
    if (m_joined.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        if (m_thread.joinable()) {
            m_thread.join();
        }
    }
}

void Thread::Detach() {
    bool expected = false;
    if (m_detached.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) {
        if (m_thread.joinable()) {
            m_thread.detach();
        }
    }
}

bool Thread::IsStarted() const noexcept {
    return m_started.load(std::memory_order_acquire);
}

std::thread::id Thread::GetId() const noexcept {
    return m_thread.get_id();
}

const std::string& Thread::GetName() const noexcept {
    return m_name;
}

void Thread::SetCurrentThreadName(std::string_view name) {
    const std::string truncated = TruncateThreadName(name);
    if (truncated.empty()) {
        return;
    }
#if defined(__linux__) && defined(__GLIBC__)
    pthread_setname_np(pthread_self(), truncated.c_str());
#endif
}

std::string Thread::GetCurrentThreadName() {
    std::array<char, kMaxThreadNameLength + 1> buffer{};
#if defined(__linux__) && defined(__GLIBC__)
    if (pthread_getname_np(pthread_self(), buffer.data(), buffer.size()) == 0) {
        return std::string{buffer.data()};
    }
#endif
    return {};
}

} // namespace solar_net
