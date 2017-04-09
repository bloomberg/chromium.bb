// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webgl/WebGLContextAttributeHelpers.h"

#include "core/frame/Settings.h"

namespace blink {

WebGLContextAttributes ToWebGLContextAttributes(
    const CanvasContextCreationAttributes& attrs) {
  WebGLContextAttributes result;
  result.setAlpha(attrs.alpha());
  result.setDepth(attrs.depth());
  result.setStencil(attrs.stencil());
  result.setAntialias(attrs.antialias());
  result.setPremultipliedAlpha(attrs.premultipliedAlpha());
  result.setPreserveDrawingBuffer(attrs.preserveDrawingBuffer());
  result.setFailIfMajorPerformanceCaveat(attrs.failIfMajorPerformanceCaveat());
  return result;
}

Platform::ContextAttributes ToPlatformContextAttributes(
    const CanvasContextCreationAttributes& attrs,
    unsigned web_gl_version,
    bool support_own_offscreen_surface) {
  Platform::ContextAttributes result;
  result.fail_if_major_performance_caveat =
      attrs.failIfMajorPerformanceCaveat();
  result.web_gl_version = web_gl_version;
  if (support_own_offscreen_surface) {
    // Only ask for alpha/depth/stencil/antialias if we may be using the default
    // framebuffer. They are not needed for standard offscreen rendering.
    result.support_alpha = attrs.alpha();
    result.support_depth = attrs.depth();
    result.support_stencil = attrs.stencil();
    result.support_antialias = attrs.antialias();
  }
  return result;
}

}  // namespace blink
