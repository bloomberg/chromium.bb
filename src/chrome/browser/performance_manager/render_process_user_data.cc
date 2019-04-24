// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/render_process_user_data.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/performance_manager/performance_manager.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_process_host_observer.h"
#include "content/public/common/content_switches.h"
#include "services/service_manager/public/cpp/connector.h"

namespace performance_manager {

namespace {

const void* const kRenderProcessUserDataKey = &kRenderProcessUserDataKey;

class RenderProcessLifetimeWatcher : public content::RenderProcessHostObserver {
 public:
  // RenderProcessHostObserver implementation.
  void RenderProcessReady(content::RenderProcessHost* host) override {
    RenderProcessUserData* user_data =
        RenderProcessUserData::GetForRenderProcessHost(host);

    // TODO(siggi): Rename OnProcessLaunched->OnProcessReady.
    user_data->process_resource_coordinator()->OnProcessLaunched(
        host->GetProcess());
  }

  void RenderProcessExited(
      content::RenderProcessHost* host,
      const content::ChildProcessTerminationInfo& info) override {
    RenderProcessUserData* user_data =
        RenderProcessUserData::GetForRenderProcessHost(host);

    user_data->process_resource_coordinator()->SetProcessExitStatus(
        info.exit_code);
  }

  void RenderProcessHostDestroyed(content::RenderProcessHost* host) override {
    delete this;
  }
};

}  // namespace

RenderProcessUserData::RenderProcessUserData(
    content::RenderProcessHost* render_process_host)
    : process_resource_coordinator_(
          performance_manager::PerformanceManager::GetInstance()) {
  // The process itself shouldn't have been created at this point.
  DCHECK(!render_process_host->GetProcess().IsValid() ||
         base::CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kSingleProcess));
  render_process_host->AddObserver(new RenderProcessLifetimeWatcher);
}

void RenderProcessUserData::CreateForRenderProcessHost(
    content::RenderProcessHost* host) {
  std::unique_ptr<RenderProcessUserData> user_data =
      base::WrapUnique(new RenderProcessUserData(host));

  host->SetUserData(kRenderProcessUserDataKey, std::move(user_data));
}

RenderProcessUserData* RenderProcessUserData::GetForRenderProcessHost(
    content::RenderProcessHost* host) {
  return static_cast<RenderProcessUserData*>(
      host->GetUserData(kRenderProcessUserDataKey));
}

}  // namespace performance_manager
