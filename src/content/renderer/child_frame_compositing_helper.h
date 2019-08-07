// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
#define CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "cc/layers/content_layer_client.h"
#include "cc/layers/surface_layer.h"
#include "components/viz/common/surfaces/surface_id.h"
#include "content/common/content_export.h"

namespace cc {
class PictureLayer;
}

namespace gfx {
class Size;
}

namespace viz {
class SurfaceId;
}

namespace content {

class ChildFrameCompositor;

class CONTENT_EXPORT ChildFrameCompositingHelper
    : public cc::ContentLayerClient {
 public:
  explicit ChildFrameCompositingHelper(
      ChildFrameCompositor* child_frame_compositor);
  ~ChildFrameCompositingHelper() override;

  void SetSurfaceId(const viz::SurfaceId& surface_id,
                    const gfx::Size& frame_size_in_dip,
                    const cc::DeadlinePolicy& deadline);
  void UpdateVisibility(bool visible);
  void ChildFrameGone(const gfx::Size& frame_size_in_dip,
                      float device_scale_factor);

  const viz::SurfaceId& surface_id() const { return surface_id_; }

 private:
  // cc::ContentLayerClient implementation. Called from the cc::PictureLayer
  // created for the crashed child frame to display the sad image.
  gfx::Rect PaintableRegion() override;
  scoped_refptr<cc::DisplayItemList> PaintContentsToDisplayList(
      PaintingControlSetting) override;
  bool FillsBoundsCompletely() const override;
  size_t GetApproximateUnsharedMemoryUsage() const override;

  ChildFrameCompositor* const child_frame_compositor_;
  viz::SurfaceId surface_id_;
  scoped_refptr<cc::SurfaceLayer> surface_layer_;
  scoped_refptr<cc::PictureLayer> crash_ui_layer_;
  float device_scale_factor_ = 1.f;

  DISALLOW_COPY_AND_ASSIGN(ChildFrameCompositingHelper);
};

}  // namespace content

#endif  // CONTENT_RENDERER_CHILD_FRAME_COMPOSITING_HELPER_H_
