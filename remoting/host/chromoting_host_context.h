// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
#define REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_

#include <string>

#include "base/thread.h"
#include "remoting/jingle_glue/jingle_thread.h"
#include "testing/gtest/include/gtest/gtest_prod.h"

namespace remoting {

// A class that manages threads and running context for the chromoting host
// process.
class ChromotingHostContext {
 public:
  ChromotingHostContext();
  virtual ~ChromotingHostContext();

  virtual void Start();
  virtual void Stop();

  virtual JingleThread* jingle_thread();
  virtual MessageLoop* main_message_loop();
  virtual MessageLoop* capture_message_loop();
  virtual MessageLoop* encode_message_loop();

 private:
  FRIEND_TEST(ChromotingHostContextTest, StartAndStop);

  // A thread that hosts all network operations.
  JingleThread jingle_thread_;

  // A thread that hosts ChromotingHost and performs rate control.
  base::Thread main_thread_;

  // A thread that hosts all capture operations.
  base::Thread capture_thread_;

  // A thread that hosts all encode operations.
  base::Thread encode_thread_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHostContext);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::ChromotingHostContext);

#endif  // REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
