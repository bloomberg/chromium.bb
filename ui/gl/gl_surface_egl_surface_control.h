// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_GL_GL_SURFACE_EGL_SURFACE_CONTROL_H_
#define UI_GL_GL_SURFACE_EGL_SURFACE_CONTROL_H_

#include <android/native_window.h>
#include <memory>

#include "base/android/scoped_hardware_buffer_handle.h"
#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "ui/gl/android/android_surface_control_compat.h"
#include "ui/gl/gl_export.h"
#include "ui/gl/gl_surface_egl.h"

namespace base {
class SingleThreadTaskRunner;

namespace android {
class ScopedHardwareBufferFenceSync;
}  // namespace android
}  // namespace base

namespace gl {

class GL_EXPORT GLSurfaceEGLSurfaceControl : public GLSurfaceEGL {
 public:
  explicit GLSurfaceEGLSurfaceControl(
      ANativeWindow* window,
      scoped_refptr<base::SingleThreadTaskRunner> task_runner);

  // GLSurface implementation.
  int GetBufferCount() const override;
  bool Initialize(GLSurfaceFormat format) override;
  void Destroy() override;
  bool Resize(const gfx::Size& size,
              float scale_factor,
              ColorSpace color_space,
              bool has_alpha) override;
  bool IsOffscreen() override;

  gfx::Size GetSize() override;
  bool OnMakeCurrent(GLContext* context) override;
  bool ScheduleOverlayPlane(int z_order,
                            gfx::OverlayTransform transform,
                            GLImage* image,
                            const gfx::Rect& bounds_rect,
                            const gfx::RectF& crop_rect,
                            bool enable_blend,
                            std::unique_ptr<gfx::GpuFence> gpu_fence) override;
  bool IsSurfaceless() const override;
  void* GetHandle() override;

  // Sync versions of frame update, should never be used.
  gfx::SwapResult SwapBuffers(PresentationCallback callback) override;
  gfx::SwapResult CommitOverlayPlanes(PresentationCallback callback) override;
  gfx::SwapResult PostSubBuffer(int x,
                                int y,
                                int width,
                                int height,
                                PresentationCallback callback) override;

  void SwapBuffersAsync(SwapCompletionCallback completion_callback,
                        PresentationCallback presentation_callback) override;
  void CommitOverlayPlanesAsync(
      SwapCompletionCallback completion_callback,
      PresentationCallback presentation_callback) override;
  void PostSubBufferAsync(int x,
                          int y,
                          int width,
                          int height,
                          SwapCompletionCallback completion_callback,
                          PresentationCallback presentation_callback) override;

  bool SupportsAsyncSwap() override;
  bool SupportsPlaneGpuFences() const override;
  bool SupportsPresentationCallback() override;
  bool SupportsPostSubBuffer() override;
  bool SupportsCommitOverlayPlanes() override;

 private:
  ~GLSurfaceEGLSurfaceControl() override;

  struct SurfaceState {
    SurfaceState();
    explicit SurfaceState(const SurfaceControl::Surface& parent);
    ~SurfaceState();

    SurfaceState(SurfaceState&& other);
    SurfaceState& operator=(SurfaceState&& other);

    int z_order = 0;
    AHardwareBuffer* hardware_buffer = nullptr;
    gfx::Rect dst;
    gfx::Rect src;
    gfx::OverlayTransform transform = gfx::OVERLAY_TRANSFORM_NONE;
    bool opaque = true;

    // Indicates whether buffer for this layer was updated in the currently
    // pending transaction, or the last transaction submitted if there isn't
    // one pending.
    bool buffer_updated_in_pending_transaction = true;

    scoped_refptr<SurfaceControl::Surface> surface;
  };

  struct ResourceRef {
    ResourceRef();
    ~ResourceRef();

    ResourceRef(ResourceRef&& other);
    ResourceRef& operator=(ResourceRef&& other);

    scoped_refptr<SurfaceControl::Surface> surface;
    std::unique_ptr<base::android::ScopedHardwareBufferFenceSync> scoped_buffer;
  };
  using ResourceRefs = base::flat_map<ASurfaceControl*, ResourceRef>;

  void CommitPendingTransaction(const gfx::Rect& damage_rect,
                                SwapCompletionCallback completion_callback,
                                PresentationCallback callback);

  // Called on the |gpu_task_runner_| when a transaction is acked by the
  // framework.
  void OnTransactionAckOnGpuThread(
      SwapCompletionCallback completion_callback,
      PresentationCallback presentation_callback,
      ResourceRefs released_resources,
      SurfaceControl::TransactionStats transaction_stats);

  // The rect of the native window backing this surface.
  gfx::Rect window_rect_;

  // Holds the surface state changes made since the last call to SwapBuffers.
  base::Optional<SurfaceControl::Transaction> pending_transaction_;

  // The list of Surfaces and the corresponding state. The initial
  // |pending_surfaces_count_| surfaces in this list are surfaces with state
  // mutated since the last SwapBuffers with the updates collected in
  // |pending_transaction_|.
  // On the next SwapBuffers, the updates in the transaction are applied
  // atomically and any surfaces in |surface_list_| which are not reused in this
  // frame are destroyed.
  std::vector<SurfaceState> surface_list_;
  size_t pending_surfaces_count_ = 0u;

  // Resources in the pending frame, for which updates are being
  // collected in |pending_transaction_|. These are resources for which the
  // pending transaction has a ref but they have not been applied and
  // transferred to the framework.
  ResourceRefs pending_frame_resources_;

  // Resources in the current frame sent to the framework. The
  // framework is assumed to retain ownership of these resources until the next
  // frame update.
  ResourceRefs current_frame_resources_;

  // The root surface tied to the ANativeWindow that places the content of this
  // GLSurface in the java view tree.
  scoped_refptr<SurfaceControl::Surface> root_surface_;

  // The last context made current with this surface.
  scoped_refptr<GLContext> context_;

  scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner_;
  base::WeakPtrFactory<GLSurfaceEGLSurfaceControl> weak_factory_;
};

}  // namespace gl

#endif  // UI_GL_GL_SURFACE_EGL_SURFACE_CONTROL_H_
