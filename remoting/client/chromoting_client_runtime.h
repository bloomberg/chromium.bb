// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_CLIENT_RUNTIME_H_
#define REMOTING_CLIENT_CHROMOTING_CLIENT_RUNTIME_H_

#include <memory>

#include "base/macros.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"

namespace base {
class MessageLoopForUI;
}  // namespace base

// Houses the global resources on which the Chromoting components run
// (e.g. message loops and task runners).
namespace remoting {

class ChromotingClientRuntime {
 public:
  // Caller to create is responsible for creating and attaching a new ui thread
  // for use. Example:
  //
  // On Android, the UI thread is managed by Java, so we need to attach and
  // start a special type of message loop to allow Chromium code to run tasks.
  //
  //  base::MessageLoopForUI *ui_loop = new base::MessageLoopForUI();
  //  ui_loop_->Start();
  //  std::unique_ptr<ChromotingClientRuntime> runtime =
  //    ChromotingClientRuntime::Create(ui_loop);
  //
  // On iOS we created a new message loop and now attach it.
  //
  //  base::MessageLoopForUI *ui_loop = new base::MessageLoopForUI();
  //  ui_loop_->Attach();
  //  std::unique_ptr<ChromotingClientRuntime> runtime =
  //    ChromotingClientRuntime::Create(ui_loop);
  //
  static std::unique_ptr<ChromotingClientRuntime> Create(
      base::MessageLoopForUI* ui_loop);

  ~ChromotingClientRuntime();

  scoped_refptr<AutoThreadTaskRunner> network_task_runner() {
    return network_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> display_task_runner() {
    return display_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> file_task_runner() {
    return file_task_runner_;
  }

  scoped_refptr<net::URLRequestContextGetter> url_requester() {
    return url_requester_;
  }

 private:
  ChromotingClientRuntime();
  ChromotingClientRuntime(
      scoped_refptr<AutoThreadTaskRunner> ui_task_runner,
      scoped_refptr<AutoThreadTaskRunner> display_task_runner,
      scoped_refptr<AutoThreadTaskRunner> network_task_runner,
      scoped_refptr<AutoThreadTaskRunner> file_task_runner,
      scoped_refptr<net::URLRequestContextGetter> url_requester);

  // References to native threads.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;

  scoped_refptr<AutoThreadTaskRunner> display_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> file_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_requester_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingClientRuntime);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_CHROMOTING_CLIENT_RUNTIME_H_
