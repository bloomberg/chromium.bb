// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CONNECTION_PROXY_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CONNECTION_PROXY_H_

#include <memory>

#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/associated_binding.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gl/gl_surface.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/public/interfaces/wayland/wayland_connection.mojom.h"

#if defined(WAYLAND_GBM)
#include "ui/ozone/common/linux/gbm_device.h"  // nogncheck
#endif

namespace gfx {
enum class SwapResult;
class Rect;
}  // namespace gfx

namespace ui {

class WaylandConnection;
class WaylandSurfaceFactory;
class WaylandWindow;

// Provides a proxy connection to a WaylandConnection object on
// browser process side. When in multi-process mode, this is used to create
// Wayland dmabufs and ask it to do commits. When in single process mode,
// this class just forwards calls directly to WaylandConnection.
//
// It's guaranteed that WaylandConnectionProxy makes mojo calls on the right
// sequence.
class WaylandConnectionProxy : public ozone::mojom::WaylandConnectionClient {
 public:
  WaylandConnectionProxy(WaylandConnection* connection,
                         WaylandSurfaceFactory* factory);
  ~WaylandConnectionProxy() override;

  // WaylandConnectionProxy overrides:
  void SetWaylandConnection(ozone::mojom::WaylandConnectionPtr wc_ptr) override;
  void ResetGbmDevice() override;
  // These two calls get the surface, which backs the |widget| and notifies it
  // about the submission and the presentation. After the surface receives the
  // OnSubmission call, it can schedule a new buffer for swap.
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    gfx::SwapResult swap_result) override;
  void OnPresentation(gfx::AcceleratedWidget widget,
                      uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback) override;

  // Methods, which must be used when GPU is hosted on a different process
  // aka gpu process.
  //
  // Asks Wayland to create a wl_buffer based on a shared buffer file
  // descriptor backed (gbm_bo).
  void CreateZwpLinuxDmabuf(gfx::AcceleratedWidget widget,
                            base::File file,
                            gfx::Size size,
                            const std::vector<uint32_t>& strides,
                            const std::vector<uint32_t>& offsets,
                            const std::vector<uint64_t>& modifiers,
                            uint32_t current_format,
                            uint32_t planes_count,
                            uint32_t buffer_id);

  // Asks Wayland to destroy a wl_buffer.
  void DestroyZwpLinuxDmabuf(gfx::AcceleratedWidget widget, uint32_t buffer_id);

  // Asks Wayland to find a wl_buffer with the |buffer_id| and attach the
  // buffer to the WaylandWindow's surface, which backs the following |widget|.
  // Once the buffer is submitted and presented, the OnSubmission and
  // OnPresentation are called. Note, it's not guaranteed the OnPresentation
  // will follow the OnSubmission immediately, but the OnPresentation must never
  // be called before the OnSubmission is called for that particular buffer.
  // This logic must be checked by the client, though the host ensures this
  // logic as well. This call must not be done twice for the same |widget| until
  // the OnSubmission is called (which actually means the client can continue
  // sending buffer swap requests).
  void CommitBuffer(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::Rect& damage_region);

#if defined(WAYLAND_GBM)
  // Returns a gbm_device based on a DRM render node.
  GbmDevice* gbm_device() const { return gbm_device_.get(); }
  void set_gbm_device(std::unique_ptr<GbmDevice> gbm_device) {
    gbm_device_ = std::move(gbm_device);
  }
#endif

  // Methods that are used to manage shared buffers when software rendering is
  // used:
  //
  // Asks Wayland to create a buffer based on shared memory |file| handle for
  // specific |widget|. There can be only one buffer per widget.
  void CreateShmBufferForWidget(gfx::AcceleratedWidget widget,
                                base::File file,
                                size_t length,
                                const gfx::Size size);

  // Asks to damage and commit previously created buffer for the |widget|.
  void PresentShmBufferForWidget(gfx::AcceleratedWidget widget,
                                 const gfx::Rect& damage);

  // Asks to destroy shared memory based buffer for the |widget|.
  void DestroyShmBuffer(gfx::AcceleratedWidget widget);

  // Methods, which must be used when a single process mode is used (GPU is
  // hosted in the browser process).
  //
  // Return a WaylandWindow based on the |widget|.
  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget) const;
  // Schedule flush in the Wayland message loop.
  void ScheduleFlush();

  // Methods, which can be used with both single- and multi-process modes.
  //
  // Returns a pointer to native display. When used in single process mode,
  // a wl_display pointer is returned. For the the mode, when there are GPU
  // and browser processes, EGL_DEFAULT_DISPLAY is returned.
  intptr_t Display() const;

  // Adds a WaylandConnectionClient binding.
  void AddBindingWaylandConnectionClient(
      ozone::mojom::WaylandConnectionClientRequest request);

  WaylandConnection* connection() const { return connection_; }

 private:
  void CreateZwpLinuxDmabufInternal(gfx::AcceleratedWidget widget,
                                    base::File file,
                                    gfx::Size size,
                                    const std::vector<uint32_t>& strides,
                                    const std::vector<uint32_t>& offsets,
                                    const std::vector<uint64_t>& modifiers,
                                    uint32_t current_format,
                                    uint32_t planes_count,
                                    uint32_t buffer_id);
  void DestroyZwpLinuxDmabufInternal(gfx::AcceleratedWidget widget,
                                     uint32_t buffer_id);

  void BindHostInterface();

  // Non-owned pointer to a WaylandConnection. It is only used in a single
  // process mode, when a shared dmabuf approach is not used.
  WaylandConnection* const connection_;

  // Non-owned. Only used to get registered surfaces and notify them about
  // submission and presentation of buffers.
  WaylandSurfaceFactory* const factory_;

#if defined(WAYLAND_GBM)
  // A DRM render node based gbm device.
  std::unique_ptr<GbmDevice> gbm_device_;
#endif

  mojo::BindingSet<ozone::mojom::WaylandConnectionClient> bindings_;

  // A pointer to a WaylandConnection object, which always lives on a browser
  // process side. It's used for a multi-process mode.
  ozone::mojom::WaylandConnectionPtr wc_ptr_;
  ozone::mojom::WaylandConnectionPtrInfo wc_ptr_info_;

  mojo::AssociatedBinding<ozone::mojom::WaylandConnectionClient>
      associated_binding_;

  // A task runner, which is initialized in a multi-process mode. It is used to
  // ensure all the methods of this class are run on GpuMainThread. This is
  // needed to ensure mojo calls happen on a right sequence. What is more, it
  // makes it possible to use a frame callback (when it is implemented) in the
  // browser process, which calls back to a right sequence after a
  // CommitBuffer call.
  scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_runner_;

  DISALLOW_COPY_AND_ASSIGN(WaylandConnectionProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CONNECTION_PROXY_H_
