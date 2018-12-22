// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/background/background_service_manager.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_pump_default.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/simple_thread.h"
#include "services/catalog/store.h"
#include "services/service_manager/connect_params.h"
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/service_manager.h"
#include "services/service_manager/standalone/context.h"

namespace service_manager {

BackgroundServiceManager::BackgroundServiceManager(
    ServiceProcessLauncherDelegate* launcher_delegate,
    const std::vector<Manifest>& manifests)
    : background_thread_("service_manager") {
  background_thread_.Start();
  background_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundServiceManager::InitializeOnBackgroundThread,
                     base::Unretained(this), launcher_delegate, manifests));
}

BackgroundServiceManager::~BackgroundServiceManager() {
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  background_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundServiceManager::ShutDownOnBackgroundThread,
                 base::Unretained(this), &done_event));
  done_event.Wait();
  DCHECK(!context_);
}

void BackgroundServiceManager::RegisterService(
    const Identity& identity,
    mojom::ServicePtr service,
    mojom::PIDReceiverRequest pid_receiver_request) {
  mojom::ServicePtrInfo service_info = service.PassInterface();
  background_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&BackgroundServiceManager::RegisterServiceOnBackgroundThread,
                 base::Unretained(this), identity, base::Passed(&service_info),
                 base::Passed(&pid_receiver_request)));
}

void BackgroundServiceManager::InitializeOnBackgroundThread(
    ServiceProcessLauncherDelegate* launcher_delegate,
    const std::vector<Manifest>& manifests) {
  context_ = std::make_unique<Context>(launcher_delegate, manifests);
}

void BackgroundServiceManager::ShutDownOnBackgroundThread(
    base::WaitableEvent* done_event) {
  context_.reset();
  done_event->Signal();
}

void BackgroundServiceManager::RegisterServiceOnBackgroundThread(
    const Identity& identity,
    mojom::ServicePtrInfo service_info,
    mojom::PIDReceiverRequest pid_receiver_request) {
  mojom::ServicePtr service;
  service.Bind(std::move(service_info));
  context_->service_manager()->RegisterService(
      identity, std::move(service), std::move(pid_receiver_request));
}

}  // namespace service_manager
