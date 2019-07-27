// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/scoped_file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_window_observer.h"
#include "ui/ozone/public/mojom/wayland/wayland_buffer_manager.mojom.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

// This is the buffer manager which creates wl_buffers based on dmabuf (hw
// accelerated compositing) or shared memory (software compositing) and uses
// internal representation of surfaces, which are used to store buffers
// associated with the WaylandWindow.
class WaylandBufferManagerHost : public ozone::mojom::WaylandBufferManagerHost,
                                 public WaylandWindowObserver {
 public:
  explicit WaylandBufferManagerHost(WaylandConnection* connection);
  ~WaylandBufferManagerHost() override;

  // WaylandWindowObserver implements:
  void OnWindowAdded(WaylandWindow* window) override;
  void OnWindowRemoved(WaylandWindow* window) override;

  void SetTerminateGpuCallback(
      base::OnceCallback<void(std::string)> terminate_gpu_cb);

  // Returns bound pointer to own mojo interface.
  ozone::mojom::WaylandBufferManagerHostPtr BindInterface();

  // Unbinds the interface and clears the state of the |buffer_manager_|. Used
  // only when the GPU channel, which uses the mojo pipe to this interface, is
  // destroyed.
  void OnChannelDestroyed();

  // Says if zwp_linux_dmabuf interface is available, and the manager is able to
  // create dmabuf based buffers.
  bool CanCreateDmabufBasedBuffer() const;

  // ozone::mojom::WaylandBufferManagerHost overrides:
  //
  // These overridden methods below are invoked by the GPU when hardware
  // accelerated rendering is used.
  void SetWaylandBufferManagerGpuPtr(
      ozone::mojom::WaylandBufferManagerGpuAssociatedPtrInfo
          buffer_manager_gpu_associated_ptr) override;
  //
  // Called by the GPU and asks to import a wl_buffer based on a gbm file
  // descriptor using zwp_linux_dmabuf protocol. Check comments in the
  // ui/ozone/public/mojom/wayland/wayland_connection.mojom.
  void CreateDmabufBasedBuffer(gfx::AcceleratedWidget widget,
                               mojo::ScopedHandle dmabuf_fd,
                               const gfx::Size& size,
                               const std::vector<uint32_t>& strides,
                               const std::vector<uint32_t>& offsets,
                               const std::vector<uint64_t>& modifiers,
                               uint32_t format,
                               uint32_t planes_count,
                               uint32_t buffer_id) override;
  // Called by the GPU and asks to import a wl_buffer based on a shared memory
  // file descriptor using wl_shm protocol. Check comments in the
  // ui/ozone/public/mojom/wayland/wayland_connection.mojom.
  void CreateShmBasedBuffer(gfx::AcceleratedWidget widget,
                            mojo::ScopedHandle shm_fd,
                            uint64_t length,
                            const gfx::Size& size,
                            uint32_t buffer_id) override;
  // Called by the GPU to destroy the imported wl_buffer with a |buffer_id|.
  void DestroyBuffer(gfx::AcceleratedWidget widget,
                     uint32_t buffer_id) override;
  // Called by the GPU and asks to attach a wl_buffer with a |buffer_id| to a
  // WaylandWindow with the specified |widget|.
  // Calls OnSubmission and OnPresentation on successful swap and pixels
  // presented.
  void CommitBuffer(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::Rect& damage_region) override;

  // When a surface is hidden, the client may want to detach the buffer attached
  // to the surface backed by |widget| to ensure Wayland does not present those
  // contents and do not composite in a wrong way. Otherwise, users may see the
  // contents of a hidden surface on their screens.
  void ResetSurfaceContents(gfx::AcceleratedWidget widget);

 private:
  // This is an internal representation of a real surface, which holds a pointer
  // to WaylandWindow. Also, this object holds buffers, frame callbacks and
  // presentation callbacks for that window's surface.
  class Surface;

  bool CreateBuffer(gfx::AcceleratedWidget& widget,
                    const gfx::Size& size,
                    uint32_t buffer_id);

  Surface* GetSurface(gfx::AcceleratedWidget widget) const;

  // Validates data sent from GPU. If invalid, returns false and sets an error
  // message to |error_message_|.
  bool ValidateDataFromGpu(const gfx::AcceleratedWidget& widget,
                           const base::ScopedFD& file,
                           const gfx::Size& size,
                           const std::vector<uint32_t>& strides,
                           const std::vector<uint32_t>& offsets,
                           const std::vector<uint64_t>& modifiers,
                           uint32_t format,
                           uint32_t planes_count,
                           uint32_t buffer_id);
  bool ValidateDataFromGpu(const gfx::AcceleratedWidget& widget,
                           uint32_t buffer_id);
  bool ValidateDataFromGpu(const gfx::AcceleratedWidget& widget,
                           const base::ScopedFD& file,
                           size_t length,
                           const gfx::Size& size,
                           uint32_t buffer_id);

  // Callback method. Receives a result for the request to create a wl_buffer
  // backend by dmabuf file descriptor from ::CreateBuffer call.
  void OnCreateBufferComplete(gfx::AcceleratedWidget widget,
                              uint32_t buffer_id,
                              wl::Object<struct wl_buffer> new_buffer);

  // Tells the |buffer_manager_gpu_ptr_| the result of a swap call and provides
  // it with the presentation feedback.
  void OnSubmission(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::SwapResult& swap_result);
  void OnPresentation(gfx::AcceleratedWidget widget,
                      uint32_t buffer_id,
                      const gfx::PresentationFeedback& feedback);

  // Terminates the GPU process on invalid data received
  void TerminateGpuProcess();

  base::flat_map<gfx::AcceleratedWidget, std::unique_ptr<Surface>> surfaces_;

  // Set when invalid data is received from the GPU process.
  std::string error_message_;

  // Non-owned pointer to the main connection.
  WaylandConnection* const connection_;

  ozone::mojom::WaylandBufferManagerGpuAssociatedPtr
      buffer_manager_gpu_associated_ptr_;
  mojo::Binding<ozone::mojom::WaylandBufferManagerHost> binding_;

  // A callback, which is used to terminate a GPU process in case of invalid
  // data sent by the GPU to the browser process.
  base::OnceCallback<void(std::string)> terminate_gpu_cb_;

  base::WeakPtrFactory<WaylandBufferManagerHost> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManagerHost);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_HOST_H_
