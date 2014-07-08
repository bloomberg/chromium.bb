// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_context.h"

#include <string>

#include "base/bind.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/url_request_context_getter.h"

namespace remoting {

ChromotingHostContext::ChromotingHostContext(
    AutoThreadTaskRunner* ui_task_runner)
    : ui_task_runner_(ui_task_runner) {
#if defined(OS_WIN)
  // On Windows the AudioCapturer requires COM, so we run a single-threaded
  // apartment, which requires a UI thread.
  audio_task_runner_ =
      AutoThread::CreateWithLoopAndComInitTypes("ChromotingAudioThread",
                                                ui_task_runner_,
                                                base::MessageLoop::TYPE_UI,
                                                AutoThread::COM_INIT_STA);
#else // !defined(OS_WIN)
  audio_task_runner_ = AutoThread::CreateWithType(
      "ChromotingAudioThread", ui_task_runner_, base::MessageLoop::TYPE_IO);
#endif  // !defined(OS_WIN)

  file_task_runner_ = AutoThread::CreateWithType(
      "ChromotingFileThread", ui_task_runner_, base::MessageLoop::TYPE_IO);
  input_task_runner_ = AutoThread::CreateWithType(
      "ChromotingInputThread", ui_task_runner_, base::MessageLoop::TYPE_IO);
  network_task_runner_ = AutoThread::CreateWithType(
      "ChromotingNetworkThread", ui_task_runner_, base::MessageLoop::TYPE_IO);
  video_capture_task_runner_ =
      AutoThread::Create("ChromotingCaptureThread", ui_task_runner_);
  video_encode_task_runner_ = AutoThread::Create(
      "ChromotingEncodeThread", ui_task_runner_);

  url_request_context_getter_ = new URLRequestContextGetter(
      network_task_runner_);
}

ChromotingHostContext::~ChromotingHostContext() {
}

scoped_ptr<ChromotingHostContext> ChromotingHostContext::Create(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner) {
  DCHECK(ui_task_runner->BelongsToCurrentThread());

  scoped_ptr<ChromotingHostContext> context(
      new ChromotingHostContext(ui_task_runner.get()));
  if (!context->audio_task_runner_.get() || !context->file_task_runner_.get() ||
      !context->input_task_runner_.get() ||
      !context->network_task_runner_.get() ||
      !context->video_capture_task_runner_.get() ||
      !context->video_encode_task_runner_.get() ||
      !context->url_request_context_getter_.get()) {
    context.reset();
  }

  return context.Pass();
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::audio_task_runner() {
  return audio_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::file_task_runner() {
  return file_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::input_task_runner() {
  return input_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::network_task_runner() {
  return network_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::ui_task_runner() {
  return ui_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::video_capture_task_runner() {
  return video_capture_task_runner_;
}

scoped_refptr<AutoThreadTaskRunner>
ChromotingHostContext::video_encode_task_runner() {
  return video_encode_task_runner_;
}

scoped_refptr<net::URLRequestContextGetter>
ChromotingHostContext::url_request_context_getter() {
  return url_request_context_getter_;
}

}  // namespace remoting
