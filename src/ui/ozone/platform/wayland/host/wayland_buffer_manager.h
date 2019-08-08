// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"

struct wp_presentation_feedback;

namespace ui {

class WaylandConnection;

// The manager uses zwp_linux_dmabuf protocol to create wl_buffers from added
// dmabuf buffers. Only used when GPU runs in own process.
class WaylandBufferManager {
 public:
  explicit WaylandBufferManager(WaylandConnection* connection);
  ~WaylandBufferManager();

  std::string error_message() { return std::move(error_message_); }

  // Creates a wl_buffer based on the dmabuf |file| descriptor. On error, false
  // is returned and |error_message_| is set.
  bool CreateBuffer(base::File file,
                    uint32_t width,
                    uint32_t height,
                    const std::vector<uint32_t>& strides,
                    const std::vector<uint32_t>& offsets,
                    uint32_t format,
                    const std::vector<uint64_t>& modifiers,
                    uint32_t planes_count,
                    uint32_t buffer_id);

  // Assigns a wl_buffer with |buffer_id| to a window with the same |widget|. On
  // error, false is returned and |error_message_| is set. A |damage_region|
  // identifies which part of the buffer is updated. If an empty region is
  // provided, the whole buffer is updated. Once a frame callback or
  // presentation callback is received, WaylandConnection::OnSubmission and
  // WaylandConnection::OnPresentation are called. Though, it is guaranteed
  // OnPresentation won't be called earlier than OnSubmission.
  bool ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                          uint32_t buffer_id,
                          const gfx::Rect& damage_region);

  // Destroys a buffer with |buffer_id| in |buffers_|. On error, false is
  // returned and |error_message_| is set.
  bool DestroyBuffer(uint32_t buffer_id);

  // Destroys all the data and buffers stored in own containers.
  void ClearState();

 private:
  FRIEND_TEST_ALL_PREFIXES(WaylandBufferManagerTest, ValidateDataFromGpu);

  // This is an internal helper representation of a wayland buffer object, which
  // the GPU process creates when CreateBuffer is called. It's used for
  // asynchronous buffer creation and stores |params| parameter to find out,
  // which Buffer the wl_buffer corresponds to when CreateSucceeded is called.
  // What is more, the Buffer stores such information as a widget it is attached
  // to, its buffer id for simplier buffer management and other members specific
  // to this Buffer object on run-time.
  struct Buffer {
    Buffer() = delete;
    Buffer(const gfx::Size& buffer_size, uint32_t buffer_id);
    ~Buffer();

    // Actual buffer size.
    const gfx::Size size;

    // The id of the buffer.
    const uint32_t buffer_id;

    // Widget to attached/being attach WaylandWindow.
    gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;

    // Describes the region where the pending buffer is different from the
    // current surface contents, and where the surface therefore needs to be
    // repainted.
    gfx::Rect damage_region;

    // A feedback, which is received if a presentation feedback protocol is
    // supported.
    gfx::PresentationFeedback feedback;

    // A wl_buffer backed by a dmabuf created on the GPU side.
    wl::Object<struct wl_buffer> wl_buffer;

    // Provide the status of this buffer. Reset on each new swap.
    bool swapped = false;
    bool presented = false;

    // A Wayland callback, which is triggered once wl_buffer has been committed
    // and it is right time to notify the GPU that it can start a new drawing
    // operation.
    wl::Object<wl_callback> wl_frame_callback;

    // A presentation feedback provided by the Wayland server once frame is
    // shown.
    wl::Object<struct wp_presentation_feedback> wp_presentation_feedback;

    DISALLOW_COPY_AND_ASSIGN(Buffer);
  };

  bool SwapBuffer(Buffer* buffer);

  // Validates data sent from GPU. If invalid, returns false and sets an error
  // message to |error_message_|.
  bool ValidateDataFromGpu(const base::File& file,
                           uint32_t width,
                           uint32_t height,
                           const std::vector<uint32_t>& strides,
                           const std::vector<uint32_t>& offsets,
                           uint32_t format,
                           const std::vector<uint64_t>& modifiers,
                           uint32_t planes_count,
                           uint32_t buffer_id);
  bool ValidateDataFromGpu(const gfx::AcceleratedWidget& widget,
                           uint32_t buffer_id);

  // Callback method. Receives a result for the request to create a wl_buffer
  // backend by dmabuf file descriptor from ::CreateBuffer call.
  void OnCreateBufferComplete(uint32_t buffer_id,
                              wl::Object<struct wl_buffer> new_buffer);

  void OnSubmission(Buffer* buffer, const gfx::SwapResult& swap_result);
  void OnPresentation(Buffer* buffer,
                      const gfx::PresentationFeedback& feedback);

  // wl_callback_listener
  static void FrameCallbackDone(void* data,
                                wl_callback* callback,
                                uint32_t time);

  // wp_presentation_feedback_listener
  static void FeedbackSyncOutput(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback,
      struct wl_output* output);
  static void FeedbackPresented(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback,
      uint32_t tv_sec_hi,
      uint32_t tv_sec_lo,
      uint32_t tv_nsec,
      uint32_t refresh,
      uint32_t seq_hi,
      uint32_t seq_lo,
      uint32_t flags);
  static void FeedbackDiscarded(
      void* data,
      struct wp_presentation_feedback* wp_presentation_feedback);

  // A container of created buffers.
  base::flat_map<uint32_t, std::unique_ptr<Buffer>> buffers_;

  // Set when invalid data is received from the GPU process.
  std::string error_message_;

  // Non-owned pointer to the main connection.
  WaylandConnection* const connection_;

  base::WeakPtrFactory<WaylandBufferManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_H_
