// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_RENDER_PROCESS_USER_DATA_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_RENDER_PROCESS_USER_DATA_H_

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "chrome/browser/performance_manager/process_resource_coordinator.h"

namespace content {

class RenderProcessHost;

}  // namespace content

namespace performance_manager {

// Attached to RenderProcessHost as user data, associates the RenderProcessHost
// with the Resource Coordinator process node.
class RenderProcessUserData : public base::SupportsUserData::Data {
 public:
  static void CreateForRenderProcessHost(content::RenderProcessHost* host);
  static RenderProcessUserData* GetForRenderProcessHost(
      content::RenderProcessHost* host);

  performance_manager::ProcessResourceCoordinator*
  process_resource_coordinator() {
    return &process_resource_coordinator_;
  }

 private:
  explicit RenderProcessUserData(
      content::RenderProcessHost* render_process_host);

  performance_manager::ProcessResourceCoordinator process_resource_coordinator_;

  DISALLOW_COPY_AND_ASSIGN(RenderProcessUserData);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_RENDER_PROCESS_USER_DATA_H_
