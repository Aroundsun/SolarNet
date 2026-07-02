#include "solar_net/net/transport/acceptor.h"
#include "solar_net/net/event_loop.h"
#include "solar_net/net/event_loop_thread.h"
#include "solar_net/net/transport/inet_address.h"

#include <atomic>
#include <chrono>
#include <format>
#include <future>
#include <iostream>
#include <memory>
#include <thread>

#include <unistd.h>

int main() {
    solar_net::EventLoopThread loop_thread("acceptor-demo");
    solar_net::EventLoop* loop = loop_thread.Start();
    if (loop == nullptr) {
        std::cerr << "failed to start loop thread\n";
        return 1;
    }

    std::atomic<int> count{0};
    std::shared_ptr<solar_net::Acceptor> acceptor;

    loop->RunInLoop([&] {
        acceptor = std::make_shared<solar_net::Acceptor>(loop, solar_net::InetAddress(8080));
        acceptor->SetNewConnectionCallback([&](int sockfd, const solar_net::InetAddress& peer) {
            std::cout << std::format("accepted fd={} from {} (total={})\n",
                                     sockfd,
                                     peer.ToIpPort(),
                                     count.fetch_add(1, std::memory_order_relaxed) + 1);
            ::close(sockfd);
        });
        if (!acceptor->Listen()) {
            std::cerr << "failed to listen\n";
            loop->Quit();
        }
    });

    std::cout << "acceptor listening on 0.0.0.0:8080, connect with:\n";
    std::cout << "  nc 127.0.0.1 8080\n";
    std::cout << "press Ctrl-C to stop\n";

    std::this_thread::sleep_for(std::chrono::seconds(10));

    std::promise<void> cleanup_done;
    loop->RunInLoop([&] {
        acceptor.reset();
        cleanup_done.set_value();
    });
    cleanup_done.get_future().wait();

    loop_thread.Stop();
    return 0;
}
