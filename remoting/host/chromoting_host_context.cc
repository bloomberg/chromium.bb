// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_context.h"

#include <string>

#include "base/bind.h"
#include "base/threading/thread.h"
#include "remoting/base/auto_thread_task_runner.h"
#include "remoting/host/url_request_context.h"

namespace remoting {

ChromotingHostContext::ChromotingHostContext(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner)
    : audio_thread_("ChromotingAudioThread"),
      capture_thread_("ChromotingCaptureThread"),
      desktop_thread_("ChromotingDesktopThread"),
      encode_thread_("ChromotingEncodeThread"),
      file_thread_("ChromotingFileIOThread"),
      network_thread_("ChromotingNetworkThread"),
      ui_task_runner_(ui_task_runner) {
}

ChromotingHostContext::~ChromotingHostContext() {
}

void ChromotingHostContext::ReleaseTaskRunners() {
  url_request_context_getter_ = NULL;
  audio_task_runner_ = NULL;
  capture_task_runner_ = NULL;
  desktop_task_runner_ = NULL;
  encode_task_runner_ = NULL;
  file_task_runner_ = NULL;
  network_task_runner_ = NULL;
  ui_task_runner_ = NULL;
}

bool ChromotingHostContext::Start() {
  // Start all the threads.
  bool started = capture_thread_.Start() && encode_thread_.Start() &&
      audio_thread_.StartWithOptions(base::Thread::Options(
          MessageLoop::TYPE_IO, 0)) &&
      network_thread_.StartWithOptions(base::Thread::Options(
          MessageLoop::TYPE_IO, 0)) &&
      desktop_thread_.Start() &&
      file_thread_.StartWithOptions(
          base::Thread::Options(MessageLoop::TYPE_IO, 0));
  if (!started)
    return false;

  // Wrap worker threads with |AutoThreadTaskRunner| and have them reference
  // the main thread via |ui_task_runner_|, to ensure that it remain active to
  // Stop() them when no references remain.
  audio_task_runner_ =
      new AutoThreadTaskRunner(audio_thread_.message_loop_proxy(),
                               ui_task_runner_);
  capture_task_runner_ =
      new AutoThreadTaskRunner(capture_thread_.message_loop_proxy(),
                               ui_task_runner_);
  desktop_task_runner_ =
      new AutoThreadTaskRunner(desktop_thread_.message_loop_proxy(),
                               ui_task_runner_);
  encode_task_runner_ =
      new AutoThreadTaskRunner(encode_thread_.message_loop_proxy(),
                               ui_task_runner_);
  file_task_runner_ =
      new AutoThreadTaskRunner(file_thread_.message_loop_proxy(),
                               ui_task_runner_);
  network_task_runner_ =
      new AutoThreadTaskRunner(network_thread_.message_loop_proxy(),
                               ui_task_runner_);

  url_request_context_getter_ = new URLRequestContextGetter(
      ui_task_runner(), network_task_runner(),
      static_cast<MessageLoopForIO*>(file_thread_.message_loop()));
  return true;
}

base::SingleThreadTaskRunner* ChromotingHostContext::audio_task_runner() {
  return audio_task_runner_;
}

base::SingleThreadTaskRunner* ChromotingHostContext::capture_task_runner() {
  return capture_task_runner_;
}

base::SingleThreadTaskRunner* ChromotingHostContext::desktop_task_runner() {
  return desktop_task_runner_;
}

base::SingleThreadTaskRunner* ChromotingHostContext::encode_task_runner() {
  return encode_task_runner_;
}

base::SingleThreadTaskRunner* ChromotingHostContext::file_task_runner() {
  return file_task_runner_;
}

base::SingleThreadTaskRunner* ChromotingHostContext::network_task_runner() {
  return network_task_runner_;
}

base::SingleThreadTaskRunner* ChromotingHostContext::ui_task_runner() {
  return ui_task_runner_;
}

const scoped_refptr<net::URLRequestContextGetter>&
ChromotingHostContext::url_request_context_getter() {
  DCHECK(url_request_context_getter_.get());
  return url_request_context_getter_;
}

}  // namespace remoting
