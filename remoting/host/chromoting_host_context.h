// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
#define REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/threading/thread.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace remoting {

// A class that manages threads and running context for the chromoting host
// process.
class ChromotingHostContext {
 public:
  ChromotingHostContext();
  virtual ~ChromotingHostContext();

  // TODO(ajwong): Move the Start/Stop methods out of this class. Then
  // create a static factory for construction, and destruction.  We
  // should be able to remove the need for virtual functions below with that
  // design, while preserving the relative simplicity of this API.
  virtual void Start();
  virtual void Stop();

  virtual JingleThread* jingle_thread();

  virtual MessageLoop* main_message_loop();
  virtual MessageLoop* encode_message_loop();
  virtual MessageLoop* network_message_loop();

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromotingHostContextTest, StartAndStop);

  // A thread that hosts all network operations.
  JingleThread jingle_thread_;

  // A thread that hosts ChromotingHost and performs rate control.
  base::Thread main_thread_;

  // A thread that hosts all encode operations.
  base::Thread encode_thread_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingHostContext);
};

}  // namespace remoting

DISABLE_RUNNABLE_METHOD_REFCOUNT(remoting::ChromotingHostContext);

#endif  // REMOTING_HOST_CHROMOTING_HOST_CONTEXT_H_
