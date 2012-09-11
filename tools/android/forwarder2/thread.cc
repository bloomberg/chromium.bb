// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/android/forwarder2/thread.h"

#include "base/logging.h"

namespace forwarder2 {

Thread::Thread() : thread_(static_cast<pthread_t>(-1)) {}

Thread::~Thread() {}

void Thread::Start() {
  int ret = pthread_create(&thread_, NULL, &ThreadCallback, this);
  CHECK_EQ(0, ret);
}

void Thread::Join() {
  CHECK_NE(static_cast<pthread_t>(-1), thread_);
  int ret = pthread_join(thread_, NULL);
  CHECK_EQ(0, ret);
}

// static
void* Thread::ThreadCallback(void* arg) {
  CHECK(arg);
  Thread* obj = reinterpret_cast<Thread*>(arg);
  obj->Run();
  return NULL;
}

}  // namespace forwarder
