// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_DC_RENDERER_LAYER_PARAMS_H_
#define UI_GL_DC_RENDERER_LAYER_PARAMS_H_

#include <array>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/hdr_metadata.h"
#include "ui/gfx/video_types.h"
#include "ui/gl/gl_export.h"

namespace gl {
class GLImage;
}

namespace ui {

struct GL_EXPORT DCRendererLayerParams {
  DCRendererLayerParams();
  ~DCRendererLayerParams();

  // Images to display in overlay.  There can either be two software video
  // buffers for Y and UV planes, an NV12 hardware video image, or a swap chain
  // image.  If a single image is specified, the second one must be nullptr.
  enum : size_t { kNumImages = 2 };
  using OverlayImages = std::array<scoped_refptr<gl::GLImage>, kNumImages>;
  OverlayImages images;

  // Stacking order relative to backbuffer which has z-order 0.
  int z_order = 1;

  // What part of the content to display in pixels.
  gfx::Rect content_rect;

  // Bounds of the overlay in pre-transform space.
  gfx::Rect quad_rect;

  // 2D flattened transform that maps |quad_rect| to root target space,
  // after applying the |quad_rect.origin()| as an offset.
  gfx::Transform transform;

  // If present, then clip to |clip_rect| in root target space.
  absl::optional<gfx::Rect> clip_rect;

  gfx::ProtectedVideoType protected_video_type =
      gfx::ProtectedVideoType::kClear;

  gfx::HDRMetadata hdr_metadata;
};

}  // namespace ui

#endif  // UI_GL_DC_RENDERER_LAYER_PARAMS_H_
