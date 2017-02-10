// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/ios/app_runtime.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/task_scheduler.h"
#include "jingle/glue/thread_wrapper.h"
#include "net/socket/client_socket_factory.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/client_status_logger.h"
#include "remoting/client/ios/bridge/client_proxy.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/chromium_port_allocator_factory.h"
#include "remoting/protocol/client_authentication_config.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/negotiating_client_authenticator.h"
#include "remoting/protocol/transport_context.h"
#include "remoting/signaling/delegating_signal_strategy.h"

namespace remoting {
namespace ios {

AppRuntime::AppRuntime() {
  // TODO(sergeyu): Consider adding separate pools for different task classes.
  const int kMaxBackgroundThreads = 5;
  if (!base::TaskScheduler::GetInstance()) {
    // Make sure TaskScheduler is initialized.
    base::TaskScheduler::CreateAndSetSimpleTaskScheduler(kMaxBackgroundThreads);
  }

  // TODO(sergeyu): AppRuntime is not singleton, but it owns MessageLoop for the
  // current thread. This means that it's not safe to create multiple AppRuntime
  // instances on the same thread. AppRuntime should be a singleton, or this
  // code needs to be moved somewhere else.
  if (!base::MessageLoop::current()) {
    ui_loop_.reset(new base::MessageLoopForUI());
    base::MessageLoopForUI::current()->Attach();
  } else {
    ui_loop_.reset(base::MessageLoopForUI::current());
  }
  runtime_ = ChromotingClientRuntime::Create(ui_loop_.get());
}

AppRuntime::~AppRuntime() {
  // TODO(nicholss): Shutdown the app.
}

}  // namespace ios
}  // namespace remoting
