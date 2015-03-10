// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <poll.h>
#include "base/logging.h"
#include "net/tools/epoll_server/epoll_server.h"

namespace net {

// convert specified bits on which you wish to wait
// into the kernel equivalent.
static int AbstractBitsToKernelBits(int bits) {
  DCHECK((bits & ~(NET_POLLIN | NET_POLLOUT | NET_POLLPRI)) == 0);
  return ((bits & NET_POLLIN) ? POLLIN : 0) |
         ((bits & NET_POLLOUT) ? POLLOUT : 0) |
         ((bits & NET_POLLPRI) ? POLLPRI : 0);
}
static int KernelBitsToAbstractBits(int bits) {
  DCHECK((bits & ~(POLLIN | POLLOUT | POLLPRI | POLLERR | POLLHUP)) == 0);
  return ((bits & POLLIN) ? NET_POLLIN : 0) |
         ((bits & POLLOUT) ? NET_POLLOUT : 0) |
         ((bits & POLLPRI) ? NET_POLLPRI : 0) |
         ((bits & POLLERR) ? NET_POLLERR : 0) |
         ((bits & POLLHUP) ? NET_POLLHUP : 0);
}

class DarwinEpollServerImpl : public EpollServerImpl {
 private:
  const int N_FIXED_POLLFDS = 2;

 protected:
  // Array of |n_pollfds_| structures for the poll() call, except that the
  // size is never less than 2, even without having registered any descriptors.
  // In the expected case of registering exactly 1 descriptor, this works
  // perfectly, because the implementation adds 1 more descriptor for itself,
  // and there is neither resizing involved nor any waste.
  mutable struct pollfd* pollfds_;
  // Number of structures that we need to poll for, which is the same
  // as the number in the allocated array except when this value is 0 or 1,
  // in which case the array is minimally sized at 2.
  mutable int n_pollfds_;

 public:
  void AddFD(int fd, PollBits event_mask) const override;
  void SetInterestMask(int fd, PollBits event_mask) const override;
  void DeleteFD(int fd) const override;
  int KernelWait(int timeout) override;
  void ScanKernelEvents(EpollServer* eps, int nfds) override;
  DarwinEpollServerImpl();
  ~DarwinEpollServerImpl() override;
};

scoped_ptr<EpollServerImpl> EpollServer::CreateImplementation() {
  return static_cast<scoped_ptr<EpollServerImpl>>(new DarwinEpollServerImpl);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

DarwinEpollServerImpl::DarwinEpollServerImpl() : n_pollfds_(0) {
  // Reserving space for 2 pollfds satisfies the main need of quic_client:
  // 1 is for the socket and 1 for the "force wake up" pipe.
  // But in general the array will be expanded as required.
  pollfds_ = (struct pollfd*)malloc(N_FIXED_POLLFDS * sizeof(struct pollfd));
  if (!pollfds_) {
    LOG(FATAL) << "Couldn't allocate pollfds";
  }
  DVLOG(0)
      << "epoll_server: no kernel support for epoll(). using poll() instead";
}

DarwinEpollServerImpl::~DarwinEpollServerImpl() {
  free(pollfds_);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

void DarwinEpollServerImpl::DeleteFD(int fd) const {
  int i;
  for (i = 0; i < n_pollfds_; ++i) {
    if (pollfds_[i].fd == fd) {  // found
      // Fill the "hole" with the current highest-index pollfd
      // This is correct even if overwriting it with itself,
      // and we make no guarantee of order-preservation.
      pollfds_[i] = pollfds_[n_pollfds_ - 1];
      // pollfds_ is not shrunken if the new size would be less than N_FIXED
      // As with AddFD(), this is inefficient, but has a fairly easy remedy:
      // never resize downward; always track the true size of the array.
      // The burden will be on AddFD() to expand only when exceeding that.
      if (--n_pollfds_ >= N_FIXED_POLLFDS)
        pollfds_ = (struct pollfd*)realloc(pollfds_,
                                           n_pollfds_ * sizeof(struct pollfd));
      return;
    }
  }
  LOG(FATAL) << "DelFD: don't have fd " << fd;
}

void DarwinEpollServerImpl::AddFD(int fd, PollBits event_mask) const {
  int i = n_pollfds_++;
  // Somewhat inefficient if we need to scale well.
  // Some possibile better algorithms are:
  //  - grow by, let's say, 100 fds at a time, and track both the current
  //    effective number of fds as well as actual capacity.
  //  - construct the array just-in-time for the first call to poll()
  //    by scanning the callback map. Mark the array as dirty
  //    and in need of re-creation when changes to callbacks occur.
  if (n_pollfds_ > N_FIXED_POLLFDS)
    pollfds_ =
        (struct pollfd*)realloc(pollfds_, n_pollfds_ * sizeof(struct pollfd));
  pollfds_[i].fd = fd;
  // don't have to ask for HUP or ERR - you can always receive those
  pollfds_[i].events = AbstractBitsToKernelBits(event_mask.bits_);
}

void DarwinEpollServerImpl::SetInterestMask(int fd, PollBits event_mask) const {
  int i;
  for (i = 0; i < n_pollfds_; ++i) {
    if (pollfds_[i].fd == fd) {  // found
      pollfds_[i].events = AbstractBitsToKernelBits(event_mask.bits_);
      return;
    }
  }
  LOG(FATAL) << "ModFD: don't have fd " << fd;
}

int DarwinEpollServerImpl::KernelWait(int timeout_in_ms) {
  return poll(pollfds_, n_pollfds_, timeout_in_ms);
}

void DarwinEpollServerImpl::ScanKernelEvents(EpollServer* eps, int nfds) {
  // |nfds| from the kernel tells how many had nonzero events.
  // So ignore that and iterate over _all_ input structures.
  // (This won't be called when nfds == 0)
  for (int i = 0; i < n_pollfds_; ++i) {
    int event_mask = pollfds_[i].revents;
    if (event_mask) {
      eps->HandleEvent(pollfds_[i].fd, KernelBitsToAbstractBits(event_mask));
    }
  }
}

}  // namespace net
