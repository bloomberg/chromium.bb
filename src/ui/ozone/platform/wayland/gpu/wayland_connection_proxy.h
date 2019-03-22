// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CONNECTION_PROXY_H_
#define UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CONNECTION_PROXY_H_

#include "base/macros.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_checker.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_util.h"
#include "ui/ozone/public/interfaces/wayland/wayland_connection.mojom.h"

#if defined(WAYLAND_GBM)
#include "ui/ozone/common/linux/gbm_device.h"  // nogncheck
#endif

struct wl_shm;

namespace gfx {
enum class SwapResult;
class Rect;
}  // namespace gfx

namespace ui {

class WaylandConnection;
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
  explicit WaylandConnectionProxy(WaylandConnection* connection);
  ~WaylandConnectionProxy() override;

  // WaylandConnectionProxy overrides:
  void SetWaylandConnection(ozone::mojom::WaylandConnectionPtr wc_ptr) override;

  // Methods, which must be used when GPU is hosted on a different process
  // aka gpu process.
  //
  // Asks Wayland to create a wl_buffer based on a shared buffer file
  // descriptor backed (gbm_bo).
  void CreateZwpLinuxDmabuf(base::File file,
                            gfx::Size size,
                            const std::vector<uint32_t>& strides,
                            const std::vector<uint32_t>& offsets,
                            const std::vector<uint64_t>& modifiers,
                            uint32_t current_format,
                            uint32_t planes_count,
                            uint32_t buffer_id);

  // Asks Wayland to destroy a wl_buffer.
  void DestroyZwpLinuxDmabuf(uint32_t buffer_id);

  // Asks Wayland to find a wl_buffer with the |buffer_id| and schedule a
  // buffer swap for a WaylandWindow, which backs the following |widget|.
  // The |callback| is called once a frame callback from the Wayland server
  // is received.
  void ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                          uint32_t buffer_id,
                          const gfx::Rect& damage_region,
                          wl::BufferSwapCallback callback);

#if defined(WAYLAND_GBM)
  // Returns a gbm_device based on a DRM render node.
  GbmDevice* gbm_device() const { return gbm_device_.get(); }
  void set_gbm_device(std::unique_ptr<GbmDevice> gbm_device) {
    gbm_device_ = std::move(gbm_device);
  }
#endif

  // Methods, which must be used when a single process mode is used (GPU is
  // hosted in the browser process).
  //
  // Return a WaylandWindow based on the |widget|.
  WaylandWindow* GetWindow(gfx::AcceleratedWidget widget);
  // Schedule flush in the Wayland message loop.
  void ScheduleFlush();
  // Returns an object for a shared memory support. Used for software fallback.
  wl_shm* shm();

  // Methods, which can be used with both single- and multi-process modes.
  //
  // Returns a pointer to native display. When used in single process mode,
  // a wl_display pointer is returned. For the the mode, when there are GPU
  // and browser processes, EGL_DEFAULT_DISPLAY is returned.
  intptr_t Display();

  // Adds a WaylandConnectionClient binding.
  void AddBindingWaylandConnectionClient(
      ozone::mojom::WaylandConnectionClientRequest request);

  WaylandConnection* connection() { return connection_; }

 private:
  void CreateZwpLinuxDmabufInternal(base::File file,
                                    gfx::Size size,
                                    const std::vector<uint32_t>& strides,
                                    const std::vector<uint32_t>& offsets,
                                    const std::vector<uint64_t>& modifiers,
                                    uint32_t current_format,
                                    uint32_t planes_count,
                                    uint32_t buffer_id);
  void DestroyZwpLinuxDmabufInternal(uint32_t buffer_id);

  // Non-owned pointer to a WaylandConnection. It is only used in a single
  // process mode, when a shared dmabuf approach is not used.
  WaylandConnection* connection_ = nullptr;

#if defined(WAYLAND_GBM)
  // A DRM render node based gbm device.
  std::unique_ptr<GbmDevice> gbm_device_;
#endif

  mojo::BindingSet<ozone::mojom::WaylandConnectionClient> bindings_;

  // A pointer to a WaylandConnection object, which always lives on a browser
  // process side. It's used for a multi-process mode.
  ozone::mojom::WaylandConnectionPtr wc_ptr_;
  ozone::mojom::WaylandConnectionPtrInfo wc_ptr_info_;
  bool bound_ = false;

  // A task runner, which is initialized in a multi-process mode. It is used to
  // ensure all the methods of this class are run on GpuMainThread. This is
  // needed to ensure mojo calls happen on a right sequence. What is more, it
  // makes it possible to use a frame callback (when it is implemented) in the
  // browser process, which calls back to a right sequence after a
  // ScheduleBufferSwap call.
  scoped_refptr<base::SingleThreadTaskRunner> gpu_thread_runner_;

  DISALLOW_COPY_AND_ASSIGN(WaylandConnectionProxy);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_GPU_WAYLAND_CONNECTION_PROXY_H_
