#pragma once
#include <unistd.h>

namespace irr {
class Fd {
   public:
    Fd() : fd_(-1) {}
    explicit Fd(int fd) : fd_(fd) {}
    ~Fd() {
        reset();
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
    Fd(Fd&& o) noexcept : fd_(o.fd_) {
        o.fd_ = -1;
    }
    Fd& operator=(Fd&& o) noexcept {
        if (this != &o) {
            reset();
            fd_ = o.fd_;
            o.fd_ = -1;
        }
        return *this;
    }
    int get() const {
        return fd_;
    }
    int release() {
        int tmp = fd_;
        fd_ = -1;
        return tmp;
    }
    void reset(int fd = -1) {
        if (fd_ >= 0) ::close(fd_);
        fd_ = fd;
    }
    explicit operator bool() const {
        return fd_ >= 0;
    }

   private:
    int fd_;
};
}  // namespace irr
