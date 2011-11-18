// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_COMPOSITOR_COMPOSITOR_GL_H_
#define UI_GFX_COMPOSITOR_COMPOSITOR_GL_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/gl/scoped_make_current.h"
#include "ui/gfx/size.h"

namespace gfx {
class GLContext;
class GLSurface;
class Rect;
}

namespace ui {

class CompositorGL;
class TextureProgramGL;

// We share resources (such as shaders) between different Compositors via
// GLContext sharing so that we only have to create/destroy them once.
class COMPOSITOR_EXPORT SharedResourcesGL : public SharedResources {
 public:
  static SharedResourcesGL* GetInstance();

  virtual gfx::ScopedMakeCurrent* GetScopedMakeCurrent() OVERRIDE;

  // Creates a context that shares the resources hosted by this singleton.
  scoped_refptr<gfx::GLContext> CreateContext(gfx::GLSurface* surface);

  virtual void* GetDisplay();

  ui::TextureProgramGL* program_no_swizzle() {
    return program_no_swizzle_.get();
  }

  ui::TextureProgramGL* program_swizzle() {
    return program_swizzle_.get();
  }

 private:
  friend struct DefaultSingletonTraits<SharedResourcesGL>;

  SharedResourcesGL();
  virtual ~SharedResourcesGL();

  bool Initialize();
  void Destroy();

  bool initialized_;

  scoped_refptr<gfx::GLContext> context_;
  scoped_refptr<gfx::GLSurface> surface_;

  scoped_ptr<ui::TextureProgramGL> program_swizzle_;
  scoped_ptr<ui::TextureProgramGL> program_no_swizzle_;

  DISALLOW_COPY_AND_ASSIGN(SharedResourcesGL);
};

class COMPOSITOR_EXPORT TextureGL : public Texture {
 public:
  TextureGL();

  virtual void SetCanvas(const SkCanvas& canvas,
                         const gfx::Point& origin,
                         const gfx::Size& overall_size) OVERRIDE;

  virtual void Draw(const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture) OVERRIDE;

 protected:
  explicit TextureGL(const gfx::Size& size);
  virtual ~TextureGL();

  // Actually draws the texture.
  // Only the region defined by draw_bounds will be drawn.
  void DrawInternal(const TextureProgramGL& program,
                    const ui::TextureDrawParams& params,
                    const gfx::Rect& clip_bounds_in_texture);

  unsigned int texture_id_;
  gfx::Size size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TextureGL);
};

class COMPOSITOR_EXPORT CompositorGL : public Compositor {
 public:
  CompositorGL(CompositorDelegate* delegate,
               gfx::AcceleratedWidget widget,
               const gfx::Size& size);
  virtual ~CompositorGL();

  // Overridden from Compositor.
  virtual bool ReadPixels(SkBitmap* bitmap, const gfx::Rect& bounds) OVERRIDE;

  void MakeCurrent();
  gfx::Size GetSize();

 protected:
  virtual void OnWidgetSizeChanged() OVERRIDE;

 private:
  // Overridden from Compositor.
  virtual Texture* CreateTexture() OVERRIDE;
  virtual void OnNotifyStart(bool clear) OVERRIDE;
  virtual void OnNotifyEnd() OVERRIDE;
  virtual void Blur(const gfx::Rect& bounds) OVERRIDE;

  // The GL context used for compositing.
  scoped_refptr<gfx::GLSurface> gl_surface_;
  scoped_refptr<gfx::GLContext> gl_context_;

  // Keep track of whether compositing has started or not.
  bool started_;

  DISALLOW_COPY_AND_ASSIGN(CompositorGL);
};

}  // namespace ui

#endif  // UI_GFX_COMPOSITOR_COMPOSITOR_GL_H_
