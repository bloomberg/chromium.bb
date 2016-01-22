// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_overlay_candidates_host.h"

#include <stddef.h>

#include <algorithm>

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"

namespace ui {

DrmOverlayCandidatesHost::DrmOverlayCandidatesHost(
    DrmGpuPlatformSupportHost* platform_support,
    DrmWindowHost* window)
    : sender_(new OverlayCandidatesIPC(platform_support, this)),
      core_(new DrmOverlayCandidatesHostCore(sender_.get(), window)) {}

DrmOverlayCandidatesHost::~DrmOverlayCandidatesHost() {
}

void DrmOverlayCandidatesHost::CheckOverlaySupport(
    OverlaySurfaceCandidateList* candidates) {
  core_->CheckOverlaySupport(candidates);
}

void DrmOverlayCandidatesHost::OnChannelEstablished(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::Callback<void(IPC::Message*)>& sender) {
  core_->ResetCache();
}

void DrmOverlayCandidatesHost::OnChannelDestroyed(int host_id) {
}

bool DrmOverlayCandidatesHost::OnMessageReceived(const IPC::Message& message) {
  bool handled = false;
  IPC_BEGIN_MESSAGE_MAP_WITH_PARAM(DrmOverlayCandidatesHost, message, &handled)
  IPC_MESSAGE_FORWARD(OzoneHostMsg_OverlayCapabilitiesReceived, this,
                      DrmOverlayCandidatesHost::OnOverlayResult)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DrmOverlayCandidatesHost::OnOverlayResult(
    bool* handled,
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& params) {
  core_->GpuSentOverlayResult(handled, widget, params);
}

// TODO(rjkroege): There is a refactoring opportunity in the sender pattern.
DrmOverlayCandidatesHost::OverlayCandidatesIPC::OverlayCandidatesIPC(
    DrmGpuPlatformSupportHost* platform_support,
    DrmOverlayCandidatesHost* parent)
    : platform_support_(platform_support), parent_(parent) {}

DrmOverlayCandidatesHost::OverlayCandidatesIPC::~OverlayCandidatesIPC() {}

void DrmOverlayCandidatesHost::OverlayCandidatesIPC::UnregisterHandler() {
  platform_support_->UnregisterHandler(parent_);
}

void DrmOverlayCandidatesHost::OverlayCandidatesIPC::RegisterHandler() {
  platform_support_->RegisterHandler(parent_);
}

bool DrmOverlayCandidatesHost::OverlayCandidatesIPC::IsConnected() {
  return platform_support_->IsConnected();
}

bool DrmOverlayCandidatesHost::OverlayCandidatesIPC::CheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& new_params) {
  return platform_support_->Send(
      new OzoneGpuMsg_CheckOverlayCapabilities(widget, new_params));
}

}  // namespace ui
