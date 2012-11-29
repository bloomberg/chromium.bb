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
      video_capture_thread_("ChromotingCaptureThread"),
      video_encode_thread_("ChromotingEncodeThread"),
      file_thread_("ChromotingFileIOThread"),
      input_thread_("ChromotingInputThread"),
      network_thread_("ChromotingNetworkThread"),
      ui_task_runner_(ui_task_runner) {
}

ChromotingHostContext::~ChromotingHostContext() {
}

bool ChromotingHostContext::Start() {
  // Start all the threads.
  base::Thread::Options io_thread_options(MessageLoop::TYPE_IO, 0);

  bool started = video_capture_thread_.Start() && video_encode_thread_.Start();

#if defined(OS_WIN)
  // On Windows audio capturer needs to run on a UI thread that has COM
  // initialized.
  audio_thread_.init_com_with_mta(false);
  started = started && audio_thread_.Start();
#else  // defined(OS_WIN)
  started = started && audio_thread_.StartWithOptions(io_thread_options);
#endif  // !defined(OS_WIN)

  started = started &&
      file_thread_.StartWithOptions(io_thread_options) &&
      input_thread_.StartWithOptions(io_thread_options) &&
      network_thread_.StartWithOptions(io_thread_options);
  if (!started)
    return false;

  // Wrap worker threads with |AutoThreadTaskRunner| and have them reference
  // the main thread via |ui_task_runner_|, to ensure that it remain active to
  // Stop() them when no references remain.
  audio_task_runner_ =
      new AutoThreadTaskRunner(audio_thread_.message_loop_proxy(),
                               ui_task_runner_);
  video_capture_task_runner_ =
      new AutoThreadTaskRunner(video_capture_thread_.message_loop_proxy(),
                               ui_task_runner_);
  video_encode_task_runner_ =
      new AutoThreadTaskRunner(video_encode_thread_.message_loop_proxy(),
                               ui_task_runner_);
  file_task_runner_ =
      new AutoThreadTaskRunner(file_thread_.message_loop_proxy(),
                               ui_task_runner_);
  input_task_runner_ =
      new AutoThreadTaskRunner(input_thread_.message_loop_proxy(),
                               ui_task_runner_);
  network_task_runner_ =
      new AutoThreadTaskRunner(network_thread_.message_loop_proxy(),
                               ui_task_runner_);

  url_request_context_getter_ = new URLRequestContextGetter(
      ui_task_runner(), network_task_runner());
  return true;
}

base::SingleThreadTaskRunner* ChromotingHostContext::audio_task_runner() {
  return audio_task_runner_;
}

base::SingleThreadTaskRunner*
ChromotingHostContext::video_capture_task_runner() {
  return video_capture_task_runner_;
}

base::SingleThreadTaskRunner*
ChromotingHostContext::video_encode_task_runner() {
  return video_encode_task_runner_;
}

base::SingleThreadTaskRunner* ChromotingHostContext::file_task_runner() {
  return file_task_runner_;
}

base::SingleThreadTaskRunner* ChromotingHostContext::input_task_runner() {
  return input_task_runner_;
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
