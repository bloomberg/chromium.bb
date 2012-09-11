// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_ANDROID_FORWARDER2_THREAD_H_
#define TOOLS_ANDROID_FORWARDER2_THREAD_H_

#include <pthread.h>

namespace forwarder2 {

// Helper abstract class to create a new thread and curry the
// call to the Run() method in an object from the new thread.
class Thread {
 public:
  Thread();
  virtual ~Thread();

  // Start thread calling Run().
  void Start();
  virtual void Join();

 protected:
  virtual void Run() = 0;

 private:
  static void* ThreadCallback(void* arg);

  pthread_t thread_;
};

}  // namespace forwarder

#endif  // TOOLS_ANDROID_FORWARDER2_THREAD_H_
