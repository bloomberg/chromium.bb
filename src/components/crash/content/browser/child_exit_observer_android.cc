// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/crash/content/browser/child_exit_observer_android.h"

#include <unistd.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "components/crash/content/browser/crash_memory_metrics_collector_android.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_data.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"

using content::BrowserThread;

namespace crash_reporter {

namespace {

base::LazyInstance<ChildExitObserver>::DestructorAtExit g_instance =
    LAZY_INSTANCE_INITIALIZER;

void PopulateTerminationInfo(
    const content::ChildProcessTerminationInfo& content_info,
    ChildExitObserver::TerminationInfo* info) {
  info->binding_state = content_info.binding_state;
  info->was_killed_intentionally_by_browser =
      content_info.was_killed_intentionally_by_browser;
  info->remaining_process_with_strong_binding =
      content_info.remaining_process_with_strong_binding;
  info->remaining_process_with_moderate_binding =
      content_info.remaining_process_with_moderate_binding;
  info->remaining_process_with_waived_binding =
      content_info.remaining_process_with_waived_binding;
  info->was_oom_protected_status =
      content_info.status == base::TERMINATION_STATUS_OOM_PROTECTED;
}

}  // namespace

ChildExitObserver::TerminationInfo::TerminationInfo() = default;
ChildExitObserver::TerminationInfo::TerminationInfo(
    const TerminationInfo& other) = default;
ChildExitObserver::TerminationInfo& ChildExitObserver::TerminationInfo::
operator=(const TerminationInfo& other) = default;

// static
void ChildExitObserver::Create() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // If this DCHECK fails in a unit test then a previously executing
  // test that makes use of ChildExitObserver forgot to create a
  // ShadowingAtExitManager.
  DCHECK(!g_instance.IsCreated());
  g_instance.Get();
}

// static
ChildExitObserver* ChildExitObserver::GetInstance() {
  DCHECK(g_instance.IsCreated());
  return g_instance.Pointer();
}

ChildExitObserver::ChildExitObserver()
    : notification_registrar_(),
      registered_clients_lock_(),
      registered_clients_(),
      process_host_id_to_pid_(),
      browser_child_process_info_(),
      crash_signals_lock_(),
      child_pid_to_crash_signal_(),
      scoped_observer_(this) {
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CREATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                              content::NotificationService::AllSources());
  notification_registrar_.Add(this,
                              content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
                              content::NotificationService::AllSources());
  BrowserChildProcessObserver::Add(this);
  scoped_observer_.Add(crashpad::CrashHandlerHost::Get());
}

ChildExitObserver::~ChildExitObserver() {
  BrowserChildProcessObserver::Remove(this);
}

void ChildExitObserver::RegisterClient(std::unique_ptr<Client> client) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::AutoLock auto_lock(registered_clients_lock_);
  registered_clients_.push_back(std::move(client));
}

void ChildExitObserver::ChildReceivedCrashSignal(base::ProcessId pid,
                                                 int signo) {
  base::AutoLock lock(crash_signals_lock_);
  bool result =
      child_pid_to_crash_signal_.insert(std::make_pair(pid, signo)).second;
  DCHECK(result);
}

void ChildExitObserver::OnChildExit(TerminationInfo* info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  {
    base::AutoLock lock(crash_signals_lock_);
    auto pid_and_signal = child_pid_to_crash_signal_.find(info->pid);
    if (pid_and_signal != child_pid_to_crash_signal_.end()) {
      info->crash_signo = pid_and_signal->second;
      child_pid_to_crash_signal_.erase(pid_and_signal);
    }
  }

  std::vector<Client*> registered_clients_copy;
  {
    base::AutoLock auto_lock(registered_clients_lock_);
    for (auto& client : registered_clients_)
      registered_clients_copy.push_back(client.get());
  }
  for (auto* client : registered_clients_copy) {
    client->OnChildExit(*info);
  }
}

void ChildExitObserver::BrowserChildProcessHostDisconnected(
    const content::ChildProcessData& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  TerminationInfo info;
  auto it = browser_child_process_info_.find(data.id);
  if (it != browser_child_process_info_.end()) {
    info = it->second;
    browser_child_process_info_.erase(it);
  } else {
    info.process_host_id = data.id;
    if (data.GetProcess().IsValid())
      info.pid = data.GetProcess().Pid();
    info.process_type = static_cast<content::ProcessType>(data.process_type);
    info.app_state = base::android::ApplicationStatusListener::GetState();
    info.normal_termination = true;
  }
  OnChildExit(&info);
}

void ChildExitObserver::BrowserChildProcessKilled(
    const content::ChildProcessData& data,
    const content::ChildProcessTerminationInfo& content_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(!base::ContainsKey(browser_child_process_info_, data.id));
  TerminationInfo info;
  info.process_host_id = data.id;
  info.pid = data.GetProcess().Pid();
  info.process_type = static_cast<content::ProcessType>(data.process_type);
  info.app_state = base::android::ApplicationStatusListener::GetState();
  info.normal_termination = content_info.clean_exit;
  PopulateTerminationInfo(content_info, &info);
  browser_child_process_info_.emplace(data.id, info);
  // Subsequent BrowserChildProcessHostDisconnected will call OnChildExit.
}

void ChildExitObserver::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  content::RenderProcessHost* rph =
      content::Source<content::RenderProcessHost>(source).ptr();
  TerminationInfo info;
  info.process_host_id = rph->GetID();
  info.pid = rph->GetProcess().Handle();
  info.process_type = content::PROCESS_TYPE_RENDERER;
  info.app_state = base::android::APPLICATION_STATE_UNKNOWN;
  info.renderer_has_visible_clients = rph->VisibleClientCount() > 0;
  info.renderer_was_subframe = rph->GetFrameDepth() > 0u;
  CrashMemoryMetricsCollector* collector =
      CrashMemoryMetricsCollector::GetFromRenderProcessHost(rph);

  // CrashMemoryMetircsCollector is created in chrome_content_browser_client,
  // and does not exist in non-chrome platforms such as android webview /
  // chromecast.
  if (collector) {
    // SharedMemory creation / Map() might fail.
    DCHECK(collector->MemoryMetrics());
    info.blink_oom_metrics = *collector->MemoryMetrics();
  }
  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_TERMINATED: {
      // NOTIFICATION_RENDERER_PROCESS_TERMINATED is sent when the renderer
      // process is cleanly shutdown.
      info.normal_termination = true;
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED: {
      // We do not care about android fast shutdowns as it is a known case where
      // the renderer is intentionally killed when we are done with it.
      info.normal_termination = rph->FastShutdownStarted();
      info.app_state = base::android::ApplicationStatusListener::GetState();
      const auto& content_info =
          *content::Details<content::ChildProcessTerminationInfo>(details)
               .ptr();
      PopulateTerminationInfo(content_info, &info);
      break;
    }
    case content::NOTIFICATION_RENDERER_PROCESS_CREATED: {
      // The child process pid isn't available when process is gone, keep a
      // mapping between process_host_id and pid, so we can find it later.
      process_host_id_to_pid_[rph->GetID()] = rph->GetProcess().Handle();
      return;
    }
    default:
      NOTREACHED();
      return;
  }
  const auto& iter = process_host_id_to_pid_.find(rph->GetID());
  // NOTIFICATION_RENDERER_PROCESS_CLOSED corresponds to death of an underlying
  // RenderProcess. NOTIFICATION_RENDERER_PROCESS_TERMINATED corresponds to when
  // the RenderProcessHost's lifetime is ending. Ideally, we'd only listen to
  // the former, but if the RenderProcessHost is destroyed before the
  // RenderProcess, then the former is never sent.
  if (iter == process_host_id_to_pid_.end()) {
    return;
  }
  if (info.pid == base::kNullProcessHandle) {
    info.pid = iter->second;
  }
  process_host_id_to_pid_.erase(iter);
  OnChildExit(&info);
}

}  // namespace crash_reporter
