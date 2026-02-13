#pragma once
#include <functional>
#include <unordered_map>
#include <cstdint>
#include "fd.hpp"

namespace irr {
using FdHandler = std::function<void(uint32_t)>;

class Reactor {
   public:
    Reactor();
    ~Reactor();
    bool add_fd(int fd, uint32_t events, const FdHandler& cb);
    bool mod_fd(int fd, uint32_t events);
    void del_fd(int fd);
    void loop_once(int timeout_ms);
    int fd() const { return epoll_fd_; }

   private:
    int epoll_fd_{-1};
    std::unordered_map<int, FdHandler> handlers_;
};
}  // namespace irr
