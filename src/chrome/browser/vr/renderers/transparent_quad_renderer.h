// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_TRANSPARENT_QUAD_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_TRANSPARENT_QUAD_RENDERER_H_

#include "base/macros.h"
#include "chrome/browser/vr/renderers/textured_quad_renderer.h"

namespace vr {

class TransparentQuadRenderer : public TexturedQuadRenderer {
 public:
  TransparentQuadRenderer();
  ~TransparentQuadRenderer() override;

  DISALLOW_COPY_AND_ASSIGN(TransparentQuadRenderer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDERERS_TRANSPARENT_QUAD_RENDERER_H_
