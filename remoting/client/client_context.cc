// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/client_context.h"

namespace remoting {

ClientContext::ClientContext(
    const scoped_refptr<base::SingleThreadTaskRunner>& main_task_runner)
    : main_task_runner_(main_task_runner),
      decode_thread_("ChromotingClientDecodeThread"),
      audio_decode_thread_("ChromotingClientAudioDecodeThread") {
}

ClientContext::~ClientContext() {
}

void ClientContext::Start() {
  // Start all the threads.
  decode_thread_.Start();
  audio_decode_thread_.Start();
}

void ClientContext::Stop() {
  // Stop all the threads.
  decode_thread_.Stop();
  audio_decode_thread_.Stop();
}

base::SingleThreadTaskRunner* ClientContext::main_task_runner() {
  return main_task_runner_.get();
}

base::SingleThreadTaskRunner* ClientContext::decode_task_runner() {
  return decode_thread_.message_loop_proxy().get();
}

base::SingleThreadTaskRunner* ClientContext::audio_decode_task_runner() {
  return audio_decode_thread_.message_loop_proxy().get();
}

}  // namespace remoting
