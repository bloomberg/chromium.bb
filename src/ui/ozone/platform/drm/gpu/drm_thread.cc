// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread.h"

#include <gbm.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "base/message_loop/message_pump_type.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/linux/gbm_defines.h"
#include "ui/gfx/linux/gbm_device.h"
#include "ui/gfx/linux/gbm_util.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_dumb_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_pixmap.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

void CreateBufferWithGbmFlags(const scoped_refptr<DrmDevice>& drm,
                              uint32_t fourcc_format,
                              const gfx::Size& size,
                              const gfx::Size& framebuffer_size,
                              uint32_t flags,
                              const std::vector<uint64_t>& modifiers,
                              std::unique_ptr<GbmBuffer>* out_buffer,
                              scoped_refptr<DrmFramebuffer>* out_framebuffer) {
  std::unique_ptr<GbmBuffer> buffer =
      drm->gbm_device()->CreateBufferWithModifiers(fourcc_format, size, flags,
                                                   modifiers);
  if (!buffer)
    return;

  scoped_refptr<DrmFramebuffer> framebuffer;
  if (flags & GBM_BO_USE_SCANOUT) {
    framebuffer = DrmFramebuffer::AddFramebuffer(drm, buffer.get(),
                                                 framebuffer_size, modifiers);
    if (!framebuffer)
      return;
  }

  *out_buffer = std::move(buffer);
  *out_framebuffer = std::move(framebuffer);
}

}  // namespace

DrmThread::TaskInfo::TaskInfo(base::OnceClosure task, base::WaitableEvent* done)
    : task(std::move(task)), done(done) {}

DrmThread::TaskInfo::TaskInfo(TaskInfo&& other) = default;

DrmThread::TaskInfo::~TaskInfo() = default;

DrmThread::DrmThread() : base::Thread("DrmThread") {}

DrmThread::~DrmThread() {
  Stop();
}

void DrmThread::Start(base::OnceClosure receiver_completer,
                      std::unique_ptr<DrmDeviceGenerator> device_generator) {
  complete_early_receiver_requests_ = std::move(receiver_completer);
  device_generator_ = std::move(device_generator);

  base::Thread::Options thread_options;
  thread_options.message_pump_type = base::MessagePumpType::IO;
  thread_options.priority = base::ThreadPriority::DISPLAY;

  if (!StartWithOptions(thread_options))
    LOG(FATAL) << "Failed to create DRM thread";
}

void DrmThread::RunTaskAfterDeviceReady(base::OnceClosure task,
                                        base::WaitableEvent* done) {
  if (!device_manager_->GetDrmDevices().empty()) {
    std::move(task).Run();
    if (done)
      done->Signal();
    return;
  }
  pending_tasks_.emplace_back(std::move(task), done);
}

void DrmThread::Init() {
  TRACE_EVENT0("drm", "DrmThread::Init");
  device_manager_ =
      std::make_unique<DrmDeviceManager>(std::move(device_generator_));
  screen_manager_ = std::make_unique<ScreenManager>();
  display_manager_.reset(
      new DrmGpuDisplayManager(screen_manager_.get(), device_manager_.get()));

  DCHECK(task_runner())
      << "DrmThread::Init -- thread doesn't have a task_runner";

  // DRM thread is running now so can safely handle receiver requests. So drain
  // the queue of as-yet unhandled receiver requests if there are any.
  std::move(complete_early_receiver_requests_).Run();
}

void DrmThread::CreateBuffer(gfx::AcceleratedWidget widget,
                             const gfx::Size& size,
                             const gfx::Size& framebuffer_size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             uint32_t client_flags,
                             std::unique_ptr<GbmBuffer>* buffer,
                             scoped_refptr<DrmFramebuffer>* framebuffer) {
  TRACE_EVENT0("drm", "DrmThread::CreateBuffer");
  scoped_refptr<ui::DrmDevice> drm = device_manager_->GetDrmDevice(widget);
  CHECK(drm) << "No devices available for buffer allocation.";

  DrmWindow* window = screen_manager_->GetWindow(widget);
  uint32_t flags = ui::BufferUsageToGbmFlags(usage);
  uint32_t fourcc_format = ui::GetFourCCFormatFromBufferFormat(format);

  // TODO(hoegsberg): We shouldn't really get here without a window,
  // but it happens during init. Need to figure out why.
  std::vector<uint64_t> modifiers;
  if (window && window->GetController() && !(flags & GBM_BO_USE_LINEAR) &&
      !(client_flags & GbmPixmap::kFlagNoModifiers)) {
    modifiers = window->GetController()->GetFormatModifiers(fourcc_format);
  }

  CreateBufferWithGbmFlags(drm, fourcc_format, size, framebuffer_size, flags,
                           modifiers, buffer, framebuffer);

  // NOTE: BufferUsage::SCANOUT is used to create buffers that will be
  // explicitly set via kms on a CRTC (e.g: BufferQueue buffers), therefore
  // allocation should fail if it's not possible to allocate a BO_USE_SCANOUT
  // buffer in that case.
  if (!*buffer && usage != gfx::BufferUsage::SCANOUT) {
    flags &= ~GBM_BO_USE_SCANOUT;
    CreateBufferWithGbmFlags(drm, fourcc_format, size, framebuffer_size, flags,
                             modifiers, buffer, framebuffer);
  }
}

void DrmThread::CreateBufferAsync(gfx::AcceleratedWidget widget,
                                  const gfx::Size& size,
                                  gfx::BufferFormat format,
                                  gfx::BufferUsage usage,
                                  uint32_t client_flags,
                                  CreateBufferAsyncCallback callback) {
  TRACE_EVENT0("drm", "DrmThread::CreateBufferAsync");
  std::unique_ptr<GbmBuffer> buffer;
  scoped_refptr<DrmFramebuffer> framebuffer;
  CreateBuffer(widget, size, /*framebuffer_size=*/size, format, usage,
               client_flags, &buffer, &framebuffer);
  std::move(callback).Run(std::move(buffer), std::move(framebuffer));
}

void DrmThread::CreateBufferFromHandle(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::BufferFormat format,
    gfx::NativePixmapHandle handle,
    std::unique_ptr<GbmBuffer>* out_buffer,
    scoped_refptr<DrmFramebuffer>* out_framebuffer) {
  TRACE_EVENT0("drm", "DrmThread::CreateBufferFromHandle");
  scoped_refptr<ui::DrmDevice> drm = device_manager_->GetDrmDevice(widget);
  DCHECK(drm);

  std::unique_ptr<GbmBuffer> buffer = drm->gbm_device()->CreateBufferFromHandle(
      ui::GetFourCCFormatFromBufferFormat(format), size, std::move(handle));
  if (!buffer)
    return;

  scoped_refptr<DrmFramebuffer> framebuffer;
  if (buffer->GetFlags() & GBM_BO_USE_SCANOUT) {
    // NB: This is not required to succeed; framebuffers are added for
    // imported buffers on a best effort basis.
    framebuffer =
        DrmFramebuffer::AddFramebuffer(drm, buffer.get(), buffer->GetSize());
  }

  *out_buffer = std::move(buffer);
  *out_framebuffer = std::move(framebuffer);
}

void DrmThread::SetClearOverlayCacheCallback(base::RepeatingClosure callback) {
  display_manager_->SetClearOverlayCacheCallback(std::move(callback));
}

void DrmThread::SchedulePageFlip(
    gfx::AcceleratedWidget widget,
    std::vector<DrmOverlayPlane> planes,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback) {
  TRACE_EVENT0("drm", "DrmThread::SchedulePageFlip");
  scoped_refptr<ui::DrmDevice> drm_device =
      device_manager_->GetDrmDevice(widget);

  drm_device->plane_manager()->RequestPlanesReadyCallback(
      std::move(planes), base::BindOnce(&DrmThread::OnPlanesReadyForPageFlip,
                                        weak_ptr_factory_.GetWeakPtr(), widget,
                                        std::move(submission_callback),
                                        std::move(presentation_callback)));
}

void DrmThread::OnPlanesReadyForPageFlip(
    gfx::AcceleratedWidget widget,
    SwapCompletionOnceCallback submission_callback,
    PresentationOnceCallback presentation_callback,
    std::vector<DrmOverlayPlane> planes) {
  TRACE_EVENT0("drm", "DrmThread::OnPlanesReadyForPageFlip");
  DrmWindow* window = screen_manager_->GetWindow(widget);
  if (window) {
    window->SchedulePageFlip(std::move(planes), std::move(submission_callback),
                             std::move(presentation_callback));
  } else {
    std::move(submission_callback).Run(gfx::SwapResult::SWAP_ACK, nullptr);
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
  }
}

void DrmThread::IsDeviceAtomic(gfx::AcceleratedWidget widget, bool* is_atomic) {
  TRACE_EVENT0("drm", "DrmThread::IsDeviceAtomic");
  scoped_refptr<ui::DrmDevice> drm_device =
      device_manager_->GetDrmDevice(widget);

  *is_atomic = drm_device && drm_device->is_atomic();
}

void DrmThread::CreateWindow(gfx::AcceleratedWidget widget,
                             const gfx::Rect& initial_bounds) {
  TRACE_EVENT0("drm", "DrmThread::CreateWindow");
  DCHECK_GT(widget, last_created_window_);
  last_created_window_ = widget;

  std::unique_ptr<DrmWindow> window(
      new DrmWindow(widget, device_manager_.get(), screen_manager_.get()));
  window->Initialize();
  screen_manager_->AddWindow(widget, std::move(window));
  screen_manager_->GetWindow(widget)->SetBounds(initial_bounds);

  // There might be tasks that were waiting for |widget| to become available.
  ProcessPendingTasks();
}

void DrmThread::DestroyWindow(gfx::AcceleratedWidget widget) {
  TRACE_EVENT0("drm", "DrmThread::DestroyWindow");
  std::unique_ptr<DrmWindow> window = screen_manager_->RemoveWindow(widget);
  window->Shutdown();
}

void DrmThread::SetWindowBounds(gfx::AcceleratedWidget widget,
                                const gfx::Rect& bounds) {
  TRACE_EVENT0("drm", "DrmThread::SetWindowBounds");
  screen_manager_->GetWindow(widget)->SetBounds(bounds);
}

void DrmThread::SetCursor(gfx::AcceleratedWidget widget,
                          const std::vector<SkBitmap>& bitmaps,
                          const gfx::Point& location,
                          int32_t frame_delay_ms) {
  TRACE_EVENT0("drm", "DrmThread::SetCursor");
  screen_manager_->GetWindow(widget)
      ->SetCursor(bitmaps, location, frame_delay_ms);
}

void DrmThread::MoveCursor(gfx::AcceleratedWidget widget,
                           const gfx::Point& location) {
  TRACE_EVENT0("drm", "DrmThread::MoveCursor");
  screen_manager_->GetWindow(widget)->MoveCursor(location);
}

void DrmThread::CheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const OverlaySurfaceCandidateList& overlays,
    OverlayCapabilitiesCallback callback) {
  TRACE_EVENT0("drm,hwoverlays", "DrmThread::CheckOverlayCapabilities");

  std::vector<OverlayStatus> result;
  CheckOverlayCapabilitiesSync(widget, overlays, &result);
  std::move(callback).Run(widget, overlays, std::move(result));
}

void DrmThread::CheckOverlayCapabilitiesSync(
    gfx::AcceleratedWidget widget,
    const OverlaySurfaceCandidateList& overlays,
    std::vector<OverlayStatus>* result) {
  TRACE_EVENT0("drm,hwoverlays", "DrmThread::CheckOverlayCapabilitiesSync");

  DrmWindow* window = screen_manager_->GetWindow(widget);
  if (!window) {
    result->clear();
    result->insert(result->end(), overlays.size(), OVERLAY_STATUS_NOT);
    return;
  }
  *result = window->TestPageFlip(overlays);
}

void DrmThread::GetDeviceCursor(
    mojo::PendingAssociatedReceiver<ozone::mojom::DeviceCursor> receiver) {
  cursor_receivers_.Add(this, std::move(receiver));
}

void DrmThread::RefreshNativeDisplays(
    base::OnceCallback<void(MovableDisplaySnapshots)> callback) {
  TRACE_EVENT0("drm", "DrmThread::RefreshNativeDisplays");
  std::move(callback).Run(display_manager_->GetDisplays());
}

void DrmThread::ConfigureNativeDisplay(
    int64_t id,
    std::unique_ptr<display::DisplayMode> mode,
    const gfx::Point& origin,
    base::OnceCallback<void(int64_t, bool)> callback) {
  TRACE_EVENT0("drm", "DrmThread::ConfigureNativeDisplay");
  std::move(callback).Run(
      id, display_manager_->ConfigureDisplay(id, *mode, origin));
}

void DrmThread::DisableNativeDisplay(
    int64_t id,
    base::OnceCallback<void(int64_t, bool)> callback) {
  TRACE_EVENT0("drm", "DrmThread::DisableNativeDisplay");
  std::move(callback).Run(id, display_manager_->DisableDisplay(id));
}

void DrmThread::TakeDisplayControl(base::OnceCallback<void(bool)> callback) {
  TRACE_EVENT0("drm", "DrmThread::TakeDisplayControl");
  std::move(callback).Run(display_manager_->TakeDisplayControl());
}

void DrmThread::RelinquishDisplayControl(
    base::OnceCallback<void(bool)> callback) {
  TRACE_EVENT0("drm", "DrmThread::RelinquishDisplayControl");
  display_manager_->RelinquishDisplayControl();
  std::move(callback).Run(true);
}

void DrmThread::AddGraphicsDevice(const base::FilePath& path, base::File file) {
  TRACE_EVENT0("drm", "DrmThread::AddGraphicsDevice");
  device_manager_->AddDrmDevice(path, std::move(file));

  // There might be tasks that were blocked on a DrmDevice becoming available.
  ProcessPendingTasks();
}

void DrmThread::RemoveGraphicsDevice(const base::FilePath& path) {
  TRACE_EVENT0("drm", "DrmThread::RemoveGraphicsDevice");
  device_manager_->RemoveDrmDevice(path);
}

void DrmThread::GetHDCPState(
    int64_t display_id,
    base::OnceCallback<void(int64_t, bool, display::HDCPState)> callback) {
  TRACE_EVENT0("drm", "DrmThread::GetHDCPState");
  display::HDCPState state = display::HDCP_STATE_UNDESIRED;
  bool success = display_manager_->GetHDCPState(display_id, &state);
  std::move(callback).Run(display_id, success, state);
}

void DrmThread::SetHDCPState(int64_t display_id,
                             display::HDCPState state,
                             base::OnceCallback<void(int64_t, bool)> callback) {
  TRACE_EVENT0("drm", "DrmThread::SetHDCPState");
  std::move(callback).Run(display_id,
                          display_manager_->SetHDCPState(display_id, state));
}

void DrmThread::SetColorMatrix(int64_t display_id,
                               const std::vector<float>& color_matrix) {
  TRACE_EVENT0("drm", "DrmThread::SetColorMatrix");
  display_manager_->SetColorMatrix(display_id, color_matrix);
}

void DrmThread::SetGammaCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut) {
  TRACE_EVENT0("drm", "DrmThread::SetGammaCorrection");
  display_manager_->SetGammaCorrection(display_id, degamma_lut, gamma_lut);
}

void DrmThread::SetPrivacyScreen(int64_t display_id, bool enabled) {
  display_manager_->SetPrivacyScreen(display_id, enabled);
}

void DrmThread::AddDrmDeviceReceiver(
    mojo::PendingReceiver<ozone::mojom::DrmDevice> receiver) {
  TRACE_EVENT0("drm", "DrmThread::AddDrmDeviceReceiver");
  drm_receivers_.Add(this, std::move(receiver));
}

void DrmThread::ProcessPendingTasks() {
  DCHECK(!device_manager_->GetDrmDevices().empty());

  for (auto& task_info : pending_tasks_) {
    std::move(task_info.task).Run();
    if (task_info.done)
      task_info.done->Signal();
  }

  pending_tasks_.clear();
}

}  // namespace ui
