// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_CONTEXT_CREATION_ATTRIBUTES_CORE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_CONTEXT_CREATION_ATTRIBUTES_CORE_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"
#include "third_party/blink/renderer/platform/graphics/graphics_types.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

class CORE_EXPORT CanvasContextCreationAttributesCore {
  DISALLOW_NEW();

 public:
  CanvasContextCreationAttributesCore();
  CanvasContextCreationAttributesCore(
      blink::CanvasContextCreationAttributesCore const&);
  virtual ~CanvasContextCreationAttributesCore();

  bool alpha = true;
  bool antialias = true;
  PredefinedColorSpace color_space = PredefinedColorSpace::kSRGB;
  bool depth = true;
  bool fail_if_major_performance_caveat = false;
  bool desynchronized = false;
  CanvasPixelFormat pixel_format = CanvasPixelFormat::kUint8;
  bool premultiplied_alpha = true;
  bool preserve_drawing_buffer = false;
  String power_preference = "default";
  bool stencil = false;
  // Help to determine whether to use GPU or CPU for the canvas. It can only
  // be set to true when the new-canvas-2d-api flag is enabled.
  bool will_read_frequently = false;
  bool xr_compatible = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_CANVAS_CANVAS_CONTEXT_CREATION_ATTRIBUTES_CORE_H_
