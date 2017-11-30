// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_H_
#define UI_GL_GL_SURFACE_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "build/build_config.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_format.h"

namespace gfx {
class VSyncProvider;
}

namespace ui {
struct CARendererLayerParams;
struct DCRendererLayerParams;
}

namespace gl {

class GLContext;

// Encapsulates a surface that can be rendered to with GL, hiding platform
// specific management.
class GL_EXPORT GLSurface : public base::RefCounted<GLSurface> {
 public:
  GLSurface();

  // Non-virtual initialization, this always calls Initialize with a
  // default GLSurfaceFormat. Subclasses should override the format-
  // specific Initialize method below and interpret the default format
  // as appropriate.
  bool Initialize();

  // (Re)create the surface. TODO(apatrick): This is an ugly hack to allow the
  // EGL surface associated to be recreated without destroying the associated
  // context. The implementation of this function for other GLSurface derived
  // classes is in a pending changelist.
  virtual bool Initialize(GLSurfaceFormat format);

  // Destroys the surface.
  virtual void Destroy() = 0;

  // Color spaces that can be dynamically specified to the surface when resized.
  enum class ColorSpace {
    UNSPECIFIED,
    SRGB,
    DISPLAY_P3,
    SCRGB_LINEAR,
  };

  virtual bool Resize(const gfx::Size& size,
                      float scale_factor,
                      ColorSpace color_space,
                      bool has_alpha);

  // Recreate the surface without changing the size.
  virtual bool Recreate();

  // Unschedule the CommandExecutor and return true to abort the processing of
  // a GL draw call to this surface and defer it until the CommandExecutor is
  // rescheduled.
  virtual bool DeferDraws();

  // Returns true if this surface is offscreen.
  virtual bool IsOffscreen() = 0;

  // The callback is for receiving presentation feedback from |SwapBuffers|,
  // |PostSubBuffer|, |CommitOverlayPlanes|, etc. If
  // |SupportsPresentationCallback()| returns true, it is guarantee that the
  // |PresentationCallback| will be called.
  // See |PresentationFeedback| for detail.
  using PresentationCallback =
      base::Callback<void(const gfx::PresentationFeedback& feedback)>;

  // Swaps front and back buffers. This has no effect for off-screen
  // contexts.
  virtual gfx::SwapResult SwapBuffers(const PresentationCallback& callback) = 0;

  // Get the size of the surface.
  virtual gfx::Size GetSize() = 0;

  // Get the underlying platform specific surface "handle".
  virtual void* GetHandle() = 0;

  // Returns whether or not the surface supports the |PresentationCallback|
  // of |SwapBuffers|, |SwapBuffersAsync|, |SwapBuffersWithBounds|,
  // |PostSubBuffer|, |PostSubBufferAsync|, |CommitOverlayPlanes|,
  // |CommitOverlayPlanesAsync|, etc. If returns false, the
  // |PresentationCallback| will never be called.
  virtual bool SupportsPresentationCallback();

  // Returns whether or not the surface supports SwapBuffersWithBounds
  virtual bool SupportsSwapBuffersWithBounds();

  // Returns whether or not the surface supports PostSubBuffer.
  virtual bool SupportsPostSubBuffer();

  // Returns whether or not the surface supports CommitOverlayPlanes.
  virtual bool SupportsCommitOverlayPlanes();

  // Returns whether SwapBuffersAsync() is supported.
  virtual bool SupportsAsyncSwap();

  // Returns the internal frame buffer object name if the surface is backed by
  // FBO. Otherwise returns 0.
  virtual unsigned int GetBackingFramebufferObject();

  using SwapCompletionCallback = base::Callback<void(gfx::SwapResult)>;
  // Swaps front and back buffers. This has no effect for off-screen
  // contexts. On some platforms, we want to send SwapBufferAck only after the
  // surface is displayed on screen. The callback can be used to delay sending
  // SwapBufferAck till that data is available. The callback should be run on
  // the calling thread (i.e. same thread SwapBuffersAsync is called)
  virtual void SwapBuffersAsync(
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback);

  // Swap buffers with content bounds.
  virtual gfx::SwapResult SwapBuffersWithBounds(
      const std::vector<gfx::Rect>& rects,
      const PresentationCallback& callback);

  // Copy part of the backbuffer to the frontbuffer.
  virtual gfx::SwapResult PostSubBuffer(int x,
                                        int y,
                                        int width,
                                        int height,
                                        const PresentationCallback& callback);

  // Copy part of the backbuffer to the frontbuffer. On some platforms, we want
  // to send SwapBufferAck only after the surface is displayed on screen. The
  // callback can be used to delay sending SwapBufferAck till that data is
  // available. The callback should be run on the calling thread (i.e. same
  // thread PostSubBufferAsync is called)
  virtual void PostSubBufferAsync(
      int x,
      int y,
      int width,
      int height,
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback);

  // Show overlay planes but don't swap the front and back buffers. This acts
  // like SwapBuffers from the point of view of the client, but is cheaper when
  // overlays account for all the damage.
  virtual gfx::SwapResult CommitOverlayPlanes(
      const PresentationCallback& callback);

  // Show overlay planes but don't swap the front and back buffers. On some
  // platforms, we want to send SwapBufferAck only after the overlays are
  // displayed on screen. The callback can be used to delay sending
  // SwapBufferAck till that data is available. The callback should be run on
  // the calling thread (i.e. same thread CommitOverlayPlanesAsync is called).
  virtual void CommitOverlayPlanesAsync(
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback);

  // Called after a context is made current with this surface. Returns false
  // on error.
  virtual bool OnMakeCurrent(GLContext* context);

  // Used for explicit buffer management.
  virtual bool SetBackbufferAllocation(bool allocated);
  virtual void SetFrontbufferAllocation(bool allocated);

  // Get a handle used to share the surface with another process. Returns null
  // if this is not possible.
  virtual void* GetShareHandle();

  // Get the platform specific display on which this surface resides, if
  // available.
  virtual void* GetDisplay();

  // Get the platfrom specific configuration for this surface, if available.
  virtual void* GetConfig();

  // Get the key corresponding to the set of GLSurfaces that can be made current
  // with this GLSurface.
  virtual unsigned long GetCompatibilityKey();

  // Get the GL pixel format of the surface. Must be implemented in a
  // subclass, though it's ok to just "return GLSurfaceFormat()" if
  // the default is appropriate.
  virtual GLSurfaceFormat GetFormat() = 0;

  // Get access to a helper providing time of recent refresh and period
  // of screen refresh. If unavailable, returns NULL.
  virtual gfx::VSyncProvider* GetVSyncProvider();

  // Schedule an overlay plane to be shown at swap time, or on the next
  // CommitOverlayPlanes call.
  // |z_order| specifies the stacking order of the plane relative to the
  // main framebuffer located at index 0. For the case where there is no
  // main framebuffer, overlays may be scheduled at 0, taking its place.
  // |transform| specifies how the buffer is to be transformed during
  // composition.
  // |image| to be presented by the overlay.
  // |bounds_rect| specify where it is supposed to be on the screen in pixels.
  // |crop_rect| specifies the region within the buffer to be placed inside
  // |bounds_rect|.
  virtual bool ScheduleOverlayPlane(int z_order,
                                    gfx::OverlayTransform transform,
                                    GLImage* image,
                                    const gfx::Rect& bounds_rect,
                                    const gfx::RectF& crop_rect);

  // Schedule a CALayer to be shown at swap time.
  // All arguments correspond to their CALayer properties.
  virtual bool ScheduleCALayer(const ui::CARendererLayerParams& params);

  struct GL_EXPORT CALayerInUseQuery {
    CALayerInUseQuery();
    explicit CALayerInUseQuery(const CALayerInUseQuery&);
    ~CALayerInUseQuery();
    unsigned texture = 0;
    scoped_refptr<GLImage> image;
  };
  virtual void ScheduleCALayerInUseQuery(
      std::vector<CALayerInUseQuery> queries);

  virtual bool ScheduleDCLayer(const ui::DCRendererLayerParams& params);

  virtual bool SetEnableDCLayers(bool enable);

  virtual bool IsSurfaceless() const;

  virtual bool FlipsVertically() const;

  // Returns true if SwapBuffers or PostSubBuffers causes a flip, such that
  // the next buffer may be 2 frames old.
  virtual bool BuffersFlipped() const;

  virtual bool SupportsDCLayers() const;

  virtual bool UseOverlaysForVideo() const;

  // Set the rectangle that will be drawn into on the surface.
  virtual bool SetDrawRectangle(const gfx::Rect& rect);

  // This is the amount by which the scissor and viewport rectangles should be
  // offset.
  virtual gfx::Vector2d GetDrawOffset() const;

  // This waits until rendering work is complete enough that an OS snapshot
  // will capture the last swapped contents. A GL context must be current when
  // calling this.
  virtual void WaitForSnapshotRendering();

  // Tells the surface to rely on implicit sync when swapping buffers.
  virtual void SetRelyOnImplicitSync();

  // Support for eglGetFrameTimestamps.
  virtual bool SupportsSwapTimestamps() const;
  virtual void SetEnableSwapTimestamps();

  static GLSurface* GetCurrent();

 protected:
  virtual ~GLSurface();

  static void SetCurrent(GLSurface* surface);

  static bool ExtensionsContain(const char* extensions, const char* name);

 private:
  friend class base::RefCounted<GLSurface>;
  friend class GLContext;

  DISALLOW_COPY_AND_ASSIGN(GLSurface);
};

// Implementation of GLSurface that forwards all calls through to another
// GLSurface.
class GL_EXPORT GLSurfaceAdapter : public GLSurface {
 public:
  explicit GLSurfaceAdapter(GLSurface* surface);

  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  bool Recreate() override;
  bool DeferDraws() override;
  bool IsOffscreen() override;
  gfx::SwapResult SwapBuffers(const PresentationCallback& callback) override;
  void SwapBuffersAsync(
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback) override;
  gfx::SwapResult SwapBuffersWithBounds(
      const std::vector<gfx::Rect>& rects,
      const PresentationCallback& callback) override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                const PresentationCallback& callback) override;
  void PostSubBufferAsync(
      int x,
      int y,
      int width,
      int height,
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback) override;
  gfx::SwapResult CommitOverlayPlanes(
      const PresentationCallback& callback) override;
  void CommitOverlayPlanesAsync(
      const SwapCompletionCallback& completion_callback,
      const PresentationCallback& presentation_callback) override;
  bool SupportsPresentationCallback() override;
  bool SupportsSwapBuffersWithBounds() override;
  bool SupportsPostSubBuffer() override;
  bool SupportsCommitOverlayPlanes() override;
  bool SupportsAsyncSwap() override;
  gfx::Size GetSize() override;
  void* GetHandle() override;
  unsigned int GetBackingFramebufferObject() override;
  bool OnMakeCurrent(GLContext* context) override;
  bool SetBackbufferAllocation(bool allocated) override;
  void SetFrontbufferAllocation(bool allocated) override;
  void* GetShareHandle() override;
  void* GetDisplay() override;
  void* GetConfig() override;
  unsigned long GetCompatibilityKey() override;
  GLSurfaceFormat GetFormat() override;
  gfx::VSyncProvider* GetVSyncProvider() override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect) override;
  bool ScheduleDCLayer(const ui::DCRendererLayerParams& params) override;
  bool SetEnableDCLayers(bool enable) override;
  bool IsSurfaceless() const override;
  bool FlipsVertically() const override;
  bool BuffersFlipped() const override;
  bool SupportsDCLayers() const override;
  bool UseOverlaysForVideo() const override;
  bool SetDrawRectangle(const gfx::Rect& rect) override;
  gfx::Vector2d GetDrawOffset() const override;
  void WaitForSnapshotRendering() override;
  void SetRelyOnImplicitSync() override;
  bool SupportsSwapTimestamps() const override;
  void SetEnableSwapTimestamps() override;

  GLSurface* surface() const { return surface_.get(); }

 protected:
  ~GLSurfaceAdapter() override;

 private:
  scoped_refptr<GLSurface> surface_;

  DISALLOW_COPY_AND_ASSIGN(GLSurfaceAdapter);
};

// Wraps GLSurface in scoped_refptr and tries to initializes it. Returns a
// scoped_refptr containing the initialized GLSurface or nullptr if
// initialization fails.
GL_EXPORT scoped_refptr<GLSurface> InitializeGLSurface(
    scoped_refptr<GLSurface> surface);

GL_EXPORT scoped_refptr<GLSurface> InitializeGLSurfaceWithFormat(
    scoped_refptr<GLSurface> surface, GLSurfaceFormat format);

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_H_
