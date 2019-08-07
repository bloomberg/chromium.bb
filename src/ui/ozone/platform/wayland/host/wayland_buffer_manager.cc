// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager.h"

#include <presentation-time-client-protocol.h>
#include <memory>

#include "base/trace_event/trace_event.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"

namespace ui {

namespace {

uint32_t GetPresentationKindFlags(uint32_t flags) {
  uint32_t presentation_flags = 0;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_VSYNC)
    presentation_flags |= gfx::PresentationFeedback::kVSync;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_HW_CLOCK)
    presentation_flags |= gfx::PresentationFeedback::kHWClock;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_HW_COMPLETION)
    presentation_flags |= gfx::PresentationFeedback::kHWCompletion;
  if (flags & WP_PRESENTATION_FEEDBACK_KIND_ZERO_COPY)
    presentation_flags |= gfx::PresentationFeedback::kZeroCopy;

  return presentation_flags;
}

base::TimeTicks GetPresentationFeedbackTimeStamp(uint32_t tv_sec_hi,
                                                 uint32_t tv_sec_lo,
                                                 uint32_t tv_nsec) {
  const int64_t seconds = (static_cast<int64_t>(tv_sec_hi) << 32) + tv_sec_lo;
  const int64_t microseconds = seconds * base::Time::kMicrosecondsPerSecond +
                               tv_nsec / base::Time::kNanosecondsPerMicrosecond;
  return base::TimeTicks() + base::TimeDelta::FromMicroseconds(microseconds);
}

}  // namespace

WaylandBufferManager::Buffer::Buffer(const gfx::Size& buffer_size,
                                     uint32_t buffer_id)
    : size(buffer_size), buffer_id(buffer_id) {}
WaylandBufferManager::Buffer::~Buffer() = default;

WaylandBufferManager::WaylandBufferManager(WaylandConnection* connection)
    : connection_(connection), weak_factory_(this) {}

WaylandBufferManager::~WaylandBufferManager() {
  DCHECK(buffers_.empty());
}

bool WaylandBufferManager::CreateBuffer(base::File file,
                                        uint32_t width,
                                        uint32_t height,
                                        const std::vector<uint32_t>& strides,
                                        const std::vector<uint32_t>& offsets,
                                        uint32_t format,
                                        const std::vector<uint64_t>& modifiers,
                                        uint32_t planes_count,
                                        uint32_t buffer_id) {
  TRACE_EVENT2("wayland", "WaylandBufferManager::CreateZwpLinuxDmabuf",
               "Format", format, "Buffer id", buffer_id);

  if (!ValidateDataFromGpu(file, width, height, strides, offsets, format,
                           modifiers, planes_count, buffer_id)) {
    // base::File::Close() has an assertion that checks if blocking operations
    // are allowed. Thus, manually close the fd here.
    base::ScopedFD deleter(file.TakePlatformFile());
    return false;
  }

  std::unique_ptr<Buffer> buffer =
      std::make_unique<Buffer>(gfx::Size(width, height), buffer_id);
  buffers_.insert(std::make_pair(buffer_id, std::move(buffer)));

  auto callback = base::BindOnce(&WaylandBufferManager::OnCreateBufferComplete,
                                 weak_factory_.GetWeakPtr(), buffer_id);
  connection_->zwp_dmabuf()->CreateBuffer(
      std::move(file), gfx::Size(width, height), strides, offsets, modifiers,
      format, planes_count, std::move(callback));
  return true;
}

bool WaylandBufferManager::ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                                              uint32_t buffer_id,
                                              const gfx::Rect& damage_region) {
  TRACE_EVENT1("wayland", "WaylandBufferManager::ScheduleSwapBuffer",
               "Buffer id", buffer_id);

  if (!ValidateDataFromGpu(widget, buffer_id))
    return false;

  auto it = buffers_.find(buffer_id);
  if (it == buffers_.end()) {
    error_message_ =
        "Buffer with " + std::to_string(buffer_id) + " id does not exist";
    return false;
  }

  Buffer* buffer = it->second.get();
  DCHECK(buffer);
  DCHECK(!buffer->swapped && !buffer->presented);

  // Assign a widget to this buffer, which is used to find a corresponding
  // WaylandWindow.
  buffer->widget = widget;
  buffer->damage_region = damage_region;

  if (buffer->wl_buffer) {
    // A wl_buffer might not exist by this time. Silently return.
    // TODO: check this.
    return SwapBuffer(buffer);
  }
  return true;
}

bool WaylandBufferManager::DestroyBuffer(uint32_t buffer_id) {
  TRACE_EVENT1("wayland", "WaylandBufferManager::DestroyZwpLinuxDmabuf",
               "Buffer id", buffer_id);

  auto it = buffers_.find(buffer_id);
  if (it == buffers_.end()) {
    error_message_ = "Trying to destroy non-existing buffer";
    return false;
  }

  buffers_.erase(it);

  connection_->ScheduleFlush();
  return true;
}

void WaylandBufferManager::ClearState() {
  buffers_.clear();
}

// TODO(msisov): handle buffer swap failure or success.
bool WaylandBufferManager::SwapBuffer(Buffer* buffer) {
  WaylandWindow* window = connection_->GetWindow(buffer->widget);
  if (!window) {
    error_message_ = "A WaylandWindow with current widget does not exist";
    return false;
  }

  gfx::Rect damage_region = buffer->damage_region;
  // If the size of the damage region is empty, wl_surface_damage must be
  // supplied with the actual size of the buffer, which is going to be
  // committed.
  if (damage_region.size().IsEmpty())
    damage_region.set_size(buffer->size);

  wl_surface_damage_buffer(window->surface(), damage_region.x(),
                           damage_region.y(), damage_region.width(),
                           damage_region.height());
  wl_surface_attach(window->surface(), buffer->wl_buffer.get(), 0, 0);

  static const wl_callback_listener frame_listener = {
      WaylandBufferManager::FrameCallbackDone};
  DCHECK(!buffer->wl_frame_callback);
  buffer->wl_frame_callback.reset(wl_surface_frame(window->surface()));
  wl_callback_add_listener(buffer->wl_frame_callback.get(), &frame_listener,
                           this);

  // Set up presentation feedback.
  static const wp_presentation_feedback_listener feedback_listener = {
      WaylandBufferManager::FeedbackSyncOutput,
      WaylandBufferManager::FeedbackPresented,
      WaylandBufferManager::FeedbackDiscarded};
  if (connection_->presentation()) {
    DCHECK(!buffer->wp_presentation_feedback);
    buffer->wp_presentation_feedback.reset(wp_presentation_feedback(
        connection_->presentation(), window->surface()));
    wp_presentation_feedback_add_listener(
        buffer->wp_presentation_feedback.get(), &feedback_listener, this);
  }

  wl_surface_commit(window->surface());

  connection_->ScheduleFlush();
  return true;
}

bool WaylandBufferManager::ValidateDataFromGpu(
    const base::File& file,
    uint32_t width,
    uint32_t height,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    uint32_t format,
    const std::vector<uint64_t>& modifiers,
    uint32_t planes_count,
    uint32_t buffer_id) {
  std::string reason;
  if (!file.IsValid())
    reason = "Buffer fd is invalid";

  if (width == 0 || height == 0)
    reason = "Buffer size is invalid";

  if (planes_count < 1)
    reason = "Planes count cannot be less than 1";

  if (planes_count != strides.size() || planes_count != offsets.size() ||
      planes_count != modifiers.size()) {
    reason = "Number of strides(" + std::to_string(strides.size()) +
             ")/offsets(" + std::to_string(offsets.size()) + ")/modifiers(" +
             std::to_string(modifiers.size()) +
             ") does not correspond to the number of planes(" +
             std::to_string(planes_count) + ")";
  }

  for (auto stride : strides) {
    if (stride == 0)
      reason = "Strides are invalid";
  }

  if (!IsValidBufferFormat(format))
    reason = "Buffer format is invalid";

  if (buffer_id < 1)
    reason = "Invalid buffer id: " + std::to_string(buffer_id);

  auto it = buffers_.find(buffer_id);
  if (it != buffers_.end()) {
    reason = "A buffer with " + std::to_string(buffer_id) +
             " id has already existed";
  }

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }
  return true;
}

bool WaylandBufferManager::ValidateDataFromGpu(
    const gfx::AcceleratedWidget& widget,
    uint32_t buffer_id) {
  std::string reason;

  if (widget == gfx::kNullAcceleratedWidget)
    reason = "Invalid accelerated widget";

  if (buffer_id < 1)
    reason = "Invalid buffer id: " + std::to_string(buffer_id);

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

void WaylandBufferManager::OnCreateBufferComplete(
    uint32_t buffer_id,
    wl::Object<struct wl_buffer> new_buffer) {
  auto it = buffers_.find(buffer_id);
  // It can happen that buffer was destroyed by a client while the Wayland
  // compositor was processing a request to create a wl_buffer.
  if (it == buffers_.end())
    return;

  Buffer* buffer = it->second.get();
  buffer->wl_buffer = std::move(new_buffer);

  if (buffer->widget != gfx::kNullAcceleratedWidget)
    SwapBuffer(buffer);
}

void WaylandBufferManager::OnSubmission(Buffer* buffer,
                                        const gfx::SwapResult& swap_result) {
  DCHECK(!buffer->swapped);

  buffer->wl_frame_callback.reset();
  buffer->swapped = true;
  connection_->OnSubmission(buffer->widget, buffer->buffer_id,
                            gfx::SwapResult::SWAP_ACK);

  // If presentation feedback is not supported, use a fake feedback.
  if (!connection_->presentation()) {
    DCHECK(!buffer->wp_presentation_feedback && !buffer->presented);
    OnPresentation(buffer, gfx::PresentationFeedback(base::TimeTicks::Now(),
                                                     base::TimeDelta(), 0));
  } else if (buffer->presented) {
    DCHECK(!buffer->wp_presentation_feedback);
    // If the buffer has been presented before the frame callback aka
    // completion callback (in the future, release callback is going to be
    // used), present the feedback to the GPU.
    OnPresentation(buffer, buffer->feedback);
  } else {
    DCHECK(buffer->wp_presentation_feedback);
  }
}

void WaylandBufferManager::OnPresentation(
    Buffer* buffer,
    const gfx::PresentationFeedback& feedback) {
  buffer->presented = true;
  buffer->feedback = feedback;

  // If buffer has already been swapped, we can safely notify about the
  // presentation as well.
  if (buffer->swapped) {
    connection_->OnPresentation(buffer->widget, buffer->buffer_id,
                                buffer->feedback);

    // Reset the status so that DCHECK passes in ::ScheduleBufferSwap call.
    buffer->presented = false;
    buffer->swapped = false;
  }
}

// static
void WaylandBufferManager::FrameCallbackDone(void* data,
                                             wl_callback* callback,
                                             uint32_t time) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  DCHECK(self);

  for (auto& item : self->buffers_) {
    Buffer* buffer = item.second.get();
    if (buffer->wl_frame_callback.get() == callback) {
      buffer->wl_frame_callback.reset();
      self->OnSubmission(buffer, gfx::SwapResult::SWAP_ACK);
      return;
    }
  }
}

// static
void WaylandBufferManager::FeedbackSyncOutput(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    struct wl_output* output) {
  NOTIMPLEMENTED_LOG_ONCE();
}

// static
void WaylandBufferManager::FeedbackPresented(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback,
    uint32_t tv_sec_hi,
    uint32_t tv_sec_lo,
    uint32_t tv_nsec,
    uint32_t refresh,
    uint32_t seq_hi,
    uint32_t seq_lo,
    uint32_t flags) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  DCHECK(self);
  for (auto& item : self->buffers_) {
    Buffer* buffer = item.second.get();
    if (buffer->wp_presentation_feedback.get() == wp_presentation_feedback) {
      DCHECK(!buffer->presented);
      buffer->wp_presentation_feedback.reset();
      self->OnPresentation(
          buffer,
          gfx::PresentationFeedback(
              GetPresentationFeedbackTimeStamp(tv_sec_hi, tv_sec_lo, tv_nsec),
              base::TimeDelta::FromNanoseconds(refresh),
              GetPresentationKindFlags(flags)));
      return;
    }
  }
}

// static
void WaylandBufferManager::FeedbackDiscarded(
    void* data,
    struct wp_presentation_feedback* wp_presentation_feedback) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  DCHECK(self);
  for (auto& item : self->buffers_) {
    Buffer* buffer = item.second.get();
    if (buffer->wp_presentation_feedback.get() == wp_presentation_feedback) {
      DCHECK(!buffer->presented);
      buffer->wp_presentation_feedback.reset();
      self->OnPresentation(buffer, gfx::PresentationFeedback::Failure());
      return;
    }
  }
}

}  // namespace ui
