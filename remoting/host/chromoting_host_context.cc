// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_context.h"

#include <string>

#include "base/threading/thread.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace remoting {

ChromotingHostContext::ChromotingHostContext(MessageLoopForUI* ui_message_loop)
    : main_thread_("ChromotingMainThread"),
      encode_thread_("ChromotingEncodeThread"),
      ui_message_loop_(ui_message_loop) {
}

ChromotingHostContext::~ChromotingHostContext() {
}

void ChromotingHostContext::Start() {
  // Start all the threads.
  main_thread_.Start();
  encode_thread_.Start();
  jingle_thread_.Start();
}

void ChromotingHostContext::Stop() {
  // Stop all the threads.
  jingle_thread_.Stop();
  encode_thread_.Stop();
  main_thread_.Stop();
}

JingleThread* ChromotingHostContext::jingle_thread() {
  return &jingle_thread_;
}

MessageLoop* ChromotingHostContext::main_message_loop() {
  return main_thread_.message_loop();
}

MessageLoop* ChromotingHostContext::encode_message_loop() {
  return encode_thread_.message_loop();
}

MessageLoop* ChromotingHostContext::network_message_loop() {
  return jingle_thread_.message_loop();
}

MessageLoopForUI* ChromotingHostContext::ui_message_loop() {
  return ui_message_loop_;
}

}  // namespace remoting
