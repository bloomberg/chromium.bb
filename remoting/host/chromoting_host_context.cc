// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromoting_host_context.h"

#include "content/public/browser/browser_thread.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/url_request_context_getter.h"

namespace remoting {

ChromotingHostContext::ChromotingHostContext(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
    scoped_refptr<AutoThreadTaskRunner> audio_task_runner,
    scoped_refptr<AutoThreadTaskRunner> file_task_runner,
    scoped_refptr<AutoThreadTaskRunner> input_task_runner,
    scoped_refptr<AutoThreadTaskRunner> network_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_capture_task_runner,
    scoped_refptr<AutoThreadTaskRunner> video_encode_task_runner,
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter)
    : ui_task_runner_(ui_task_runner),
      audio_task_runner_(audio_task_runner),
      file_task_runner_(file_task_runner),
      input_task_runner_(input_task_runner),
      network_task_runner_(network_task_runner),
      video_capture_task_runner_(video_capture_task_runner),
      video_encode_task_runner_(video_encode_task_runner),
      url_request_context_getter_(url_request_context_getter) {
}

ChromotingHostContext::~ChromotingHostContext() {
}

scoped_ptr<ChromotingHostContext> ChromotingHostContext::Copy() {
  return make_scoped_ptr(new ChromotingHostContext(
      ui_task_runner_, audio_task_runner_, file_task_runner_,
      input_task_runner_, network_task_runner_, video_capture_task_runner_,
      video_encode_task_runner_, url_request_context_getter_));
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

scoped_ptr<ChromotingHostContext> ChromotingHostContext::Create(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner) {
#if defined(OS_WIN)
  // On Windows the AudioCapturer requires COM, so we run a single-threaded
  // apartment, which requires a UI thread.
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner =
      AutoThread::CreateWithLoopAndComInitTypes(
          "ChromotingAudioThread", ui_task_runner, base::MessageLoop::TYPE_UI,
          AutoThread::COM_INIT_STA);
#else   // !defined(OS_WIN)
  scoped_refptr<AutoThreadTaskRunner> audio_task_runner =
      AutoThread::CreateWithType("ChromotingAudioThread", ui_task_runner,
                                 base::MessageLoop::TYPE_IO);
#endif  // !defined(OS_WIN)
  scoped_refptr<AutoThreadTaskRunner> file_task_runner =
      AutoThread::CreateWithType("ChromotingFileThread", ui_task_runner,
                                 base::MessageLoop::TYPE_IO);
  scoped_refptr<AutoThreadTaskRunner> network_task_runner =
      AutoThread::CreateWithType("ChromotingNetworkThread", ui_task_runner,
                                 base::MessageLoop::TYPE_IO);

  return make_scoped_ptr(new ChromotingHostContext(
      ui_task_runner,
      audio_task_runner,
      file_task_runner,
      AutoThread::CreateWithType("ChromotingInputThread", ui_task_runner,
                                 base::MessageLoop::TYPE_IO),
      network_task_runner,
      AutoThread::Create("ChromotingCaptureThread", ui_task_runner),
      AutoThread::Create("ChromotingEncodeThread", ui_task_runner),
      make_scoped_refptr(
          new URLRequestContextGetter(network_task_runner, file_task_runner))));
}

#if defined(OS_CHROMEOS)
namespace {
// Retrieves the task_runner from the browser thread with |id|.
scoped_refptr<AutoThreadTaskRunner> WrapBrowserThread(
    content::BrowserThread::ID id) {
  // AutoThreadTaskRunner is a TaskRunner with the special property that it will
  // continue to process tasks until no references remain, at least. The
  // QuitClosure we usually pass does the simple thing of stopping the
  // underlying TaskRunner.  Since we are re-using the ui_task_runner of the
  // browser thread, we cannot stop it explicitly.  Therefore, base::DoNothing
  // is passed in as the quit closure.
  // TODO(kelvinp): Fix this (See crbug.com/428187).
  return new AutoThreadTaskRunner(
      content::BrowserThread::GetMessageLoopProxyForThread(id).get(),
      base::Bind(&base::DoNothing));
}

}  // namespace

// static
scoped_ptr<ChromotingHostContext> ChromotingHostContext::CreateForChromeOS(
    scoped_refptr<net::URLRequestContextGetter> url_request_context_getter) {
  DCHECK(url_request_context_getter.get());

  // Use BrowserThread::FILE as the joiner as it is the only browser-thread
  // that allows blocking I/O, which is required by thread joining.
  // TODO(kelvinp): Fix AutoThread so that it can be joinable on task runners
  // that disallow I/O (crbug.com/428466).
  scoped_refptr<AutoThreadTaskRunner> file_task_runner =
      WrapBrowserThread(content::BrowserThread::FILE);

  scoped_refptr<AutoThreadTaskRunner> ui_task_runner =
      WrapBrowserThread(content::BrowserThread::UI);

  return make_scoped_ptr(new ChromotingHostContext(
      ui_task_runner,
      AutoThread::CreateWithType("ChromotingAudioThread", file_task_runner,
                                 base::MessageLoop::TYPE_IO),
      file_task_runner,
      AutoThread::CreateWithType("ChromotingInputThread", file_task_runner,
                                 base::MessageLoop::TYPE_IO),
      WrapBrowserThread(content::BrowserThread::IO),  // network_task_runner
      ui_task_runner,  // video_capture_task_runner
      AutoThread::CreateWithType("ChromotingEncodeThread", file_task_runner,
                                 base::MessageLoop::TYPE_IO),
      url_request_context_getter));
}
#endif  // defined(OS_CHROMEOS)

}  // namespace remoting
