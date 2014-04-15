// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/setup/daemon_controller.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/auto_thread_task_runner.h"

namespace remoting {

// Name of the Daemon Controller's worker thread.
const char kDaemonControllerThreadName[] = "Daemon Controller thread";

DaemonController::DaemonController(scoped_ptr<Delegate> delegate)
    : caller_task_runner_(base::ThreadTaskRunnerHandle::Get()),
      delegate_(delegate.Pass()) {
  // Launch the delegate thread.
  delegate_thread_.reset(new AutoThread(kDaemonControllerThreadName));
#if defined(OS_WIN)
  delegate_thread_->SetComInitType(AutoThread::COM_INIT_STA);
  delegate_task_runner_ =
      delegate_thread_->StartWithType(base::MessageLoop::TYPE_UI);
#else
  delegate_task_runner_ =
      delegate_thread_->StartWithType(base::MessageLoop::TYPE_DEFAULT);
#endif
}

DaemonController::State DaemonController::GetState() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());
  return delegate_->GetState();
}

void DaemonController::GetConfig(const GetConfigCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::GetConfigCallback wrapped_done = base::Bind(
      &DaemonController::InvokeConfigCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoGetConfig, this, wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::InstallHost(const CompletionCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::CompletionCallback wrapped_done = base::Bind(
      &DaemonController::InvokeCompletionCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoInstallHost, this, wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::SetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    bool consent,
    const CompletionCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::CompletionCallback wrapped_done = base::Bind(
      &DaemonController::InvokeCompletionCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoSetConfigAndStart, this, base::Passed(&config),
      consent, wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::UpdateConfig(scoped_ptr<base::DictionaryValue> config,
                                    const CompletionCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::CompletionCallback wrapped_done = base::Bind(
      &DaemonController::InvokeCompletionCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoUpdateConfig, this, base::Passed(&config),
      wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::Stop(const CompletionCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::CompletionCallback wrapped_done = base::Bind(
      &DaemonController::InvokeCompletionCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoStop, this, wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::SetWindow(void* window_handle) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  base::Closure done = base::Bind(&DaemonController::ScheduleNext, this);
  base::Closure request = base::Bind(
      &DaemonController::DoSetWindow, this, window_handle, done);
  ServiceOrQueueRequest(request);
}

void DaemonController::GetVersion(const GetVersionCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::GetVersionCallback wrapped_done = base::Bind(
      &DaemonController::InvokeVersionCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoGetVersion, this, wrapped_done);
  ServiceOrQueueRequest(request);
}

void DaemonController::GetUsageStatsConsent(
    const GetUsageStatsConsentCallback& done) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  DaemonController::GetUsageStatsConsentCallback wrapped_done = base::Bind(
      &DaemonController::InvokeConsentCallbackAndScheduleNext, this, done);
  base::Closure request = base::Bind(
      &DaemonController::DoGetUsageStatsConsent, this, wrapped_done);
  ServiceOrQueueRequest(request);
}

DaemonController::~DaemonController() {
  // Make sure |delegate_| is deleted on the background thread.
  delegate_task_runner_->DeleteSoon(FROM_HERE, delegate_.release());

  // Stop the thread.
  delegate_task_runner_ = NULL;
  caller_task_runner_->DeleteSoon(FROM_HERE, delegate_thread_.release());
}

void DaemonController::DoGetConfig(const GetConfigCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  scoped_ptr<base::DictionaryValue> config = delegate_->GetConfig();
  caller_task_runner_->PostTask(FROM_HERE,
                                base::Bind(done, base::Passed(&config)));
}

void DaemonController::DoInstallHost(const CompletionCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->InstallHost(done);
}

void DaemonController::DoSetConfigAndStart(
    scoped_ptr<base::DictionaryValue> config,
    bool consent,
    const CompletionCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->SetConfigAndStart(config.Pass(), consent, done);
}

void DaemonController::DoUpdateConfig(
    scoped_ptr<base::DictionaryValue> config,
    const CompletionCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->UpdateConfig(config.Pass(), done);
}

void DaemonController::DoStop(const CompletionCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->Stop(done);
}

void DaemonController::DoSetWindow(void* window_handle,
                                   const base::Closure& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  delegate_->SetWindow(window_handle);
  caller_task_runner_->PostTask(FROM_HERE, done);
}

void DaemonController::DoGetVersion(const GetVersionCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  std::string version = delegate_->GetVersion();
  caller_task_runner_->PostTask(FROM_HERE, base::Bind(done, version));
}

void DaemonController::DoGetUsageStatsConsent(
    const GetUsageStatsConsentCallback& done) {
  DCHECK(delegate_task_runner_->BelongsToCurrentThread());

  DaemonController::UsageStatsConsent consent =
      delegate_->GetUsageStatsConsent();
  caller_task_runner_->PostTask(FROM_HERE, base::Bind(done, consent));
}

void DaemonController::InvokeCompletionCallbackAndScheduleNext(
    const CompletionCallback& done,
    AsyncResult result) {
  if (!caller_task_runner_->BelongsToCurrentThread()) {
    caller_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&DaemonController::InvokeCompletionCallbackAndScheduleNext,
                   this, done, result));
    return;
  }

  done.Run(result);
  ScheduleNext();
}

void DaemonController::InvokeConfigCallbackAndScheduleNext(
    const GetConfigCallback& done,
    scoped_ptr<base::DictionaryValue> config) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  done.Run(config.Pass());
  ScheduleNext();
}

void DaemonController::InvokeConsentCallbackAndScheduleNext(
    const GetUsageStatsConsentCallback& done,
    const UsageStatsConsent& consent) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  done.Run(consent);
  ScheduleNext();
}

void DaemonController::InvokeVersionCallbackAndScheduleNext(
    const GetVersionCallback& done,
    const std::string& version) {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  done.Run(version);
  ScheduleNext();
}

void DaemonController::ScheduleNext() {
  DCHECK(caller_task_runner_->BelongsToCurrentThread());

  pending_requests_.pop();
  ServiceNextRequest();
}

void DaemonController::ServiceOrQueueRequest(const base::Closure& request) {
  bool servicing_request = !pending_requests_.empty();
  pending_requests_.push(request);
  if (!servicing_request)
    ServiceNextRequest();
}

void DaemonController::ServiceNextRequest() {
  if (!pending_requests_.empty())
    delegate_task_runner_->PostTask(FROM_HERE, pending_requests_.front());
}

}  // namespace remoting
