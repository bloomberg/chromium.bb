// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_OZONE_OVERLAY_CANDIDATES_OZONE_H_
#define UI_GFX_OZONE_OVERLAY_CANDIDATES_OZONE_H_

#include <vector>

#include "base/basictypes.h"
#include "ui/gfx/gfx_export.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"
#include "ui/gfx/rect_f.h"

namespace gfx {

// This class can be used to answer questions about possible overlay
// configurations for a particular output device. We get an instance of this
// class from SurfaceFactoryOzone given an AcceleratedWidget.
class GFX_EXPORT OverlayCandidatesOzone {
 public:
  struct OverlaySurfaceCandidate {
    OverlaySurfaceCandidate();
    ~OverlaySurfaceCandidate();

    // Transformation to apply to layer during composition.
    gfx::OverlayTransform transform;
    // Format of the buffer to composite.
    SurfaceFactoryOzone::BufferFormat format;
    // Rect on the display to position the overlay to.
    gfx::Rect display_rect;
    // Crop within the buffer to be placed inside |display_rect|.
    gfx::RectF crop_rect;
    // Stacking order of the overlay plane relative to the main surface,
    // which is 0. Signed to allow for "underlays".
    int plane_z_order;

    // To be modified by the implementer if this candidate can go into
    // an overlay.
    bool overlay_handled;
  };

  typedef std::vector<OverlaySurfaceCandidate> OverlaySurfaceCandidateList;

  // A list of possible overlay candidates is presented to this function.
  // The expected result is that those candidates that can be in a separate
  // plane are marked with |overlay_handled| set to true, otherwise they are
  // to be tranditionally composited.
  virtual void CheckOverlaySupport(OverlaySurfaceCandidateList* surfaces);

  virtual ~OverlayCandidatesOzone();
};

}  // namespace gfx

#endif  // UI_GFX_OZONE_OVERLAY_CANDIDATES_OZONE_H_
