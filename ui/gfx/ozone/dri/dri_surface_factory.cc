// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/ozone/dri/dri_surface_factory.h"

#include <drm.h>
#include <errno.h>
#include <xf86drm.h>

#include "base/message_loop/message_loop.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/ozone/dri/dri_surface.h"
#include "ui/gfx/ozone/dri/dri_vsync_provider.h"
#include "ui/gfx/ozone/dri/dri_wrapper.h"
#include "ui/gfx/ozone/dri/hardware_display_controller.h"
#include "ui/gfx/ozone/surface_ozone_canvas.h"

namespace gfx {

namespace {

const char kDefaultGraphicsCardPath[] = "/dev/dri/card0";
const char kDPMSProperty[] = "DPMS";

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;

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
  static_cast<HardwareDisplayController*>(controller)
      ->OnPageFlipEvent(frame, seconds, useconds);
}

uint32_t GetDriProperty(int fd, drmModeConnector* connector, const char* name) {
  for (int i = 0; i < connector->count_props; ++i) {
    drmModePropertyPtr property = drmModeGetProperty(fd, connector->props[i]);
    if (!property)
      continue;

    if (strcmp(property->name, name) == 0) {
      uint32_t id = property->prop_id;
      drmModeFreeProperty(property);
      return id;
    }

    drmModeFreeProperty(property);
  }
  return 0;
}

uint32_t GetCrtc(int fd, drmModeRes* resources, drmModeConnector* connector) {
  // If the connector already has an encoder try to re-use.
  if (connector->encoder_id) {
    drmModeEncoder* encoder = drmModeGetEncoder(fd, connector->encoder_id);
    if (encoder) {
      if (encoder->crtc_id) {
        uint32_t crtc = encoder->crtc_id;
        drmModeFreeEncoder(encoder);
        return crtc;
      }
      drmModeFreeEncoder(encoder);
    }
  }

  // Try to find an encoder for the connector.
  for (int i = 0; i < connector->count_encoders; ++i) {
    drmModeEncoder* encoder = drmModeGetEncoder(fd, connector->encoders[i]);
    if (!encoder)
      continue;

    for (int j = 0; j < resources->count_crtcs; ++j) {
      // Check if the encoder is compatible with this CRTC
      if (!(encoder->possible_crtcs & (1 << j)))
        continue;

      drmModeFreeEncoder(encoder);
      return resources->crtcs[j];
    }
  }

  return 0;
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
class DriSurfaceAdapter : public SurfaceOzoneCanvas {
 public:
  DriSurfaceAdapter(gfx::AcceleratedWidget w, DriSurfaceFactory* dri)
      : widget_(w), dri_(dri) {}
  virtual ~DriSurfaceAdapter() {}

  // SurfaceOzone:
  virtual bool InitializeCanvas() OVERRIDE { return true; }
  virtual skia::RefPtr<SkCanvas> GetCanvas() OVERRIDE {
    return skia::SharePtr(dri_->GetCanvasForWidget(widget_));
  }
  virtual bool ResizeCanvas(const gfx::Size& viewport_size) OVERRIDE {
    NOTIMPLEMENTED();
    return false;
  }
  virtual bool PresentCanvas() OVERRIDE {
    return dri_->SchedulePageFlip(widget_);
  }
  virtual scoped_ptr<gfx::VSyncProvider> CreateVSyncProvider() OVERRIDE {
    return dri_->CreateVSyncProvider(widget_);
  }

 private:
  gfx::AcceleratedWidget widget_;
  DriSurfaceFactory* dri_;
};

}  // namespace

DriSurfaceFactory::DriSurfaceFactory()
    : drm_(),
      state_(UNINITIALIZED),
      controller_() {
}

DriSurfaceFactory::~DriSurfaceFactory() {
  if (state_ == INITIALIZED)
    ShutdownHardware();
}

SurfaceFactoryOzone::HardwareState
DriSurfaceFactory::InitializeHardware() {
  CHECK(state_ == UNINITIALIZED);

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

  controller_.reset();
  drm_.reset();

  state_ = UNINITIALIZED;
}

gfx::AcceleratedWidget DriSurfaceFactory::GetAcceleratedWidget() {
  CHECK(state_ != FAILED);

  // TODO(dnicoara) When there's more information on which display we want,
  // then we can return the widget associated with the display.
  // For now just assume we have 1 display device and return it.
  if (!controller_.get())
    controller_.reset(new HardwareDisplayController());

  // TODO(dnicoara) We only have 1 display for now, so only 1 AcceleratedWidget.
  // When we'll support multiple displays this needs to be changed to return a
  // different handle for every display.
  return kDefaultWidgetHandle;
}

scoped_ptr<SurfaceOzoneCanvas> DriSurfaceFactory::CreateCanvasForWidget(
    gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  // TODO(dnicoara) Once we can handle multiple displays this needs to be
  // changed.
  CHECK(w == kDefaultWidgetHandle);

  CHECK(controller_->get_state() ==
        HardwareDisplayController::UNASSOCIATED);

  // Until now the controller is just a stub. Initializing it will link it to a
  // hardware display.
  if (!InitializeControllerForPrimaryDisplay(drm_.get(), controller_.get())) {
    LOG(ERROR) << "Failed to initialize controller";
    return scoped_ptr<SurfaceOzoneCanvas>();
  }

  // Create a surface suitable for the current controller.
  scoped_ptr<DriSurface> surface(CreateSurface(
      Size(controller_->get_mode().hdisplay,
           controller_->get_mode().vdisplay)));

  if (!surface->Initialize()) {
    LOG(ERROR) << "Failed to initialize surface";
    return scoped_ptr<SurfaceOzoneCanvas>();
  }

  // Bind the surface to the controller. This will register the backing buffers
  // with the hardware CRTC such that we can show the buffers. The controller
  // takes ownership of the surface.
  if (!controller_->BindSurfaceToController(surface.Pass())) {
    LOG(ERROR) << "Failed to bind surface to controller";
    return scoped_ptr<SurfaceOzoneCanvas>();
  }

  // Initial cursor set.
  ResetCursor();

  return make_scoped_ptr<SurfaceOzoneCanvas>(new DriSurfaceAdapter(w, this));
}

bool DriSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

bool DriSurfaceFactory::SchedulePageFlip(gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  // TODO(dnicoara) Change this CHECK once we're running with the threaded
  // compositor.
  CHECK(base::MessageLoopForUI::IsCurrent());

  // TODO(dnicoara) Once we can handle multiple displays this needs to be
  // changed.
  CHECK(w == kDefaultWidgetHandle);

  if (!controller_->SchedulePageFlip())
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
  CHECK_EQ(kDefaultWidgetHandle, w);
  return controller_->get_surface()->GetDrawableForWidget();
}

scoped_ptr<gfx::VSyncProvider> DriSurfaceFactory::CreateVSyncProvider(
    gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  return scoped_ptr<VSyncProvider>(new DriVSyncProvider(controller_.get()));
}

void DriSurfaceFactory::SetHardwareCursor(gfx::AcceleratedWidget window,
                                          const SkBitmap& image,
                                          const gfx::Point& location) {
  cursor_bitmap_ = image;
  cursor_location_ = location;

  if (state_ != INITIALIZED)
    return;

  ResetCursor();
}

void DriSurfaceFactory::MoveHardwareCursor(gfx::AcceleratedWidget window,
                                           const gfx::Point& location) {
  cursor_location_ = location;

  if (state_ != INITIALIZED)
    return;

  controller_->MoveCursor(location);
}

void DriSurfaceFactory::UnsetHardwareCursor(gfx::AcceleratedWidget window) {
  cursor_bitmap_.reset();

  if (state_ != INITIALIZED)
    return;

  ResetCursor();
}

////////////////////////////////////////////////////////////////////////////////
// DriSurfaceFactory private

DriSurface* DriSurfaceFactory::CreateSurface(const gfx::Size& size) {
  return new DriSurface(drm_.get(), size);
}

DriWrapper* DriSurfaceFactory::CreateWrapper() {
  return new DriWrapper(kDefaultGraphicsCardPath);
}

bool DriSurfaceFactory::InitializeControllerForPrimaryDisplay(
    DriWrapper* drm,
    HardwareDisplayController* controller) {
  CHECK(state_ == SurfaceFactoryOzone::INITIALIZED);

  drmModeRes* resources = drmModeGetResources(drm->get_fd());

  // Search for an active connector.
  for (int i = 0; i < resources->count_connectors; ++i) {
    drmModeConnector* connector = drmModeGetConnector(
        drm->get_fd(),
        resources->connectors[i]);

    if (!connector)
      continue;

    if (connector->connection != DRM_MODE_CONNECTED ||
        connector->count_modes == 0) {
      drmModeFreeConnector(connector);
      continue;
    }

    uint32_t crtc = GetCrtc(drm->get_fd(), resources, connector);

    if (!crtc)
      continue;

    uint32_t dpms_property_id = GetDriProperty(drm->get_fd(),
                                               connector,
                                               kDPMSProperty);

    // TODO(dnicoara) Select one mode for now. In the future we may need to
    // save all the modes and allow the user to choose a specific mode. Or
    // even some fullscreen applications may need to change the mode.
    controller->SetControllerInfo(
        drm,
        connector->connector_id,
        crtc,
        dpms_property_id,
        connector->modes[0]);

    drmModeFreeConnector(connector);

    return true;
  }

  return false;
}

void DriSurfaceFactory::WaitForPageFlipEvent(int fd) {
  drmEventContext drm_event;
  drm_event.version = DRM_EVENT_CONTEXT_VERSION;
  drm_event.page_flip_handler = HandlePageFlipEvent;
  drm_event.vblank_handler = NULL;

  // Wait for the page-flip to complete.
  drmHandleEvent(fd, &drm_event);
}

void DriSurfaceFactory::ResetCursor() {
  if (!cursor_bitmap_.empty()) {
    // Draw new cursor into backbuffer.
    UpdateCursorImage(cursor_surface_.get(), cursor_bitmap_);

    // Reset location & buffer.
    controller_->MoveCursor(cursor_location_);
    controller_->SetCursor(cursor_surface_.get());
  } else {
    // No cursor set.
    controller_->UnsetCursor();
  }
}


}  // namespace gfx
