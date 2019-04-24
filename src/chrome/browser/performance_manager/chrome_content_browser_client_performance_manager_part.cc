// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/chrome_content_browser_client_performance_manager_part.h"

#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/performance_manager/render_process_user_data.h"

namespace {

void BindProcessPerformanceManager(
    content::RenderProcessHost* render_process_host,
    resource_coordinator::mojom::ProcessCoordinationUnitRequest request) {
  performance_manager::RenderProcessUserData* user_data =
      performance_manager::RenderProcessUserData::GetForRenderProcessHost(
          render_process_host);

  user_data->process_resource_coordinator()->AddBinding(std::move(request));
}

}  // namespace

ChromeContentBrowserClientPerformanceManagerPart::
    ChromeContentBrowserClientPerformanceManagerPart() = default;
ChromeContentBrowserClientPerformanceManagerPart::
    ~ChromeContentBrowserClientPerformanceManagerPart() = default;

void ChromeContentBrowserClientPerformanceManagerPart::
    ExposeInterfacesToRenderer(
        service_manager::BinderRegistry* registry,
        blink::AssociatedInterfaceRegistry* associated_registry,
        content::RenderProcessHost* render_process_host) {
  registry->AddInterface(
      base::BindRepeating(&BindProcessPerformanceManager,
                          base::Unretained(render_process_host)),
      base::SequencedTaskRunnerHandle::Get());

  // When a RenderFrameHost is "resurrected" with a new process  it will already
  // have user data attached. This will happen on renderer crash.
  if (!performance_manager::RenderProcessUserData::GetForRenderProcessHost(
          render_process_host)) {
    performance_manager::RenderProcessUserData::CreateForRenderProcessHost(
        render_process_host);
  }
}
