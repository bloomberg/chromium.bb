// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_RENDERERS_EXTERNAL_TEXTURED_QUAD_RENDERER_H_
#define CHROME_BROWSER_VR_RENDERERS_EXTERNAL_TEXTURED_QUAD_RENDERER_H_

#include "base/macros.h"
#include "chrome/browser/vr/renderers/textured_quad_renderer.h"

namespace vr {

class ExternalTexturedQuadRenderer : public TexturedQuadRenderer {
 public:
  ExternalTexturedQuadRenderer();
  ~ExternalTexturedQuadRenderer() override;

 private:
  GLenum TextureType() const override;

  DISALLOW_COPY_AND_ASSIGN(ExternalTexturedQuadRenderer);
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_RENDERERS_EXTERNAL_TEXTURED_QUAD_RENDERER_H_
