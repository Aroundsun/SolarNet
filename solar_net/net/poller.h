#pragma once

#include "solar_net/base/non_copyable.h"
#include "solar_net/base/time.h"

#include <map>
#include <memory>
#include <vector>

namespace solar_net {

class Channel;
class EventLoop;

/**
 * @brief IO 多路复用抽象基类。
 *
 * 维护 fd → Channel 映射，供 EventLoop 统一调度。
 * 线程安全：否，须在所属 EventLoop 线程访问。
 */
class Poller : NonCopyable {
public:
    using ChannelList = std::vector<Channel*>;

    explicit Poller(EventLoop* loop);
    virtual ~Poller();

    /** @brief 等待 IO 事件并填充就绪 Channel 列表。 */
    virtual Time Poll(int timeout_ms, ChannelList* active_channels) = 0;
    /** @brief 注册或更新 Channel 关注的事件。 */
    virtual void UpdateChannel(Channel* channel) = 0;
    /** @brief 从 Poller 移除 Channel。 */
    virtual void RemoveChannel(Channel* channel) = 0;

    [[nodiscard]] bool HasChannel(Channel* channel) const;

    /** @brief 创建平台默认 Poller（Linux 为 EpollPoller）。 */
    static std::unique_ptr<Poller> NewDefaultPoller(EventLoop* loop);

protected:
    using ChannelMap = std::map<int, Channel*>;
    ChannelMap m_channels;

    void AssertInLoopThread() const;

private:
    EventLoop* m_loop;
};

} // namespace solar_net
