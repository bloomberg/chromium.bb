// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_context.h"

namespace remoting {

ClientContext::ClientContext(base::MessageLoopProxy* main_message_loop_proxy)
    : main_message_loop_proxy_(main_message_loop_proxy),
      decode_thread_("ChromotingClientDecodeThread") {
}

ClientContext::~ClientContext() {
}

void ClientContext::Start() {
  // Start all the threads.
  decode_thread_.Start();
}

void ClientContext::Stop() {
  // Stop all the threads.
  decode_thread_.Stop();
}

base::MessageLoopProxy* ClientContext::main_message_loop() {
  return main_message_loop_proxy_;
}

base::MessageLoopProxy* ClientContext::decode_message_loop() {
  return decode_thread_.message_loop_proxy();
}

base::MessageLoopProxy* ClientContext::network_message_loop() {
  return main_message_loop_proxy_;
}

}  // namespace remoting
