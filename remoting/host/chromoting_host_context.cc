// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_context.h"

#include <string>

#include "base/bind.h"
#include "base/threading/thread.h"
#include "remoting/jingle_glue/jingle_thread.h"

namespace remoting {

ChromotingHostContext::ChromotingHostContext()
    : main_thread_("ChromotingMainThread"),
      encode_thread_("ChromotingEncodeThread"),
      desktop_thread_("ChromotingDesktopThread") {
}

ChromotingHostContext::~ChromotingHostContext() {
}

void ChromotingHostContext::Start() {
  // Start all the threads.
  main_thread_.Start();
  encode_thread_.Start();
  jingle_thread_.Start();
  desktop_thread_.Start();
}

void ChromotingHostContext::Stop() {
  // Stop all the threads.
  jingle_thread_.Stop();
  encode_thread_.Stop();
  main_thread_.Stop();
  desktop_thread_.Stop();
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

base::MessageLoopProxy* ChromotingHostContext::network_message_loop() {
  return jingle_thread_.message_loop_proxy();
}

MessageLoop* ChromotingHostContext::desktop_message_loop() {
  return desktop_thread_.message_loop();
}

void ChromotingHostContext::SetUITaskPostFunction(
    const UIThreadPostTaskFunction& poster) {
  ui_poster_ = poster;
  ui_main_thread_id_ = base::PlatformThread::CurrentId();
}

void ChromotingHostContext::PostTaskToUIThread(
    const tracked_objects::Location& from_here, const base::Closure& task) {
  ui_poster_.Run(from_here, task);
}

void ChromotingHostContext::PostDelayedTaskToUIThread(
    const tracked_objects::Location& from_here,
    const base::Closure& task,
    int delay_ms) {
  // Post delayed task on the main thread that will post task on UI
  // thread. It is safe to use base::Unretained() here because
  // ChromotingHostContext owns |main_thread_|.
  main_message_loop()->PostDelayedTask(from_here, base::Bind(
      &ChromotingHostContext::PostTaskToUIThread, base::Unretained(this),
      from_here, task), delay_ms);
}

bool ChromotingHostContext::IsUIThread() const {
  return ui_main_thread_id_ == base::PlatformThread::CurrentId();
}

}  // namespace remoting
