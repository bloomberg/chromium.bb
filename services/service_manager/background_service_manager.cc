// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/service_manager/background_service_manager.h"

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
#include "services/service_manager/public/cpp/service.h"
#include "services/service_manager/service_manager.h"

#if !defined(OS_IOS)
#include "services/service_manager/service_process_launcher.h"
#include "services/service_manager/service_process_launcher_factory.h"
#endif

namespace service_manager {

namespace {

#if !defined(OS_IOS)
// Used to ensure we only init once.
class ServiceProcessLauncherFactoryImpl : public ServiceProcessLauncherFactory {
 public:
  ServiceProcessLauncherFactoryImpl() = default;

 private:
  std::unique_ptr<ServiceProcessLauncher> Create(
      const base::FilePath& service_path) override {
    return std::make_unique<ServiceProcessLauncher>(nullptr, service_path);
  }
};
#endif

}  // namespace

BackgroundServiceManager::BackgroundServiceManager(
    const std::vector<Manifest>& manifests)
    : background_thread_("service_manager") {
  background_thread_.Start();
  background_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundServiceManager::InitializeOnBackgroundThread,
                     base::Unretained(this), manifests));
}

BackgroundServiceManager::~BackgroundServiceManager() {
  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::MANUAL,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  background_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&BackgroundServiceManager::ShutDownOnBackgroundThread,
                     base::Unretained(this), &done_event));
  done_event.Wait();
  DCHECK(!service_manager_);
}

void BackgroundServiceManager::RegisterService(
    const Identity& identity,
    mojom::ServicePtr service,
    mojom::PIDReceiverRequest pid_receiver_request) {
  mojom::ServicePtrInfo service_info = service.PassInterface();
  background_thread_.task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &BackgroundServiceManager::RegisterServiceOnBackgroundThread,
          base::Unretained(this), identity, base::Passed(&service_info),
          base::Passed(&pid_receiver_request)));
}

void BackgroundServiceManager::InitializeOnBackgroundThread(
    const std::vector<Manifest>& manifests) {
  std::unique_ptr<ServiceProcessLauncherFactory> process_launcher_factory;
#if !defined(OS_IOS)
  process_launcher_factory =
      std::make_unique<ServiceProcessLauncherFactoryImpl>();
#endif

  service_manager_ = std::make_unique<ServiceManager>(
      std::move(process_launcher_factory), manifests);
}

void BackgroundServiceManager::ShutDownOnBackgroundThread(
    base::WaitableEvent* done_event) {
  service_manager_.reset();
  done_event->Signal();
}

void BackgroundServiceManager::RegisterServiceOnBackgroundThread(
    const Identity& identity,
    mojom::ServicePtrInfo service_info,
    mojom::PIDReceiverRequest pid_receiver_request) {
  mojom::ServicePtr service;
  service.Bind(std::move(service_info));
  service_manager_->RegisterService(identity, std::move(service),
                                    std::move(pid_receiver_request));
}

}  // namespace service_manager
