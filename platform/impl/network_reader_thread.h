// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PLATFORM_IMPL_NETWORK_READER_THREAD_H_
#define PLATFORM_IMPL_NETWORK_READER_THREAD_H_

#include <thread>

#include "platform/impl/network_reader.h"

namespace openscreen {
namespace platform {

// This is the class responsible for handling the threading associated with
// a network reader. More specifically, when this object is created, it
// starts a thread on which NetworkReader's RunUntilStopped method is called,
// and upon destruction it calls NetworkReader's RequestStopSoon method and
// joins the thread it created, blocking until the NetworkReader's operation
// completes.
// TODO(rwkeane): Move this class to osp/demo.
class NetworkReaderThread {
 public:
  NetworkReaderThread();
  ~NetworkReaderThread();

  NetworkReader* network_reader() { return &network_reader_; }

 private:
  NetworkReader network_reader_;
  std::thread thread_;

  OSP_DISALLOW_COPY_AND_ASSIGN(NetworkReaderThread);
};

}  // namespace platform
}  // namespace openscreen

#endif  // PLATFORM_IMPL_NETWORK_READER_THREAD_H_
