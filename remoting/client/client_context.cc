// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_context.h"

namespace remoting {

ClientContext::ClientContext(base::SingleThreadTaskRunner* main_task_runner)
    : main_task_runner_(main_task_runner),
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

base::SingleThreadTaskRunner* ClientContext::main_task_runner() {
  return main_task_runner_;
}

base::SingleThreadTaskRunner* ClientContext::decode_task_runner() {
  return decode_thread_.message_loop_proxy();
}

}  // namespace remoting
