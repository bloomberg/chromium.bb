// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "net/tools/epoll_server/epoll_server.h"
#include <sys/epoll.h>

// The size we use for buffers passed to strerror_r
static const int kErrorBufferSize = 256;

namespace net {

// convert specified bits on which you wish to wait
// into the kernel equivalent.
static int AbstractBitsToKernelBits(int bits) {
  if (bits & (NET_POLLERR | NET_POLLHUP)) {
    LOG(INFO) << "You don't need to specify waiting on POLLERR/POLLUP bits";
    bits &= ~(NET_POLLERR | NET_POLLHUP);
  }
  DCHECK((bits & ~(NET_POLLIN | NET_POLLOUT | NET_POLLPRI | NET_POLLET)) == 0);
  return ((bits & NET_POLLIN) ? EPOLLIN : 0) |
         ((bits & NET_POLLOUT) ? EPOLLOUT : 0) |
         ((bits & NET_POLLPRI) ? EPOLLPRI : 0) |
         ((bits & NET_POLLET) ? EPOLLET : 0);
}
static int KernelBitsToAbstractBits(int bits) {
  DCHECK((bits & ~(EPOLLIN | EPOLLOUT | EPOLLPRI | EPOLLERR | EPOLLHUP)) == 0);
  return ((bits & EPOLLIN) ? NET_POLLIN : 0) |
         ((bits & EPOLLOUT) ? NET_POLLOUT : 0) |
         ((bits & EPOLLPRI) ? NET_POLLPRI : 0) |
         ((bits & EPOLLERR) ? NET_POLLERR : 0) |
         ((bits & EPOLLHUP) ? NET_POLLHUP : 0);
}

class LinuxEpollServerImpl : public EpollServerImpl {
 protected:
  int epoll_fd_;
  // TODO(alyssar): make this into something that scales up.
  static const int events_size_ = 256;
  struct epoll_event events_[256];

 public:
  void AddFD(int fd, PollBits event_mask) const override;
  void SetInterestMask(int fd, PollBits event_mask) const override;
  void DeleteFD(int fd) const override;
  int KernelWait(int timeout) override;
  void ScanKernelEvents(EpollServer* eps, int nfds) override;
  LinuxEpollServerImpl();
  ~LinuxEpollServerImpl() override;
};

scoped_ptr<EpollServerImpl> EpollServer::CreateImplementation() {
  return static_cast<scoped_ptr<EpollServerImpl>>(new LinuxEpollServerImpl);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

LinuxEpollServerImpl::LinuxEpollServerImpl() : epoll_fd_(epoll_create(1024)) {
  CHECK_NE(epoll_fd_, -1);  // ensure that the epoll_fd_ is valid.
}

LinuxEpollServerImpl::~LinuxEpollServerImpl() {
  close(epoll_fd_);
}

void LinuxEpollServerImpl::DeleteFD(int fd) const {
  struct epoll_event ee;
  memset(&ee, 0, sizeof(ee));
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, &ee)) {
    int saved_errno = errno;
    char buf[kErrorBufferSize];
    LOG(FATAL) << "Epoll set removal error for fd " << fd << ": "
               << strerror_r(saved_errno, buf, sizeof(buf));
  }
}

void LinuxEpollServerImpl::AddFD(int fd, PollBits event_mask) const {
  struct epoll_event ee;
  memset(&ee, 0, sizeof(ee));
  // don't have to ask for HUP or ERR - you can always receive those
  ee.events = AbstractBitsToKernelBits(event_mask.bits_);
  ee.data.fd = fd;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &ee)) {
    int saved_errno = errno;
    char buf[kErrorBufferSize];
    LOG(FATAL) << "Epoll set insertion error for fd " << fd << ": "
               << strerror_r(saved_errno, buf, sizeof(buf));
  }
}

void LinuxEpollServerImpl::SetInterestMask(int fd, PollBits event_mask) const {
  struct epoll_event ee;
  memset(&ee, 0, sizeof(ee));
  ee.events = event_mask.bits_;
  ee.data.fd = fd;
  if (epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &ee)) {
    int saved_errno = errno;
    char buf[kErrorBufferSize];
    LOG(FATAL) << "Epoll set modification error for fd " << fd << ": "
               << strerror_r(saved_errno, buf, sizeof(buf));
  }
}

int LinuxEpollServerImpl::KernelWait(int timeout_in_ms) {
  return epoll_wait(epoll_fd_, events_, events_size_, timeout_in_ms);
}

void LinuxEpollServerImpl::ScanKernelEvents(EpollServer* eps, int nfds) {
  for (int i = 0; i < nfds; ++i) {
    eps->HandleEvent(events_[i].data.fd,
                     KernelBitsToAbstractBits(events_[i].events));
  }
}

}  // namespace net
