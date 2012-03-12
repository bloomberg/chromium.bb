// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_context.h"

#include <string>

#include "base/bind.h"
#include "base/threading/thread.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace remoting {

ChromotingHostContext::ChromotingHostContext(
    base::MessageLoopProxy* io_message_loop,
    base::MessageLoopProxy* ui_message_loop)
    : main_thread_("ChromotingMainThread"),
      encode_thread_("ChromotingEncodeThread"),
      desktop_thread_("ChromotingDesktopThread"),
      io_message_loop_(io_message_loop),
      ui_message_loop_(ui_message_loop) {
}

ChromotingHostContext::~ChromotingHostContext() {
}

bool ChromotingHostContext::Start() {
  // Start all the threads.
  return main_thread_.Start() && encode_thread_.Start() &&
      jingle_thread_.Start() && desktop_thread_.Start();
}

JingleThread* ChromotingHostContext::jingle_thread() {
  return &jingle_thread_;
}

base::MessageLoopProxy* ChromotingHostContext::io_message_loop() {
  return io_message_loop_;
}

base::MessageLoopProxy* ChromotingHostContext::ui_message_loop() {
  return ui_message_loop_;
}

MessageLoop* ChromotingHostContext::main_message_loop() {
  return main_thread_.message_loop();
}

MessageLoop* ChromotingHostContext::encode_message_loop() {
  return encode_thread_.message_loop();
}

base::MessageLoopProxy* ChromotingHostContext::network_message_loop() {
  return jingle_thread_.message_loop_proxy();
}

MessageLoop* ChromotingHostContext::desktop_message_loop() {
  return desktop_thread_.message_loop();
}

}  // namespace remoting
