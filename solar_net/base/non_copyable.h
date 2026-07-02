#pragma once

namespace solar_net {

/**
 * @brief 禁止拷贝、允许移动的基类，用于表达“唯一所有权”语义。
 *
 * 派生类自动删除拷贝构造与拷贝赋值，保留默认移动语义。
 * 线程安全：无共享状态，本身不涉及并发。
 */
class NonCopyable {
public:
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;

    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;

protected:
    NonCopyable() = default;
    ~NonCopyable() = default;
};

} // namespace solar_net
