// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_STUB_OVERLAY_MANAGER_H_
#define UI_OZONE_COMMON_STUB_OVERLAY_MANAGER_H_

#include "base/macros.h"
#include "ui/ozone/ozone_base_export.h"
#include "ui/ozone/public/overlay_manager_ozone.h"

namespace ui {

class OZONE_BASE_EXPORT StubOverlayManager : public OverlayManagerOzone {
 public:
  StubOverlayManager();
  ~StubOverlayManager() override;

  // OverlayManagerOzone:
  scoped_ptr<OverlayCandidatesOzone> CreateOverlayCandidates(
      gfx::AcceleratedWidget w) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(StubOverlayManager);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_STUB_OVERLAY_MANAGER_H_
