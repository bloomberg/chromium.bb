// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_DEMO_GL_RENDERER_H_
#define UI_OZONE_DEMO_GL_RENDERER_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "ui/ozone/demo/renderer_base.h"

namespace gfx {
class GLContext;
class GLSurface;
}  // namespace gfx

namespace ui {

class GlRenderer : public RendererBase {
 public:
  GlRenderer(gfx::AcceleratedWidget widget, const gfx::Size& size);
  ~GlRenderer() override;

  // Renderer:
  bool Initialize() override;
  void RenderFrame() override;

 protected:
  virtual scoped_refptr<gfx::GLSurface> CreateSurface();

  scoped_refptr<gfx::GLSurface> surface_;
  scoped_refptr<gfx::GLContext> context_;

  DISALLOW_COPY_AND_ASSIGN(GlRenderer);
};

}  // namespace ui

#endif  // UI_OZONE_DEMO_GL_RENDERER_H_
