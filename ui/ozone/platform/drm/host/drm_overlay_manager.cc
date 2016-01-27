// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_overlay_manager.h"

#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/ozone/common/gpu/ozone_gpu_messages.h"
#include "ui/ozone/platform/drm/host/drm_gpu_platform_support_host.h"
#include "ui/ozone/platform/drm/host/drm_overlay_candidates_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"
#include "ui/ozone/public/overlay_candidates_ozone.h"

namespace ui {

DrmOverlayManager::DrmOverlayManager(
    DrmGpuPlatformSupportHost* platform_support_host,
    DrmWindowHostManager* window_manager)
    : sender_(new OverlayCandidatesIPC(platform_support_host, this)),
      core_(new DrmOverlayManagerCore(sender_.get(), window_manager)) {}

DrmOverlayManager::~DrmOverlayManager() {
}

scoped_ptr<OverlayCandidatesOzone> DrmOverlayManager::CreateOverlayCandidates(
    gfx::AcceleratedWidget w) {
  return core_->CreateOverlayCandidates(w);
}

void DrmOverlayManager::OnChannelEstablished(
    int host_id,
    scoped_refptr<base::SingleThreadTaskRunner> send_runner,
    const base::Callback<void(IPC::Message*)>& sender) {
  core_->ResetCache();
}

void DrmOverlayManager::OnChannelDestroyed(int host_id) {}

bool DrmOverlayManager::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(DrmOverlayManager, message)
    IPC_MESSAGE_HANDLER(OzoneHostMsg_OverlayCapabilitiesReceived,
                        OnOverlayResult)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void DrmOverlayManager::ResetCache() {
  core_->ResetCache();
}

void DrmOverlayManager::OnOverlayResult(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& params) {
  core_->GpuSentOverlayResult(widget, params);
}

// TODO(rjkroege): There is a refactoring opportunity in the sender pattern.
DrmOverlayManager::OverlayCandidatesIPC::OverlayCandidatesIPC(
    DrmGpuPlatformSupportHost* platform_support,
    DrmOverlayManager* parent)
    : platform_support_(platform_support), parent_(parent) {}

DrmOverlayManager::OverlayCandidatesIPC::~OverlayCandidatesIPC() {}

void DrmOverlayManager::OverlayCandidatesIPC::UnregisterHandler() {
  platform_support_->UnregisterHandler(parent_);
}

void DrmOverlayManager::OverlayCandidatesIPC::RegisterHandler() {
  platform_support_->RegisterHandler(parent_);
}

bool DrmOverlayManager::OverlayCandidatesIPC::IsConnected() {
  return platform_support_->IsConnected();
}

bool DrmOverlayManager::OverlayCandidatesIPC::CheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& new_params) {
  return platform_support_->Send(
      new OzoneGpuMsg_CheckOverlayCapabilities(widget, new_params));
}

}  // namespace ui
