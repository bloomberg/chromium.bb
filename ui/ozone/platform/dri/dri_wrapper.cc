// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_wrapper.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "base/stl_util.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/hardware_display_plane_manager_legacy.h"

namespace ui {

namespace {

struct PageFlipPayload {
  PageFlipPayload(const scoped_refptr<base::TaskRunner>& task_runner,
                  const DriWrapper::PageFlipCallback& callback)
      : task_runner(task_runner), callback(callback) {}

  // Task runner for the thread scheduling the page flip event. This is used to
  // run the callback on the same thread the callback was created on.
  scoped_refptr<base::TaskRunner> task_runner;
  DriWrapper::PageFlipCallback callback;
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

}  // namespace

class DriWrapper::IOWatcher
    : public base::RefCountedThreadSafe<DriWrapper::IOWatcher>,
      public base::MessagePumpLibevent::Watcher {
 public:
  IOWatcher(int fd,
            const scoped_refptr<base::SingleThreadTaskRunner>& io_task_runner)
      : io_task_runner_(io_task_runner) {
    io_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&IOWatcher::RegisterOnIO, this, fd));
  }

  void Shutdown() {
    io_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&IOWatcher::UnregisterOnIO, this));
  }

 private:
  friend class base::RefCountedThreadSafe<IOWatcher>;

  ~IOWatcher() override {}

  void RegisterOnIO(int fd) {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    base::MessageLoopForIO::current()->WatchFileDescriptor(
        fd, true, base::MessageLoopForIO::WATCH_READ, &controller_, this);
  }

  void UnregisterOnIO() {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    controller_.StopWatchingFileDescriptor();
  }

  // base::MessagePumpLibevent::Watcher overrides:
  void OnFileCanReadWithoutBlocking(int fd) override {
    DCHECK(base::MessageLoopForIO::IsCurrent());
    TRACE_EVENT1("dri", "OnDrmEvent", "socket", fd);

    drmEventContext event;
    event.version = DRM_EVENT_CONTEXT_VERSION;
    event.page_flip_handler = HandlePageFlipEventOnIO;
    event.vblank_handler = nullptr;

    drmHandleEvent(fd, &event);
  }

  void OnFileCanWriteWithoutBlocking(int fd) override { NOTREACHED(); }

  scoped_refptr<base::SingleThreadTaskRunner> io_task_runner_;

  base::MessagePumpLibevent::FileDescriptorWatcher controller_;

  DISALLOW_COPY_AND_ASSIGN(IOWatcher);
};

DriWrapper::DriWrapper(const char* device_path, bool use_sync_flips)
    : fd_(-1),
      use_sync_flips_(use_sync_flips),
      device_path_(device_path),
      io_thread_("DriIOThread") {
  plane_manager_.reset(new HardwareDisplayPlaneManagerLegacy());
}

DriWrapper::~DriWrapper() {
  if (fd_ >= 0)
    close(fd_);

  if (watcher_)
    watcher_->Shutdown();
}

void DriWrapper::Initialize() {
  fd_ = open(device_path_, O_RDWR | O_CLOEXEC);
  if (fd_ < 0)
    PLOG(FATAL) << "open: " << device_path_;
  if (!plane_manager_->Initialize(this))
    LOG(ERROR) << "Failed to initialize the plane manager";
}

void DriWrapper::InitializeIOWatcher() {
  if (!use_sync_flips_ && !watcher_) {
    if (!io_thread_.StartWithOptions(
            base::Thread::Options(base::MessageLoop::TYPE_IO, 0)))
      LOG(FATAL) << "Failed to start the IO helper thread";

    watcher_ = new IOWatcher(fd_, io_thread_.task_runner());
  }
}

ScopedDrmCrtcPtr DriWrapper::GetCrtc(uint32_t crtc_id) {
  DCHECK(fd_ >= 0);
  return ScopedDrmCrtcPtr(drmModeGetCrtc(fd_, crtc_id));
}

bool DriWrapper::SetCrtc(uint32_t crtc_id,
                         uint32_t framebuffer,
                         std::vector<uint32_t> connectors,
                         drmModeModeInfo* mode) {
  DCHECK(fd_ >= 0);
  DCHECK(!connectors.empty());
  DCHECK(mode);

  TRACE_EVENT2("dri", "DriWrapper::SetCrtc", "crtc", crtc_id, "size",
               gfx::Size(mode->hdisplay, mode->vdisplay).ToString());
  return !drmModeSetCrtc(fd_,
                         crtc_id,
                         framebuffer,
                         0,
                         0,
                         vector_as_array(&connectors),
                         connectors.size(), mode);
}

bool DriWrapper::SetCrtc(drmModeCrtc* crtc, std::vector<uint32_t> connectors) {
  DCHECK(fd_ >= 0);
  // If there's no buffer then the CRTC was disabled.
  if (!crtc->buffer_id)
    return DisableCrtc(crtc->crtc_id);

  DCHECK(!connectors.empty());

  TRACE_EVENT1("dri", "DriWrapper::RestoreCrtc",
               "crtc", crtc->crtc_id);
  return !drmModeSetCrtc(fd_,
                         crtc->crtc_id,
                         crtc->buffer_id,
                         crtc->x,
                         crtc->y,
                         vector_as_array(&connectors),
                         connectors.size(),
                         &crtc->mode);
}

bool DriWrapper::DisableCrtc(uint32_t crtc_id) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT1("dri", "DriWrapper::DisableCrtc",
               "crtc", crtc_id);
  return !drmModeSetCrtc(fd_, crtc_id, 0, 0, 0, NULL, 0, NULL);
}

ScopedDrmConnectorPtr DriWrapper::GetConnector(uint32_t connector_id) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT1("dri", "DriWrapper::GetConnector", "connector", connector_id);
  return ScopedDrmConnectorPtr(drmModeGetConnector(fd_, connector_id));
}

bool DriWrapper::AddFramebuffer(uint32_t width,
                                uint32_t height,
                                uint8_t depth,
                                uint8_t bpp,
                                uint32_t stride,
                                uint32_t handle,
                                uint32_t* framebuffer) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT1("dri", "DriWrapper::AddFramebuffer",
               "handle", handle);
  return !drmModeAddFB(fd_,
                       width,
                       height,
                       depth,
                       bpp,
                       stride,
                       handle,
                       framebuffer);
}

bool DriWrapper::RemoveFramebuffer(uint32_t framebuffer) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT1("dri", "DriWrapper::RemoveFramebuffer",
               "framebuffer", framebuffer);
  return !drmModeRmFB(fd_, framebuffer);
}

bool DriWrapper::PageFlip(uint32_t crtc_id,
                          uint32_t framebuffer,
                          const PageFlipCallback& callback) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT2("dri", "DriWrapper::PageFlip",
               "crtc", crtc_id,
               "framebuffer", framebuffer);

  // NOTE: Calling drmModeSetCrtc will immediately update the state, though
  // callbacks to already scheduled page flips will be honored by the kernel.
  scoped_ptr<PageFlipPayload> payload(
      new PageFlipPayload(base::ThreadTaskRunnerHandle::Get(), callback));
  if (!drmModePageFlip(fd_, crtc_id, framebuffer, DRM_MODE_PAGE_FLIP_EVENT,
                       payload.get())) {
    // If successful the payload will be removed by a PageFlip event.
    ignore_result(payload.release());
    if (use_sync_flips_) {
      TRACE_EVENT1("dri", "OnDrmEvent", "socket", fd_);

      drmEventContext event;
      event.version = DRM_EVENT_CONTEXT_VERSION;
      event.page_flip_handler = HandlePageFlipEventOnUI;
      event.vblank_handler = nullptr;

      drmHandleEvent(fd_, &event);
    } else {
      InitializeIOWatcher();
    }

    return true;
  }

  return false;
}

bool DriWrapper::PageFlipOverlay(uint32_t crtc_id,
                                 uint32_t framebuffer,
                                 const gfx::Rect& location,
                                 const gfx::Rect& source,
                                 int overlay_plane) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT2("dri", "DriWrapper::PageFlipOverlay",
               "crtc", crtc_id,
               "framebuffer", framebuffer);
  return !drmModeSetPlane(fd_, overlay_plane, crtc_id, framebuffer, 0,
                          location.x(), location.y(), location.width(),
                          location.height(), source.x(), source.y(),
                          source.width(), source.height());
}

ScopedDrmFramebufferPtr DriWrapper::GetFramebuffer(uint32_t framebuffer) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT1("dri", "DriWrapper::GetFramebuffer",
               "framebuffer", framebuffer);
  return ScopedDrmFramebufferPtr(drmModeGetFB(fd_, framebuffer));
}

ScopedDrmPropertyPtr DriWrapper::GetProperty(drmModeConnector* connector,
                                             const char* name) {
  TRACE_EVENT2("dri", "DriWrapper::GetProperty",
               "connector", connector->connector_id,
               "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(drmModeGetProperty(fd_, connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0)
      return property.Pass();
  }

  return ScopedDrmPropertyPtr();
}

bool DriWrapper::SetProperty(uint32_t connector_id,
                             uint32_t property_id,
                             uint64_t value) {
  DCHECK(fd_ >= 0);
  return !drmModeConnectorSetProperty(fd_, connector_id, property_id, value);
}

bool DriWrapper::GetCapability(uint64_t capability, uint64_t* value) {
  DCHECK(fd_ >= 0);
  return !drmGetCap(fd_, capability, value);
}

ScopedDrmPropertyBlobPtr DriWrapper::GetPropertyBlob(
    drmModeConnector* connector, const char* name) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT2("dri", "DriWrapper::GetPropertyBlob",
               "connector", connector->connector_id,
               "name", name);
  for (int i = 0; i < connector->count_props; ++i) {
    ScopedDrmPropertyPtr property(drmModeGetProperty(fd_, connector->props[i]));
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0 &&
        property->flags & DRM_MODE_PROP_BLOB)
      return ScopedDrmPropertyBlobPtr(
          drmModeGetPropertyBlob(fd_, connector->prop_values[i]));
  }

  return ScopedDrmPropertyBlobPtr();
}

bool DriWrapper::SetCursor(uint32_t crtc_id,
                           uint32_t handle,
                           const gfx::Size& size) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT1("dri", "DriWrapper::SetCursor", "handle", handle);
  return !drmModeSetCursor(fd_, crtc_id, handle, size.width(), size.height());
}

bool DriWrapper::MoveCursor(uint32_t crtc_id, const gfx::Point& point) {
  DCHECK(fd_ >= 0);
  return !drmModeMoveCursor(fd_, crtc_id, point.x(), point.y());
}

bool DriWrapper::CreateDumbBuffer(const SkImageInfo& info,
                                  uint32_t* handle,
                                  uint32_t* stride,
                                  void** pixels) {
  DCHECK(fd_ >= 0);

  TRACE_EVENT0("dri", "DriWrapper::CreateDumbBuffer");
  if (!DrmCreateDumbBuffer(fd_, info, handle, stride))
    return false;

  if (!MapDumbBuffer(fd_, *handle, info.getSafeSize(*stride), pixels)) {
    DrmDestroyDumbBuffer(fd_, *handle);
    return false;
  }

  return true;
}

void DriWrapper::DestroyDumbBuffer(const SkImageInfo& info,
                                   uint32_t handle,
                                   uint32_t stride,
                                   void* pixels) {
  DCHECK(fd_ >= 0);
  TRACE_EVENT1("dri", "DriWrapper::DestroyDumbBuffer", "handle", handle);
  munmap(pixels, info.getSafeSize(stride));
  DrmDestroyDumbBuffer(fd_, handle);
}

bool DriWrapper::SetMaster() {
  DCHECK(fd_ >= 0);
  return (drmSetMaster(fd_) == 0);
}

bool DriWrapper::DropMaster() {
  DCHECK(fd_ >= 0);
  return (drmDropMaster(fd_) == 0);
}

}  // namespace ui
