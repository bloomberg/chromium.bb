// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_surface_factory.h"

#include <drm.h>
#include <errno.h>
#include <xf86drm.h>

#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/ozone/surface_ozone_canvas.h"
#include "ui/ozone/platform/dri/dri_surface.h"
#include "ui/ozone/platform/dri/dri_util.h"
#include "ui/ozone/platform/dri/dri_vsync_provider.h"
#include "ui/ozone/platform/dri/dri_wrapper.h"
#include "ui/ozone/platform/dri/hardware_display_controller.h"

namespace ui {

namespace {

const char kDefaultGraphicsCardPath[] = "/dev/dri/card0";

// TODO(dnicoara) Read the cursor plane size from the hardware.
const gfx::Size kCursorSize(64, 64);

// DRM callback on page flip events. This callback is triggered after the
// page flip has happened and the backbuffer is now the new frontbuffer
// The old frontbuffer is no longer used by the hardware and can be used for
// future draw operations.
//
// |device| will contain a reference to the |DriSurface| object which
// the event belongs to.
//
// TODO(dnicoara) When we have a FD handler for the DRM calls in the message
// loop, we can move this function in the handler.
void HandlePageFlipEvent(int fd,
                         unsigned int frame,
                         unsigned int seconds,
                         unsigned int useconds,
                         void* controller) {
  TRACE_EVENT0("dri", "HandlePageFlipEvent");
  static_cast<HardwareDisplayController*>(controller)
      ->OnPageFlipEvent(frame, seconds, useconds);
}

void UpdateCursorImage(DriSurface* cursor, const SkBitmap& image) {
  SkRect damage;
  image.getBounds(&damage);

  // Clear to transparent in case |image| is smaller than the canvas.
  SkCanvas* canvas = cursor->GetDrawableForWidget();
  canvas->clear(SK_ColorTRANSPARENT);

  SkRect clip;
  clip.set(
      0, 0, canvas->getDeviceSize().width(), canvas->getDeviceSize().height());
  canvas->clipRect(clip, SkRegion::kReplace_Op);
  canvas->drawBitmapRectToRect(image, &damage, damage);
}

// Adapter from SurfaceOzone to DriSurfaceFactory
//
// This class is derived from SurfaceOzone and owned by the compositor.
//
// For DRI the hadware surface & canvas are owned by the platform, so
// the compositor merely owns this proxy object.
//
// TODO(spang): Should the compositor own any bits of the DriSurface?
class DriSurfaceAdapter : public gfx::SurfaceOzoneCanvas {
 public:
  DriSurfaceAdapter(gfx::AcceleratedWidget w, DriSurfaceFactory* dri)
      : widget_(w), dri_(dri) {}
  virtual ~DriSurfaceAdapter() {}

  // SurfaceOzoneCanvas:
  virtual skia::RefPtr<SkCanvas> GetCanvas() OVERRIDE;
  virtual void ResizeCanvas(const gfx::Size& viewport_size) OVERRIDE;
  virtual void PresentCanvas(const gfx::Rect& damage) OVERRIDE;
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE;

 private:
  skia::RefPtr<SkSurface> surface_;
  gfx::Rect last_damage_;

  gfx::AcceleratedWidget widget_;
  DriSurfaceFactory* dri_;
};

skia::RefPtr<SkCanvas> DriSurfaceAdapter::GetCanvas() {
  return skia::SharePtr(surface_->getCanvas());
}

void DriSurfaceAdapter::ResizeCanvas(const gfx::Size& viewport_size) {
  SkImageInfo info = SkImageInfo::MakeN32(
      viewport_size.width(), viewport_size.height(), kOpaque_SkAlphaType);
  surface_ = skia::AdoptRef(SkSurface::NewRaster(info));
}

void DriSurfaceAdapter::PresentCanvas(const gfx::Rect& damage) {
  SkCanvas* canvas = dri_->GetCanvasForWidget(widget_);

  // The DriSurface is double buffered, so the current back buffer is
  // missing the previous update. Expand damage region.
  SkRect real_damage = RectToSkRect(UnionRects(damage, last_damage_));

  // Copy damage region.
  skia::RefPtr<SkImage> image = skia::AdoptRef(surface_->newImageSnapshot());
  image->draw(canvas, &real_damage, real_damage, NULL);

  last_damage_ = damage;
  dri_->SchedulePageFlip(widget_);
}

scoped_ptr<gfx::VSyncProvider> DriSurfaceAdapter::CreateVSyncProvider() {
  return dri_->CreateVSyncProvider(widget_);
}

}  // namespace

// static
const gfx::AcceleratedWidget DriSurfaceFactory::kDefaultWidgetHandle = 1;

DriSurfaceFactory::DriSurfaceFactory()
    : drm_(),
      state_(UNINITIALIZED),
      controllers_(),
      allocated_widgets_(0) {
}

DriSurfaceFactory::~DriSurfaceFactory() {
  if (state_ == INITIALIZED)
    ShutdownHardware();
}

gfx::SurfaceFactoryOzone::HardwareState
DriSurfaceFactory::InitializeHardware() {
  if (state_ != UNINITIALIZED)
    return state_;

  // TODO(dnicoara): Short-cut right now. What we want is to look at all the
  // graphics devices available and select the primary one.
  drm_.reset(CreateWrapper());
  if (drm_->get_fd() < 0) {
    LOG(ERROR) << "Cannot open graphics card '"
               << kDefaultGraphicsCardPath << "': " << strerror(errno);
    state_ = FAILED;
    return state_;
  }

  cursor_surface_.reset(CreateSurface(kCursorSize));
  if (!cursor_surface_->Initialize()) {
    LOG(ERROR) << "Failed to initialize cursor surface";
    state_ = FAILED;
    return state_;
  }

  state_ = INITIALIZED;
  return state_;
}

void DriSurfaceFactory::ShutdownHardware() {
  CHECK(state_ == INITIALIZED);

  controllers_.clear();
  drm_.reset();

  state_ = UNINITIALIZED;
}

gfx::AcceleratedWidget DriSurfaceFactory::GetAcceleratedWidget() {
  CHECK(state_ != FAILED);

  // We're not using 0 since other code assumes that a 0 AcceleratedWidget is an
  // invalid widget.
  return ++allocated_widgets_;
}

scoped_ptr<gfx::SurfaceOzoneCanvas> DriSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  // When running with content_shell, a default Display gets created. But we
  // can't just create a surface without a backing native display. This forces
  // initialization of a display.
  if (controllers_.size() == 0 && !InitializePrimaryDisplay()) {
      LOG(ERROR) << "Failed forced initialization of primary display";
      return scoped_ptr<gfx::SurfaceOzoneCanvas>();
  }

  // Initial cursor set.
  ResetCursor(w);

  return scoped_ptr<gfx::SurfaceOzoneCanvas>(new DriSurfaceAdapter(w, this));
}

bool DriSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

bool DriSurfaceFactory::SchedulePageFlip(gfx::AcceleratedWidget w) {
  TRACE_EVENT0("dri", "DriSurfaceFactory::SchedulePageFlip");

  CHECK(state_ == INITIALIZED);
  // TODO(dnicoara) Change this CHECK once we're running with the threaded
  // compositor.
  CHECK(base::MessageLoopForUI::IsCurrent());

  if (!GetControllerForWidget(w)->SchedulePageFlip())
    return false;

  // Only wait for the page flip event to finish if it was properly scheduled.
  //
  // TODO(dnicoara) The following call will wait for the page flip event to
  // complete. This means that it will block until the next VSync. Ideally the
  // wait should happen in the message loop. The message loop would then
  // schedule the next draw event. Alternatively, the VSyncProvider could be
  // used to schedule the next draw. Unfortunately, at this point,
  // DriOutputDevice does not provide any means to use any of the above
  // solutions. Note that if the DRM callback does not schedule the next draw,
  // then some sort of synchronization needs to take place since starting a new
  // draw before the page flip happened is considered an error. However we can
  // not use any lock constructs unless we're using the threaded compositor.
  // Note that the following call does not use any locks, so it is safe to be
  // made on the UI thread (thought not ideal).
  WaitForPageFlipEvent(drm_->get_fd());

  return true;
}

SkCanvas* DriSurfaceFactory::GetCanvasForWidget(
    gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  return GetControllerForWidget(w)->get_surface()->GetDrawableForWidget();
}

scoped_ptr<gfx::VSyncProvider> DriSurfaceFactory::CreateVSyncProvider(
    gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  return scoped_ptr<gfx::VSyncProvider>(new DriVSyncProvider(
      GetControllerForWidget(w)));
}

bool DriSurfaceFactory::CreateHardwareDisplayController(
    uint32_t connector, uint32_t crtc, const drmModeModeInfo& mode) {
  scoped_ptr<HardwareDisplayController> controller(
      new HardwareDisplayController(drm_.get(), connector, crtc, mode));

  // Create a surface suitable for the current controller.
  scoped_ptr<DriSurface> surface(CreateSurface(
      gfx::Size(mode.hdisplay, mode.vdisplay)));

  if (!surface->Initialize()) {
    LOG(ERROR) << "Failed to initialize surface";
    return false;
  }

  // Bind the surface to the controller. This will register the backing buffers
  // with the hardware CRTC such that we can show the buffers and performs the
  // initial modeset. The controller takes ownership of the surface.
  if (!controller->BindSurfaceToController(surface.Pass())) {
    LOG(ERROR) << "Failed to bind surface to controller";
    return false;
  }

  controllers_.push_back(controller.release());
  return true;
}

bool DriSurfaceFactory::DisableHardwareDisplayController(uint32_t crtc) {
  return drm_->DisableCrtc(crtc);
}

void DriSurfaceFactory::SetHardwareCursor(gfx::AcceleratedWidget window,
                                          const SkBitmap& image,
                                          const gfx::Point& location) {
  cursor_bitmap_ = image;
  cursor_location_ = location;

  if (state_ != INITIALIZED)
    return;

  ResetCursor(window);
}

void DriSurfaceFactory::MoveHardwareCursor(gfx::AcceleratedWidget window,
                                           const gfx::Point& location) {
  cursor_location_ = location;

  if (state_ != INITIALIZED)
    return;

  GetControllerForWidget(window)->MoveCursor(location);
}

void DriSurfaceFactory::UnsetHardwareCursor(gfx::AcceleratedWidget window) {
  cursor_bitmap_.reset();

  if (state_ != INITIALIZED)
    return;

  ResetCursor(window);
}

////////////////////////////////////////////////////////////////////////////////
// DriSurfaceFactory private

DriSurface* DriSurfaceFactory::CreateSurface(const gfx::Size& size) {
  return new DriSurface(drm_.get(), size);
}

DriWrapper* DriSurfaceFactory::CreateWrapper() {
  return new DriWrapper(kDefaultGraphicsCardPath);
}

bool DriSurfaceFactory::InitializePrimaryDisplay() {
  drmModeRes* resources = drmModeGetResources(drm_->get_fd());
  DCHECK(resources) << "Failed to get DRM resources";
  ScopedVector<HardwareDisplayControllerInfo> displays =
      GetAvailableDisplayControllerInfos(drm_->get_fd(), resources);
  drmModeFreeResources(resources);

  if (displays.size() == 0)
    return false;

  drmModePropertyRes* dpms = drm_->GetProperty(displays[0]->connector(),
                                               "DPMS");
  if (dpms)
    drm_->SetProperty(displays[0]->connector()->connector_id,
                      dpms->prop_id,
                      DRM_MODE_DPMS_ON);

  CreateHardwareDisplayController(
      displays[0]->connector()->connector_id,
      displays[0]->crtc()->crtc_id,
      displays[0]->connector()->modes[0]);
  return true;
}

void DriSurfaceFactory::WaitForPageFlipEvent(int fd) {
  TRACE_EVENT0("dri", "WaitForPageFlipEvent");

  drmEventContext drm_event;
  drm_event.version = DRM_EVENT_CONTEXT_VERSION;
  drm_event.page_flip_handler = HandlePageFlipEvent;
  drm_event.vblank_handler = NULL;

  // Wait for the page-flip to complete.
  drmHandleEvent(fd, &drm_event);
}

void DriSurfaceFactory::ResetCursor(gfx::AcceleratedWidget w) {
  if (!cursor_bitmap_.empty()) {
    // Draw new cursor into backbuffer.
    UpdateCursorImage(cursor_surface_.get(), cursor_bitmap_);

    // Reset location & buffer.
    GetControllerForWidget(w)->MoveCursor(cursor_location_);
    GetControllerForWidget(w)->SetCursor(cursor_surface_.get());
  } else {
    // No cursor set.
    GetControllerForWidget(w)->UnsetCursor();
  }
}

HardwareDisplayController* DriSurfaceFactory::GetControllerForWidget(
    gfx::AcceleratedWidget w) {
  CHECK_GE(w, 1);
  CHECK_LE(static_cast<size_t>(w), controllers_.size());

  return controllers_[w - 1];
}

}  // namespace ui
