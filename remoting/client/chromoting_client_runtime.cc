// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client_runtime.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "remoting/base/url_request_context_getter.h"

namespace remoting {

std::unique_ptr<ChromotingClientRuntime> ChromotingClientRuntime::Create(
    base::MessageLoopForUI* ui_loop) {
  DCHECK(ui_loop);

  // |ui_loop_| runs on the main thread, so |ui_task_runner_| will run on the
  // main thread.  We can not kill the main thread when the message loop becomes
  // idle so the callback function does nothing (as opposed to the typical
  // base::MessageLoop::QuitClosure())
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner = new AutoThreadTaskRunner(
      ui_loop->task_runner(), base::Bind(&base::DoNothing));

  scoped_refptr<AutoThreadTaskRunner> display_task_runner =
      AutoThread::Create("native_disp", ui_task_runner);
  scoped_refptr<AutoThreadTaskRunner> network_task_runner =
      AutoThread::CreateWithType("native_net", ui_task_runner,
                                 base::MessageLoop::TYPE_IO);
  scoped_refptr<AutoThreadTaskRunner> file_task_runner =
      AutoThread::CreateWithType("native_file", ui_task_runner,
                                 base::MessageLoop::TYPE_IO);
  scoped_refptr<net::URLRequestContextGetter> url_requester =
      new URLRequestContextGetter(network_task_runner, file_task_runner);

  return base::WrapUnique(new ChromotingClientRuntime(
      ui_task_runner, display_task_runner, network_task_runner,
      file_task_runner, url_requester));
}

ChromotingClientRuntime::ChromotingClientRuntime(
    scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
    scoped_refptr<AutoThreadTaskRunner> display_task_runner,
    scoped_refptr<AutoThreadTaskRunner> network_task_runner,
    scoped_refptr<AutoThreadTaskRunner> file_task_runner,
    scoped_refptr<net::URLRequestContextGetter> url_requester)
    : ui_task_runner_(ui_task_runner),
      display_task_runner_(display_task_runner),
      network_task_runner_(network_task_runner),
      file_task_runner_(file_task_runner),
      url_requester_(url_requester) {}

ChromotingClientRuntime::~ChromotingClientRuntime() {}

}  // namespace remoting
