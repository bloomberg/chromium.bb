// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_vsync_provider.h"

#include "ui/ozone/platform/drm/gpu/drm_window.h"

namespace ui {

DrmVSyncProvider::DrmVSyncProvider(DrmWindow* window) : window_(window) {
}

DrmVSyncProvider::~DrmVSyncProvider() {
}

void DrmVSyncProvider::GetVSyncParameters(const UpdateVSyncCallback& callback) {
  window_->GetVSyncParameters(callback);
}

}  // namespace ui
