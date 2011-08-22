// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_CONTEXT_H_
#define REMOTING_CLIENT_CLIENT_CONTEXT_H_

#include <string>

#include "base/message_loop_proxy.h"
#include "base/threading/thread.h"

namespace remoting {

// A class that manages threads and running context for the chromoting client
// process.
class ClientContext {
 public:
  ClientContext(base::MessageLoopProxy* main_message_loop_proxy);
  virtual ~ClientContext();

  void Start();
  void Stop();

  base::MessageLoopProxy* main_message_loop();
  MessageLoop* decode_message_loop();
  base::MessageLoopProxy* network_message_loop();

 private:
  scoped_refptr<base::MessageLoopProxy> main_message_loop_proxy_;

  // A thread that handles all decode operations.
  base::Thread decode_thread_;

  DISALLOW_COPY_AND_ASSIGN(ClientContext);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_CONTEXT_H_
