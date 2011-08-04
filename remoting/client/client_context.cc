// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_context.h"

#include <string>

#include "base/threading/thread.h"
#include "remoting/client/plugin/pepper_util.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace remoting {

ClientContext::ClientContext()
    : main_thread_("ChromotingClientMainThread"),
      decode_thread_("ChromotingClientDecodeThread"),
      started_(false) {
}

ClientContext::~ClientContext() {
}

void ClientContext::Start() {
  // Start all the threads.
  DCHECK(CurrentlyOnPluginThread());
  main_thread_.Start();
  decode_thread_.Start();
  jingle_thread_.Start();
  started_ = true;
}

void ClientContext::Stop() {
  DCHECK(CurrentlyOnPluginThread());
  if (started_) {
    // Stop all the threads.
    jingle_thread_.Stop();
    decode_thread_.Stop();
    main_thread_.Stop();
    started_ = false;
  }
}

JingleThread* ClientContext::jingle_thread() {
  return &jingle_thread_;
}

MessageLoop* ClientContext::main_message_loop() {
  return main_thread_.message_loop();
}

MessageLoop* ClientContext::decode_message_loop() {
  return decode_thread_.message_loop();
}

MessageLoop* ClientContext::network_message_loop() {
  return jingle_thread_.message_loop();
}

}  // namespace remoting
