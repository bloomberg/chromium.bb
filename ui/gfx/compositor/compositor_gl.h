// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_COMPOSITOR_GL_H_
#define UI_GFX_COMPOSITOR_COMPOSITOR_GL_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/size.h"

namespace gfx {
class GLContext;
class GLSurface;
}

namespace ui {

class CompositorGL;
class TextureProgramGL;

class TextureGL : public Texture {
 public:
  explicit TextureGL(CompositorGL* compositor);

  virtual void SetBitmap(const SkBitmap& bitmap,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) OVERRIDE;

  // Draws the texture.
  virtual void Draw(const ui::Transform& transform) OVERRIDE;

 protected:
  TextureGL(CompositorGL* compositor, const gfx::Size& size);
  virtual ~TextureGL();

  // Actually draws the texture.
  void DrawInternal(const TextureProgramGL& program,
                    const ui::Transform& transform);

  unsigned int texture_id_;
  gfx::Size size_;
  CompositorGL* compositor_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextureGL);
};

class CompositorGL : public Compositor {
 public:
  explicit CompositorGL(gfx::AcceleratedWidget widget);
  virtual ~CompositorGL();

  void MakeCurrent();
  gfx::Size GetSize();

  TextureProgramGL* program_no_swizzle();
  TextureProgramGL* program_swizzle();

 private:
  // Overridden from Compositor.
  virtual Texture* CreateTexture() OVERRIDE;
  virtual void NotifyStart() OVERRIDE;
  virtual void NotifyEnd() OVERRIDE;
  virtual void Blur(const gfx::Rect& bounds) OVERRIDE;
  virtual void SchedulePaint() OVERRIDE;

  // Specific to CompositorGL.
  bool InitShaders();

  // The GL context used for compositing.
  scoped_refptr<gfx::GLSurface> gl_surface_;
  scoped_refptr<gfx::GLContext> gl_context_;

  // TODO(wjmaclean): Make these static so they ca be shared in a single
  // context.
  scoped_ptr<TextureProgramGL> program_swizzle_;
  scoped_ptr<TextureProgramGL> program_no_swizzle_;

  gfx::Size size_;

  // Keep track of whether compositing has started or not.
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(CompositorGL);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_COMPOSITOR_GL_H_
