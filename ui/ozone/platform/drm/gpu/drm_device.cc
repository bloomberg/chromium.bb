// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/drm_device.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/ozone/platform/drm/gpu/drm_util.h"
#include "ui/ozone/platform/drm/gpu/hardware_display_plane_manager_legacy.h"

namespace ui {

namespace {

struct PageFlipPayload {
  PageFlipPayload(const scoped_refptr<base::TaskRunner>& task_runner,
                  const DrmDevice::PageFlipCallback& callback)
      : task_runner(task_runner), callback(callback) {}

  // Task runner for the thread scheduling the page flip event. This is used to
  // run the callback on the same thread the callback was created on.
  scoped_refptr<base::TaskRunner> task_runner;
  DrmDevice::PageFlipCallback callback;
};

bool DrmCreateDumbBuffer(int fd,
                         const SkImageInfo& info,
                         uint32_t* handle,
                         uint32_t* stride) {
  struct drm_mode_create_dumb request;
  memset(&request, 0, sizeof(request));
  request.width = info.width();
  request.height = info.height();
  request.bpp = info.bytesPerPixel() << 3;
  request.flags = 0;

  if (drmIoctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &request) < 0) {
    VLOG(2) << "Cannot create dumb buffer (" << errno << ") "
            << strerror(errno);
    return false;
  }

  // The driver may choose to align the last row as well. We don't care about
  // the last alignment bits since they aren't used for display purposes, so
  // just check that the expected size is <= to what the driver allocated.
  DCHECK_LE(info.getSafeSize(request.pitch), request.size);

  *handle = request.handle;
  *stride = request.pitch;
  return true;
}

void DrmDestroyDumbBuffer(int fd, uint32_t handle) {
  struct drm_mode_destroy_dumb destroy_request;
  memset(&destroy_request, 0, sizeof(destroy_request));
  destroy_request.handle = handle;
  drmIoctl(fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy_request);
}

void HandlePageFlipEventOnIO(int fd,
                             unsigned int frame,
                             unsigned int seconds,
                             unsigned int useconds,
                             void* data) {
  scoped_ptr<PageFlipPayload> payload(static_cast<PageFlipPayload*>(data));
  payload->task_runner->PostTask(
      FROM_HERE, base::Bind(payload->callback, frame, seconds, useconds));
}

void HandlePageFlipEventOnUI(int fd,
                             unsigned int frame,
                             unsigned int seconds,
                             unsigned int useconds,
                             void* data) {
  scoped_ptr<PageFlipPayload> payload(static_cast<PageFlipPayload*>(data));
  payload->callback.Run(frame, seconds, useconds);
}

bool CanQueryForResources(int fd) {
  drm_mode_card_res resources;
  memset(&resources, 0, sizeof(resources));
  // If there is no error getting DRM resources then assume this is a
  // modesetting device.
  return !drmIoctl(fd, DRM_IOCTL_MODE_GETRESOURCES, &resources);
}

}  // namespace

class DrmDevice::IOWatcher
    : public base::RefCountedThreadSafe<DrmDevice::IOWatcher>,
      public base::MessagePumpLibevent::Watcher {
 public:
  IOWatcher(int fd,
            const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      : io_task_runner_(io_task_runner), paused_(true), fd_(fd) {}

  void SetPaused(bool paused) {
    if (paused_ == paused)
      return;

    paused_ = paused;
    base::WaitableEvent done(false, false);
    io_task_runner_->PostTask(
        FROM_HERE, base::Bind(&IOWatcher::SetPausedOnIO, this, &done));
  }

  void Shutdown() {
    if (!paused_)
      io_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&IOWatcher::UnregisterOnIO, this));
  }

 private:
  friend class base::RefCountedThreadSafe<IOWatcher>;

  ~IOWatcher() override { SetPaused(true); }

  void RegisterOnIO() {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        fd_, true, base::MessageLoopForIO::WATCH_READ, &controller_, this);
  }

  void UnregisterOnIO() {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    controller_.StopWatchingFileDescriptor();
  }

  void SetPausedOnIO(base::WaitableEvent* done) {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    if (paused_)
      UnregisterOnIO();
    else
      RegisterOnIO();
    done->Signal();
  }

  // base::MessagePumpLibevent::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    TRACE_EVENT1("drm", "OnDrmEvent", "socket", fd);

    drmEventContext event;
    event.version = DRM_EVENT_CONTEXT_VERSION;
    event.page_flip_handler = HandlePageFlipEventOnIO;
    event.vblank_handler = nullptr;

    drmHandleEvent(fd, &event);
  }

  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  bool paused_;
  int fd_;

  DISALLOW_COPY_AND_ASSIGN(IOWatcher);
};

DrmDevice::DrmDevice(const base::FilePath& device_path)
    : device_path_(device_path),
      file_(device_path,
            base::File::FLAG_OPEN | base::File::FLAG_READ |
                base::File::FLAG_WRITE) {
  LOG_IF(FATAL, !file_.IsValid())
      << "Failed to open '" << device_path_.value()
      << "': " << base::File::ErrorToString(file_.error_details());
}

DrmDevice::DrmDevice(const base::FilePath& device_path, base::File file)
    : device_path_(device_path), file_(file.Pass()) {
}

DrmDevice::~DrmDevice() {
  if (watcher_)
    watcher_->Shutdown();
}

bool DrmDevice::Initialize() {
  // Ignore devices that cannot perform modesetting.
  if (!CanQueryForResources(file_.GetPlatformFile())) {
    VLOG(2) << "Cannot query for resources for '" << device_path_.value()
            << "'";
    return false;
  }

  plane_manager_.reset(new HardwareDisplayPlaneManagerLegacy());
  if (!plane_manager_->Initialize(this)) {
    LOG(ERROR) << "Failed to initialize the plane manager for "
               << device_path_.value();
    plane_manager_.reset();
    return false;
  }

  return true;
}

void DrmDevice::InitializeTaskRunner(
    const scoped_refptr<base::SingleThreadTaskRunner>& task_runner) {
  DCHECK(!task_runner_);
  task_runner_ = task_runner;
  watcher_ = new IOWatcher(file_.GetPlatformFile(), task_runner_);
}

ScopedDrmCrtcPtr DrmDevice::GetCrtc(uint32_t crtc_id) {
  DCHECK(file_.IsValid());
  return ScopedDrmCrtcPtr(drmModeGetCrtc(file_.GetPlatformFile(), crtc_id));
}

bool DrmDevice::SetCrtc(uint32_t crtc_id,
                        uint32_t framebuffer,
                        std::vector<uint32_t> connectors,
                        drmModeModeInfo* mode) {
  DCHECK(file_.IsValid());
  DCHECK(!connectors.empty());
  DCHECK(mode);

  TRACE_EVENT2("drm", "DrmDevice::SetCrtc", "crtc", crtc_id, "size",
               gfx::Size(mode->hdisplay, mode->vdisplay).ToString());
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, framebuffer, 0, 0,
                         vector_as_array(&connectors), connectors.size(), mode);
}

bool DrmDevice::SetCrtc(drmModeCrtc* crtc, std::vector<uint32_t> connectors) {
  DCHECK(file_.IsValid());
  // If there's no buffer then the CRTC was disabled.
  if (!crtc->buffer_id)
    return DisableCrtc(crtc->crtc_id);

  DCHECK(!connectors.empty());

  TRACE_EVENT1("drm", "DrmDevice::RestoreCrtc", "crtc", crtc->crtc_id);
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc->crtc_id,
                         crtc->buffer_id, crtc->x, crtc->y,
                         vector_as_array(&connectors), connectors.size(),
                         &crtc->mode);
}

bool DrmDevice::DisableCrtc(uint32_t crtc_id) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::DisableCrtc", "crtc", crtc_id);
  return !drmModeSetCrtc(file_.GetPlatformFile(), crtc_id, 0, 0, 0, NULL, 0,
                         NULL);
}

ScopedDrmConnectorPtr DrmDevice::GetConnector(uint32_t connector_id) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::GetConnector", "connector", connector_id);
  return ScopedDrmConnectorPtr(
      drmModeGetConnector(file_.GetPlatformFile(), connector_id));
}

bool DrmDevice::AddFramebuffer(uint32_t width,
                               uint32_t height,
                               uint8_t depth,
                               uint8_t bpp,
                               uint32_t stride,
                               uint32_t handle,
                               uint32_t* framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::AddFramebuffer", "handle", handle);
  return !drmModeAddFB(file_.GetPlatformFile(), width, height, depth, bpp,
                       stride, handle, framebuffer);
}

bool DrmDevice::RemoveFramebuffer(uint32_t framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::RemoveFramebuffer", "framebuffer",
               framebuffer);
  return !drmModeRmFB(file_.GetPlatformFile(), framebuffer);
}

bool DrmDevice::PageFlip(uint32_t crtc_id,
                         uint32_t framebuffer,
                         bool is_sync,
                         const PageFlipCallback& callback) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::PageFlip", "crtc", crtc_id, "framebuffer",
               framebuffer);

  if (watcher_)
    watcher_->SetPaused(is_sync);

  // NOTE: Calling drmModeSetCrtc will immediately update the state, though
  // callbacks to already scheduled page flips will be honored by the kernel.
  scoped_ptr<PageFlipPayload> payload(
      new PageFlipPayload(base::ThreadTaskRunnerHandle::Get(), callback));
  if (!drmModePageFlip(file_.GetPlatformFile(), crtc_id, framebuffer,
                       DRM_MODE_PAGE_FLIP_EVENT, payload.get())) {
    // If successful the payload will be removed by a PageFlip event.
    ignore_result(payload.release());

    // If the flip was requested synchronous or if no watcher has been installed
    // yet, then synchronously handle the page flip events.
    if (is_sync || !watcher_) {
      TRACE_EVENT1("drm", "OnDrmEvent", "socket", file_.GetPlatformFile());

      drmEventContext event;
      event.version = DRM_EVENT_CONTEXT_VERSION;
      event.page_flip_handler = HandlePageFlipEventOnUI;
      event.vblank_handler = nullptr;

      drmHandleEvent(file_.GetPlatformFile(), &event);
    }

    return true;
  }

  return false;
}

bool DrmDevice::PageFlipOverlay(uint32_t crtc_id,
                                uint32_t framebuffer,
                                const gfx::Rect& location,
                                const gfx::Rect& source,
                                int overlay_plane) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::PageFlipOverlay", "crtc", crtc_id,
               "framebuffer", framebuffer);
  return !drmModeSetPlane(file_.GetPlatformFile(), overlay_plane, crtc_id,
                          framebuffer, 0, location.x(), location.y(),
                          location.width(), location.height(), source.x(),
                          source.y(), source.width(), source.height());
}

ScopedDrmFramebufferPtr DrmDevice::GetFramebuffer(uint32_t framebuffer) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::GetFramebuffer", "framebuffer", framebuffer);
  return ScopedDrmFramebufferPtr(
      drmModeGetFB(file_.GetPlatformFile(), framebuffer));
}

ScopedDrmPropertyPtr DrmDevice::GetProperty(drmModeConnector* connector,
                                            const char* name) {
  TRACE_EVENT2("drm", "DrmDevice::GetProperty", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0)
      return property.Pass();
  }

  return ScopedDrmPropertyPtr();
}

bool DrmDevice::SetProperty(uint32_t connector_id,
                            uint32_t property_id,
                            uint64_t value) {
  DCHECK(file_.IsValid());
  return !drmModeConnectorSetProperty(file_.GetPlatformFile(), connector_id,
                                      property_id, value);
}

bool DrmDevice::GetCapability(uint64_t capability, uint64_t* value) {
  DCHECK(file_.IsValid());
  return !drmGetCap(file_.GetPlatformFile(), capability, value);
}

ScopedDrmPropertyBlobPtr DrmDevice::GetPropertyBlob(drmModeConnector* connector,
                                                    const char* name) {
  DCHECK(file_.IsValid());
  TRACE_EVENT2("drm", "DrmDevice::GetPropertyBlob", "connector",
               connector->connector_id, "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(
        drmModeGetProperty(file_.GetPlatformFile(), connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0 &&
        property->flags & DRM_MODE_PROP_BLOB)
      return ScopedDrmPropertyBlobPtr(drmModeGetPropertyBlob(
          file_.GetPlatformFile(), connector->prop_values[i]));
  }

  return ScopedDrmPropertyBlobPtr();
}

bool DrmDevice::SetCursor(uint32_t crtc_id,
                          uint32_t handle,
                          const gfx::Size& size) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::SetCursor", "handle", handle);
  return !drmModeSetCursor(file_.GetPlatformFile(), crtc_id, handle,
                           size.width(), size.height());
}

bool DrmDevice::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  DCHECK(file_.IsValid());
  return !drmModeMoveCursor(file_.GetPlatformFile(), crtc_id, point.x(),
                            point.y());
}

bool DrmDevice::CreateDumbBuffer(const SkImageInfo& info,
                                 uint32_t* handle,
                                 uint32_t* stride,
                                 void** pixels) {
  DCHECK(file_.IsValid());

  TRACE_EVENT0("drm", "DrmDevice::CreateDumbBuffer");
  if (!DrmCreateDumbBuffer(file_.GetPlatformFile(), info, handle, stride))
    return false;

  if (!MapDumbBuffer(file_.GetPlatformFile(), *handle,
                     info.getSafeSize(*stride), pixels)) {
    DrmDestroyDumbBuffer(file_.GetPlatformFile(), *handle);
    return false;
  }

  return true;
}

void DrmDevice::DestroyDumbBuffer(const SkImageInfo& info,
                                  uint32_t handle,
                                  uint32_t stride,
                                  void* pixels) {
  DCHECK(file_.IsValid());
  TRACE_EVENT1("drm", "DrmDevice::DestroyDumbBuffer", "handle", handle);
  munmap(pixels, info.getSafeSize(stride));
  DrmDestroyDumbBuffer(file_.GetPlatformFile(), handle);
}

bool DrmDevice::SetMaster() {
  DCHECK(file_.IsValid());
  return (drmSetMaster(file_.GetPlatformFile()) == 0);
}

bool DrmDevice::DropMaster() {
  DCHECK(file_.IsValid());
  return (drmDropMaster(file_.GetPlatformFile()) == 0);
}

}  // namespace ui
