// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/impl/network_reader_thread.h"

namespace openscreen {
namespace platform {

NetworkReaderThread::NetworkReaderThread()
    : thread_(&NetworkReader::RunUntilStopped, &network_reader_) {}

NetworkReaderThread::~NetworkReaderThread() {
  network_reader_.RequestStopSoon();
  thread_.join();
}

}  // namespace platform
}  // namespace openscreen
