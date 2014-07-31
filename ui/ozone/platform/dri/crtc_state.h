// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_DRI_CRTC_STATE_H_
#define UI_OZONE_PLATFORM_DRI_CRTC_STATE_H_

#include <stdint.h>

#include "ui/ozone/platform/dri/scoped_drm_types.h"

namespace ui {

class DriWrapper;

// Represents the state of a CRTC.
//
// One CRTC can be paired up with one or more connectors. The simplest
// configuration represents one CRTC driving one monitor, while pairing up a
// CRTC with multiple connectors results in hardware mirroring.
class CrtcState {
 public:
  CrtcState(DriWrapper* drm, uint32_t crtc, uint32_t connector);
  ~CrtcState();

  uint32_t crtc() const { return crtc_; }
  uint32_t connector() const { return connector_; }
  bool is_disabled() const { return is_disabled_; }

  void set_is_disabled(bool state) { is_disabled_ = state; }

 private:
  DriWrapper* drm_;  // Not owned.

  uint32_t crtc_;

  // TODO(dnicoara) Add support for hardware mirroring (multiple connectors).
  uint32_t connector_;

  // Store the state of the CRTC before we took over. Used to restore the CRTC
  // once we no longer need it.
  ScopedDrmCrtcPtr saved_crtc_;

  // Keeps track of the CRTC state. If a surface has been bound, then the value
  // is set to false. Otherwise it is true.
  bool is_disabled_;

  DISALLOW_COPY_AND_ASSIGN(CrtcState);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_DRI_CRTC_STATE_H_
