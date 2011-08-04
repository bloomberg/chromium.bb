// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CLIENT_CONTEXT_H_
#define REMOTING_CLIENT_CLIENT_CONTEXT_H_

#include <string>

#include "base/threading/thread.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace remoting {

// A class that manages threads and running context for the chromoting client
// process. This class is not designed to be subclassed.
class ClientContext {
 public:
  ClientContext();
  ~ClientContext();

  // Start and Stop must be called from the main plugin thread.
  void Start();
  void Stop();

  JingleThread* jingle_thread();

  MessageLoop* main_message_loop();
  MessageLoop* decode_message_loop();
  MessageLoop* network_message_loop();

 private:
  // A thread that handles Jingle network operations (used in
  // JingleHostConnection).
  JingleThread jingle_thread_;

  // A thread that handles capture rate control and sending data to the
  // HostConnection.
  base::Thread main_thread_;

  // A thread that handles all decode operations.
  base::Thread decode_thread_;

  // True if Start() was called on the context.
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(ClientContext);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CLIENT_CONTEXT_H_
