// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GFX_SURFACE_ACCELERATED_SURFACE_MAC_H_
#define UI_GFX_SURFACE_ACCELERATED_SURFACE_MAC_H_
#pragma once

#include <CoreFoundation/CoreFoundation.h>

#include "base/bind.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/gl/gl_context.h"
#include "ui/gfx/gl/gl_surface.h"
#include "ui/gfx/gl/gpu_preference.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/surface/surface_export.h"
#include "ui/gfx/surface/transport_dib.h"

// Should not include GL headers in a header file. Forward declare these types
// instead.
typedef struct _CGLContextObject* CGLContextObj;
typedef unsigned int GLenum;
typedef unsigned int GLuint;

namespace gfx {
class Rect;
}

// Encapsulates an accelerated GL surface that can be shared across processes
// on systems that support it (10.6 and above). For systems that do not, it
// uses a regular dib. There will either be an IOSurface or a TransportDIB,
// never both.

class SURFACE_EXPORT AcceleratedSurface {
 public:
  AcceleratedSurface();
  virtual ~AcceleratedSurface();

  // Set up internal buffers. |share_context|, if non-NULL, is a context
  // with which the internally created OpenGL context shares textures and
  // other resources. |allocate_fbo| indicates whether or not this surface
  // should allocate an offscreen frame buffer object (FBO) internally. If
  // not, then the user is expected to allocate one. NOTE that allocating
  // an FBO internally does NOT work properly with client code which uses
  // OpenGL (i.e., via GLES2 command buffers), because the GLES2
  // implementation does not know to bind the accelerated surface's
  // internal FBO when the default FBO is bound. |gpu_preference| indicates
  // the GPU preference for the internally allocated GLContext. If
  // |share_context| is non-NULL, then on platforms supporting dual GPUs,
  // its GPU preference must match the passed one. Returns false upon
  // failure.
  bool Initialize(gfx::GLContext* share_context,
                  bool allocate_fbo,
                  gfx::GpuPreference gpu_preference);
  // Tear down. Must be called before destructor to prevent leaks.
  void Destroy();

  // These methods are used only once the accelerated surface is initialized.

  // Sets the accelerated surface to the given size, creating a new one if
  // the height or width changes. Returns a unique id of the IOSurface to
  // which the surface is bound, or 0 if no changes were made or an error
  // occurred. MakeCurrent() will have been called on the new surface.
  uint32 SetSurfaceSize(const gfx::Size& size);

  // Returns the id of this surface's IOSurface, or 0 for
  // transport DIB surfaces.
  uint32 GetSurfaceId();

  // Sets the GL context to be the current one for drawing. Returns true if
  // it succeeded.
  bool MakeCurrent();
  // Clear the surface to be transparent. Assumes the caller has already called
  // MakeCurrent().
  void Clear(const gfx::Rect& rect);
  // Call after making changes to the surface which require a visual update.
  // Makes the rendering show up in other processes. Assumes the caller has
  // already called MakeCurrent().
  //
  // If this AcceleratedSurface is configured with its own FBO, then
  // this call causes the color buffer to be transmitted. Otherwise,
  // it causes the frame buffer of the current GL context to be copied
  // either into an internal texture via glCopyTexSubImage2D or into a
  // TransportDIB via glReadPixels.
  //
  // The size of the rectangle copied is the size last specified via
  // SetSurfaceSize.  If another GL context than the one this
  // AcceleratedSurface contains is responsible for the production of
  // the pixels, then when this entry point is called, the color
  // buffer must be in a state where a glCopyTexSubImage2D or
  // glReadPixels is legal. (For example, if using multisampled FBOs,
  // the FBO must have been resolved into a non-multisampled color
  // texture.) Additionally, in this situation, the contexts must
  // share server-side GL objects, so that this AcceleratedSurface's
  // texture is a legal name in the namespace of the current context.
  void SwapBuffers();

  CGLContextObj context() {
    return static_cast<CGLContextObj>(gl_context_->GetHandle());
  }

  // These methods are only used when there is a transport DIB.

  // Sets the transport DIB to the given size, creating a new one if the
  // height or width changes. Returns a handle to the new DIB, or a default
  // handle if no changes were made. Assumes the caller has already called
  // MakeCurrent().
  TransportDIB::Handle SetTransportDIBSize(const gfx::Size& size);
  // Sets the methods to use for allocating and freeing memory for the
  // transport DIB.
  void SetTransportDIBAllocAndFree(
      const base::Callback<void(size_t, TransportDIB::Handle*)>& allocator,
      const base::Callback<void(TransportDIB::Id)>& deallocator);

  // Get the accelerated surface size.
  gfx::Size GetSize() const { return surface_size_; }

 private:
  // Helper function to generate names for the backing texture, render buffers
  // and FBO.  On return, the resulting buffer names can be attached to |fbo_|.
  // |target| is the target type for the color buffer.
  void AllocateRenderBuffers(GLenum target, const gfx::Size& size);

  // Helper function to attach the buffers previously allocated by a call to
  // AllocateRenderBuffers().  On return, |fbo_| can be used for
  // rendering.  |target| must be the same value as used in the call to
  // AllocateRenderBuffers().  Returns |true| if the resulting framebuffer
  // object is valid.
  bool SetupFrameBufferObject(GLenum target);

  gfx::Size ClampToValidDimensions(const gfx::Size& size);

  // The OpenGL context, and pbuffer drawable, used to transfer data
  // to the shared region (IOSurface or TransportDIB). Strictly
  // speaking, we do not need to allocate a GL context all of the
  // time. We only need one if (a) we are using the IOSurface code
  // path, or (b) if we are allocating an FBO internally.
  scoped_refptr<gfx::GLSurface> gl_surface_;
  scoped_refptr<gfx::GLContext> gl_context_;
  // Either |io_surface_| or |transport_dib_| is a valid pointer, but not both.
  // |io_surface_| is non-NULL if the IOSurface APIs are supported (Mac OS X
  // 10.6 and later).
  // TODO(dspringer,kbr): Should the GPU backing store be encapsulated in its
  // own class so all this implementation detail is hidden?
  base::mac::ScopedCFTypeRef<CFTypeRef> io_surface_;

  // The id of |io_surface_| or 0 if that's NULL.
  uint32 io_surface_id_;

  // TODO(dspringer): If we end up keeping this TransportDIB mechanism, this
  // should really be a scoped_ptr_malloc<>, with a deallocate functor that
  // runs |dib_free_callback_|.  I was not able to figure out how to
  // make this work (or even compile).
  scoped_ptr<TransportDIB> transport_dib_;
  gfx::Size surface_size_;
  // It's important to avoid allocating zero-width or zero-height
  // IOSurfaces and textures on the Mac, so we clamp each to a minimum
  // of 1. This is the real size of the surface; surface_size_ is what
  // the user requested.
  gfx::Size real_surface_size_;
  // TODO(kbr): the FBO management should not be in this class at all.
  // However, if it is factored out, care needs to be taken to not
  // introduce another copy of the color data on the GPU; the direct
  // binding of the internal texture to the IOSurface saves a copy.
  bool allocate_fbo_;
  // If the IOSurface code path is being used, then this texture
  // object is always allocated. Otherwise, it is only allocated if
  // the user requests an FBO be allocated.
  GLuint texture_;
  // The FBO and renderbuffer are only allocated if allocate_fbo_ is
  // true.
  GLuint fbo_;
  GLuint depth_stencil_renderbuffer_;
  // Allocate a TransportDIB in the renderer.
  base::Callback<void(size_t, TransportDIB::Handle*)> dib_alloc_callback_;
  base::Callback<void(TransportDIB::Id)> dib_free_callback_;
};

#endif  // UI_GFX_SURFACE_ACCELERATED_SURFACE_MAC_H_
