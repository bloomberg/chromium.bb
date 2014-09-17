// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/crtc_state.h"

#include "ui/ozone/platform/dri/dri_wrapper.h"

namespace ui {

CrtcState::CrtcState(DriWrapper* drm,
                     uint32_t crtc,
                     uint32_t connector)
    : drm_(drm),
      crtc_(crtc),
      connector_(connector),
      saved_crtc_(drm->GetCrtc(crtc)),
      is_disabled_(true) {}

CrtcState::~CrtcState() {
  if (!is_disabled_) {
    drm_->SetCrtc(saved_crtc_.get(), std::vector<uint32_t>(1, connector_));
    drm_->SetCursor(crtc_, 0, gfx::Size());
  }
}

}  // namespace ui
