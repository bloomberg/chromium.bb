// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/cpp/bindings/lib/thread_checker_posix.h"

#include <assert.h>

namespace mojo {
namespace internal {

ThreadCheckerPosix::ThreadCheckerPosix() : attached_thread_id_(pthread_self()) {
}

ThreadCheckerPosix::~ThreadCheckerPosix() {
  assert(CalledOnValidThread());
}

bool ThreadCheckerPosix::CalledOnValidThread() const {
  return pthread_equal(pthread_self(), attached_thread_id_);
}

}  // namespace internal
}  // namespace mojo
