// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_thread.h"

#include <gbm.h>
#include <utility>

#include "base/command_line.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/message_loop/message_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot_mojo.h"
#include "ui/ozone/common/display_snapshot_proxy.h"
#include "ui/ozone/platform/drm/common/drm_util.h"
#include "ui/ozone/platform/drm/gpu/drm_buffer.h"
#include "ui/ozone/platform/drm/gpu/drm_device_generator.h"
#include "ui/ozone/platform/drm/gpu/drm_device_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_gpu_display_manager.h"
#include "ui/ozone/platform/drm/gpu/drm_window.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_buffer.h"
#include "ui/ozone/platform/drm/gpu/gbm_device.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"
#include "ui/ozone/platform/drm/gpu/proxy_helpers.h"
#include "ui/ozone/platform/drm/gpu/screen_manager.h"
#include "ui/ozone/public/ozone_switches.h"

namespace ui {

namespace {

class GbmBufferGenerator : public ScanoutBufferGenerator {
 public:
  GbmBufferGenerator() {}
  ~GbmBufferGenerator() override {}

  // ScanoutBufferGenerator:
  scoped_refptr<ScanoutBuffer> Create(const scoped_refptr<DrmDevice>& drm,
                                      uint32_t format,
                                      const gfx::Size& size) override {
    scoped_refptr<GbmDevice> gbm(static_cast<GbmDevice*>(drm.get()));
    // TODO(dcastagna): Use GBM_BO_USE_MAP modifier once minigbm exposes it.
    return GbmBuffer::CreateBuffer(gbm, format, size, GBM_BO_USE_SCANOUT);
  }

 protected:
  DISALLOW_COPY_AND_ASSIGN(GbmBufferGenerator);
};

class GbmDeviceGenerator : public DrmDeviceGenerator {
 public:
  GbmDeviceGenerator(bool use_atomic) : use_atomic_(use_atomic) {}
  ~GbmDeviceGenerator() override {}

  // DrmDeviceGenerator:
  scoped_refptr<DrmDevice> CreateDevice(const base::FilePath& path,
                                        base::File file,
                                        bool is_primary_device) override {
    scoped_refptr<DrmDevice> drm =
        new GbmDevice(path, std::move(file), is_primary_device);
    if (drm->Initialize(use_atomic_))
      return drm;

    return nullptr;
  }

 private:
  bool use_atomic_;

  DISALLOW_COPY_AND_ASSIGN(GbmDeviceGenerator);
};

}  // namespace

DrmThread::DrmThread() : base::Thread("DrmThread") {}

DrmThread::~DrmThread() {
  Stop();
}

void DrmThread::Start() {
  base::Thread::Options thread_options;
  thread_options.message_loop_type = base::MessageLoop::TYPE_IO;
  thread_options.priority = base::ThreadPriority::DISPLAY;
  if (!StartWithOptions(thread_options))
    LOG(FATAL) << "Failed to create DRM thread";
}

void DrmThread::Init() {
  bool use_atomic = false;
  use_atomic = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableDrmAtomic);

  device_manager_.reset(
      new DrmDeviceManager(base::MakeUnique<GbmDeviceGenerator>(use_atomic)));
  buffer_generator_.reset(new GbmBufferGenerator());
  screen_manager_.reset(new ScreenManager(buffer_generator_.get()));

  display_manager_.reset(
      new DrmGpuDisplayManager(screen_manager_.get(), device_manager_.get()));
}

void DrmThread::CreateBuffer(gfx::AcceleratedWidget widget,
                             const gfx::Size& size,
                             gfx::BufferFormat format,
                             gfx::BufferUsage usage,
                             scoped_refptr<GbmBuffer>* buffer) {
  scoped_refptr<GbmDevice> gbm =
      static_cast<GbmDevice*>(device_manager_->GetDrmDevice(widget).get());
  DCHECK(gbm);

  uint32_t flags = 0;
  switch (usage) {
    case gfx::BufferUsage::GPU_READ:
      break;
    case gfx::BufferUsage::SCANOUT:
      flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING;
      break;
    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
      flags = GBM_BO_USE_SCANOUT | GBM_BO_USE_LINEAR;
      break;
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE_PERSISTENT:
      flags = GBM_BO_USE_LINEAR;
      break;
  }

  DrmWindow* window = screen_manager_->GetWindow(widget);
  std::vector<uint64_t> modifiers;

  uint32_t fourcc_format = ui::GetFourCCFormatFromBufferFormat(format);

  // TODO(hoegsberg): We shouldn't really get here without a window,
  // but it happens during init. Need to figure out why.
  if (window && window->GetController())
    modifiers = window->GetController()->GetFormatModifiers(fourcc_format);

  if (modifiers.size() > 0 && !(flags & GBM_BO_USE_LINEAR))
    *buffer = GbmBuffer::CreateBufferWithModifiers(gbm, fourcc_format, size,
                                                   flags, modifiers);
  else
    *buffer = GbmBuffer::CreateBuffer(gbm, fourcc_format, size, flags);
}

void DrmThread::CreateBufferFromFds(
    gfx::AcceleratedWidget widget,
    const gfx::Size& size,
    gfx::BufferFormat format,
    std::vector<base::ScopedFD>&& fds,
    const std::vector<gfx::NativePixmapPlane>& planes,
    scoped_refptr<GbmBuffer>* buffer) {
  scoped_refptr<GbmDevice> gbm =
      static_cast<GbmDevice*>(device_manager_->GetDrmDevice(widget).get());
  DCHECK(gbm);
  *buffer = GbmBuffer::CreateBufferFromFds(
      gbm, ui::GetFourCCFormatFromBufferFormat(format), size, std::move(fds),
      planes);
}

void DrmThread::GetScanoutFormats(
    gfx::AcceleratedWidget widget,
    std::vector<gfx::BufferFormat>* scanout_formats) {
  display_manager_->GetScanoutFormats(widget, scanout_formats);
}

void DrmThread::SchedulePageFlip(gfx::AcceleratedWidget widget,
                                 const std::vector<OverlayPlane>& planes,
                                 SwapCompletionOnceCallback callback) {
  DrmWindow* window = screen_manager_->GetWindow(widget);
  if (window)
    window->SchedulePageFlip(planes, std::move(callback));
  else
    std::move(callback).Run(gfx::SwapResult::SWAP_ACK);
}

void DrmThread::GetVSyncParameters(
    gfx::AcceleratedWidget widget,
    const gfx::VSyncProvider::UpdateVSyncCallback& callback) {
  DrmWindow* window = screen_manager_->GetWindow(widget);
  // No need to call the callback if there isn't a window since the vsync
  // provider doesn't require the callback to be called if there isn't a vsync
  // data source.
  if (window)
    window->GetVSyncParameters(callback);
}

void DrmThread::CreateWindow(gfx::AcceleratedWidget widget) {
  std::unique_ptr<DrmWindow> window(
      new DrmWindow(widget, device_manager_.get(), screen_manager_.get()));
  window->Initialize(buffer_generator_.get());
  screen_manager_->AddWindow(widget, std::move(window));
}

void DrmThread::DestroyWindow(gfx::AcceleratedWidget widget) {
  std::unique_ptr<DrmWindow> window = screen_manager_->RemoveWindow(widget);
  window->Shutdown();
}

void DrmThread::SetWindowBounds(gfx::AcceleratedWidget widget,
                                const gfx::Rect& bounds) {
  screen_manager_->GetWindow(widget)->SetBounds(bounds);
}

void DrmThread::SetCursor(const gfx::AcceleratedWidget& widget,
                          const std::vector<SkBitmap>& bitmaps,
                          const gfx::Point& location,
                          int32_t frame_delay_ms) {
  screen_manager_->GetWindow(widget)
      ->SetCursor(bitmaps, location, frame_delay_ms);
}

void DrmThread::MoveCursor(const gfx::AcceleratedWidget& widget,
                           const gfx::Point& location) {
  screen_manager_->GetWindow(widget)->MoveCursor(location);
}

void DrmThread::CheckOverlayCapabilities(
    gfx::AcceleratedWidget widget,
    const std::vector<OverlayCheck_Params>& overlays,
    base::OnceCallback<void(gfx::AcceleratedWidget,
                            const std::vector<OverlayCheck_Params>&,
                            const std::vector<OverlayCheckReturn_Params>&)>
        callback) {
  std::move(callback).Run(
      widget, overlays,
      screen_manager_->GetWindow(widget)->TestPageFlip(overlays));
}

void DrmThread::RefreshNativeDisplays(
    base::OnceCallback<void(MovableDisplaySnapshots)> callback) {
  auto snapshots =
      CreateMovableDisplaySnapshotsFromParams(display_manager_->GetDisplays());
  std::move(callback).Run(std::move(snapshots));
}

void DrmThread::ConfigureNativeDisplay(
    int64_t id,
    std::unique_ptr<display::DisplayMode> mode,
    const gfx::Point& origin,
    base::OnceCallback<void(int64_t, bool)> callback) {
  std::move(callback).Run(
      id, display_manager_->ConfigureDisplay(id, *mode, origin));
}

void DrmThread::DisableNativeDisplay(
    int64_t id,
    base::OnceCallback<void(int64_t, bool)> callback) {
  std::move(callback).Run(id, display_manager_->DisableDisplay(id));
}

void DrmThread::TakeDisplayControl(base::OnceCallback<void(bool)> callback) {
  std::move(callback).Run(display_manager_->TakeDisplayControl());
}

void DrmThread::RelinquishDisplayControl(
    base::OnceCallback<void(bool)> callback) {
  display_manager_->RelinquishDisplayControl();
  std::move(callback).Run(true);
}

void DrmThread::AddGraphicsDevice(const base::FilePath& path,
                                  const base::FileDescriptor& fd) {
  device_manager_->AddDrmDevice(path, fd);
}

void DrmThread::RemoveGraphicsDevice(const base::FilePath& path) {
  device_manager_->RemoveDrmDevice(path);
}

void DrmThread::GetHDCPState(
    int64_t display_id,
    base::OnceCallback<void(int64_t, bool, display::HDCPState)> callback) {
  display::HDCPState state = display::HDCP_STATE_UNDESIRED;
  bool success = display_manager_->GetHDCPState(display_id, &state);
  std::move(callback).Run(display_id, success, state);
}

void DrmThread::SetHDCPState(int64_t display_id,
                             display::HDCPState state,
                             base::OnceCallback<void(int64_t, bool)> callback) {
  std::move(callback).Run(display_id,
                          display_manager_->SetHDCPState(display_id, state));
}

void DrmThread::SetColorCorrection(
    int64_t display_id,
    const std::vector<display::GammaRampRGBEntry>& degamma_lut,
    const std::vector<display::GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  display_manager_->SetColorCorrection(display_id, degamma_lut, gamma_lut,
                                       correction_matrix);
}

// DrmThread requires a BindingSet instead of a simple Binding because it will
// be used from multiple threads in multiple processes.
void DrmThread::AddBinding(ozone::mojom::DeviceCursorRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

}  // namespace ui
