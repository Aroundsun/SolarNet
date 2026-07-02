#pragma once

#include "solar_net/base/non_copyable.h"

#include <atomic>
#include <functional>
#include <string>
#include <string_view>
#include <thread>

namespace solar_net {

/**
 * @brief std::thread 的 RAII 封装，支持命名与自动 join。
 *
 * 线程安全：同一实例上的 Start/Join/Detach 需由调用方串行调用，不可并发。
 */
class Thread : NonCopyable {
public:
    using ThreadFunc = std::function<void()>;

    /** @brief 构造线程对象（尚未启动）。线程安全：否。 */
    explicit Thread(ThreadFunc func, std::string name = {});
    /** @brief 若已启动且未 join/detach，则自动 join。线程安全：否。 */
    ~Thread();

    /** @brief 启动线程并执行回调，每个实例只能调用一次。线程安全：否。 */
    void Start();

    /** @brief 阻塞直到线程结束。重复调用安全。线程安全：否。 */
    void Join();

    /** @brief 分离线程，之后不可再 Join。线程安全：否。 */
    void Detach();

    /** @brief 是否已调用 Start。线程安全：是（atomic 读）。 */
    [[nodiscard]] bool IsStarted() const noexcept;
    /** @brief 返回 std::thread::id。线程安全：是（只读）。 */
    [[nodiscard]] std::thread::id GetId() const noexcept;
    /** @brief 返回构造时指定的线程名。线程安全：是（只读）。 */
    [[nodiscard]] const std::string& GetName() const noexcept;

    /** @brief 设置当前线程名（Linux 下最多 15 字节）。线程安全：是（仅影响调用线程）。 */
    static void SetCurrentThreadName(std::string_view name);
    /** @brief 获取当前线程名。线程安全：是（仅读取调用线程）。 */
    static std::string GetCurrentThreadName();

private:
    ThreadFunc m_func;
    std::string m_name;
    std::thread m_thread;
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_joined{false};
    std::atomic<bool> m_detached{false};
};

} // namespace solar_net
