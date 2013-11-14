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
#include "ui/gfx/ozone/dri/dri_skbitmap.h"
#include "ui/gfx/ozone/dri/dri_surface.h"
#include "ui/gfx/ozone/dri/dri_vsync_provider.h"
#include "ui/gfx/ozone/dri/dri_wrapper.h"
#include "ui/gfx/ozone/dri/hardware_display_controller.h"

namespace gfx {

namespace {

const char kDefaultGraphicsCardPath[] = "/dev/dri/card0";
const char kDPMSProperty[] = "DPMS";

const gfx::AcceleratedWidget kDefaultWidgetHandle = 1;

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

gfx::AcceleratedWidget DriSurfaceFactory::RealizeAcceleratedWidget(
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
    return gfx::kNullAcceleratedWidget;
  }

  // Create a surface suitable for the current controller.
  scoped_ptr<DriSurface> surface(CreateSurface(controller_.get()));

  if (!surface->Initialize()) {
    LOG(ERROR) << "Failed to initialize surface";
    return gfx::kNullAcceleratedWidget;
  }

  // Bind the surface to the controller. This will register the backing buffers
  // with the hardware CRTC such that we can show the buffers. The controller
  // takes ownership of the surface.
  if (!controller_->BindSurfaceToController(surface.Pass())) {
    LOG(ERROR) << "Failed to bind surface to controller";
    return gfx::kNullAcceleratedWidget;
  }

  return reinterpret_cast<gfx::AcceleratedWidget>(controller_->get_surface());
}

bool DriSurfaceFactory::LoadEGLGLES2Bindings(
      AddGLLibraryCallback add_gl_library,
      SetGLGetProcAddressProcCallback set_gl_get_proc_address) {
  return false;
}

bool DriSurfaceFactory::AttemptToResizeAcceleratedWidget(
    gfx::AcceleratedWidget w,
    const gfx::Rect& bounds) {
  return false;
}

bool DriSurfaceFactory::SchedulePageFlip(gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  // TODO(dnicoara) Change this CHECK once we're running with the threaded
  // compositor.
  CHECK(base::MessageLoop::current()->type() == base::MessageLoop::TYPE_UI);

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
  return reinterpret_cast<DriSurface*>(w)->GetDrawableForWidget();
}

gfx::VSyncProvider* DriSurfaceFactory::GetVSyncProvider(
    gfx::AcceleratedWidget w) {
  CHECK(state_ == INITIALIZED);
  return new DriVSyncProvider(controller_.get());
}

////////////////////////////////////////////////////////////////////////////////
// DriSurfaceFactory private

DriSurface* DriSurfaceFactory::CreateSurface(
    HardwareDisplayController* controller) {
  return new DriSurface(controller);
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

}  // namespace gfx
