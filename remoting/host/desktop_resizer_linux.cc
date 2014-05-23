// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_resizer.h"
#include "remoting/host/linux/x11_util.h"

#include <string.h>
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>

#include "base/command_line.h"
#include "remoting/base/logging.h"

// On Linux, we use the xrandr extension to change the desktop resolution. For
// now, we only support resize-to-client for Xvfb-based servers that can match
// the client resolution exactly. To support best-resolution matching, it would
// be necessary to implement |GetSupportedResolutions|, but it's not considered
// a priority now.
//
// Xrandr has a number of restrictions that make this code more complex:
//
//   1. It's not possible to change the resolution of an existing mode. Instead,
//      the mode must be deleted and recreated.
//   2. It's not possible to delete a mode that's in use.
//   3. Errors are communicated via Xlib's spectacularly unhelpful mechanism
//      of terminating the process unless you install an error handler.
//
// The basic approach is as follows:
//
//   1. Create a new mode with the correct resolution;
//   2. Switch to the new mode;
//   3. Delete the old mode.
//
// Since the new mode must have a different name, and we want the current mode
// name to be consistent, we then additionally:
//
//   4. Recreate the old mode at the new resolution;
//   5. Switch to the old mode;
//   6. Delete the temporary mode.
//
// Name consistency will allow a future CL to disable resize-to-client if the
// user has changed the mode to something other than "Chrome Remote Desktop
// client resolution". It doesn't make the code significantly more complex.

namespace {

int PixelsToMillimeters(int pixels, int dpi) {
  DCHECK(dpi != 0);

  const double kMillimetersPerInch = 25.4;

  // (pixels / dpi) is the length in inches. Multiplying by
  // kMillimetersPerInch converts to mm. Multiplication is done first to
  // avoid integer division.
  return static_cast<int>(kMillimetersPerInch * pixels / dpi);
}

// TODO(jamiewalch): Use the correct DPI for the mode: http://crbug.com/172405.
const int kDefaultDPI = 96;

}  // namespace

namespace remoting {

// Wrapper class for the XRRScreenResources struct.
class ScreenResources {
 public:
  ScreenResources() : resources_(NULL) {
  }

  ~ScreenResources() {
    Release();
  }

  bool Refresh(Display* display, Window window) {
    Release();
    resources_ = XRRGetScreenResources(display, window);
    return resources_ != NULL;
  }

  void Release() {
    if (resources_) {
      XRRFreeScreenResources(resources_);
      resources_ = NULL;
    }
  }

  RRMode GetIdForMode(const char* name) {
    CHECK(resources_);
    for (int i = 0; i < resources_->nmode; ++i) {
      const XRRModeInfo& mode = resources_->modes[i];
      if (strcmp(mode.name, name) == 0) {
        return mode.id;
      }
    }
    return 0;
  }

  // For now, assume we're only ever interested in the first output.
  RROutput GetOutput() {
    CHECK(resources_);
    return resources_->outputs[0];
  }

  // For now, assume we're only ever interested in the first crtc.
  RRCrtc GetCrtc() {
    CHECK(resources_);
    return resources_->crtcs[0];
  }

  XRROutputInfo* GetOutputInfo(Display* display, RROutput output_id) {
    CHECK(resources_);
    return XRRGetOutputInfo(display, resources_, output_id);
  }

  XRRScreenResources* get() { return resources_; }

 private:
  XRRScreenResources* resources_;
};


class DesktopResizerLinux : public DesktopResizer {
 public:
  DesktopResizerLinux();
  virtual ~DesktopResizerLinux();

  // DesktopResizer interface
  virtual ScreenResolution GetCurrentResolution() OVERRIDE;
  virtual std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred) OVERRIDE;
  virtual void SetResolution(const ScreenResolution& resolution) OVERRIDE;
  virtual void RestoreResolution(const ScreenResolution& original) OVERRIDE;

 private:
  // Create a mode, and attach it to the primary output. If the mode already
  // exists, it is left unchanged.
  void CreateMode(const char* name, int width, int height);

  // Remove the specified mode from the primary output, and delete it. If the
  // mode is in use, it is not deleted.
  void DeleteMode(const char* name);

  // Switch the primary output to the specified mode. If name is NULL, the
  // primary output is disabled instead, which is required before changing
  // its resolution.
  void SwitchToMode(const char* name);

  Display* display_;
  int screen_;
  Window root_;
  ScreenResources resources_;
  bool exact_resize_;

  DISALLOW_COPY_AND_ASSIGN(DesktopResizerLinux);
};

DesktopResizerLinux::DesktopResizerLinux()
    : display_(XOpenDisplay(NULL)),
      screen_(DefaultScreen(display_)),
      root_(RootWindow(display_, screen_)),
      exact_resize_(base::CommandLine::ForCurrentProcess()->
                    HasSwitch("server-supports-exact-resize")) {
  XRRSelectInput(display_, root_, RRScreenChangeNotifyMask);
}

DesktopResizerLinux::~DesktopResizerLinux() {
  XCloseDisplay(display_);
}

ScreenResolution DesktopResizerLinux::GetCurrentResolution() {
  if (!exact_resize_) {
    // TODO(jamiewalch): Remove this early return if we decide to support
    // non-Xvfb servers.
    return ScreenResolution();
  }

  // TODO(lambroslambrou): Xrandr requires that we process RRScreenChangeNotify
  // events, otherwise DisplayWidth and DisplayHeight do not return the current
  // values. Normally, this would be done via a central X event loop, but we
  // don't have one, hence this horrible hack.
  //
  // Note that the WatchFileDescriptor approach taken in XServerClipboard
  // doesn't work here because resize events have already been read from the
  // X server socket by the time the resize function returns, hence the
  // file descriptor is never seen as readable.
  while (XEventsQueued(display_, QueuedAlready)) {
    XEvent event;
    XNextEvent(display_, &event);
    XRRUpdateConfiguration(&event);
  }

  ScreenResolution result(
      webrtc::DesktopSize(
          DisplayWidth(display_, DefaultScreen(display_)),
          DisplayHeight(display_, DefaultScreen(display_))),
      webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
  return result;
}

std::list<ScreenResolution> DesktopResizerLinux::GetSupportedResolutions(
    const ScreenResolution& preferred) {
  std::list<ScreenResolution> result;
  if (exact_resize_) {
    // Clamp the specified size to something valid for the X server.
    int min_width = 0, min_height = 0, max_width = 0, max_height = 0;
    XRRGetScreenSizeRange(display_, root_,
                          &min_width, &min_height,
                          &max_width, &max_height);
    int width = std::min(std::max(preferred.dimensions().width(), min_width),
                         max_width);
    int height = std::min(std::max(preferred.dimensions().height(), min_height),
                          max_height);
    // Additionally impose a minimum size of 640x480, since anything smaller
    // doesn't seem very useful.
    ScreenResolution actual(
        webrtc::DesktopSize(std::max(640, width), std::max(480, height)),
        webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
    result.push_back(actual);
  } else {
    // TODO(jamiewalch): Return the list of supported resolutions if we can't
    // support exact-size matching.
  }
  return result;
}

void DesktopResizerLinux::SetResolution(const ScreenResolution& resolution) {
  if (!exact_resize_) {
    // TODO(jamiewalch): Remove this early return if we decide to support
    // non-Xvfb servers.
    return;
  }

  // Ignore X errors encountered while resizing the display. We might hit an
  // error, for example if xrandr has been used to add a mode with the same
  // name as our temporary mode, or to remove the "client resolution" mode. We
  // don't want to terminate the process if this happens.
  ScopedXErrorHandler handler(ScopedXErrorHandler::Ignore());

  // Grab the X server while we're changing the display resolution. This ensures
  // that the display configuration doesn't change under our feet.
  ScopedXGrabServer grabber(display_);

  // The name of the mode representing the current client view resolution and
  // the temporary mode used for the reasons described at the top of this file.
  // The former should be localized if it's user-visible; the latter only
  // exists briefly and does not need to localized.
  const char* kModeName = "Chrome Remote Desktop client resolution";
  const char* kTempModeName = "Chrome Remote Desktop temporary mode";

  // Actually do the resize operation, preserving the current mode name. Note
  // that we have to detach the output from any mode in order to resize it
  // (strictly speaking, this is only required when reducing the size, but it
  // seems safe to do it regardless).
  HOST_LOG << "Changing desktop size to " << resolution.dimensions().width()
            << "x" << resolution.dimensions().height();

  // TODO(lambroslambrou): Use the DPI from client size information.
  int width_mm = PixelsToMillimeters(resolution.dimensions().width(),
                                     kDefaultDPI);
  int height_mm = PixelsToMillimeters(resolution.dimensions().height(),
                                      kDefaultDPI);
  CreateMode(kTempModeName, resolution.dimensions().width(),
             resolution.dimensions().height());
  SwitchToMode(NULL);
  XRRSetScreenSize(display_, root_, resolution.dimensions().width(),
                   resolution.dimensions().height(), width_mm, height_mm);
  SwitchToMode(kTempModeName);
  DeleteMode(kModeName);
  CreateMode(kModeName, resolution.dimensions().width(),
             resolution.dimensions().height());
  SwitchToMode(kModeName);
  DeleteMode(kTempModeName);
}

void DesktopResizerLinux::RestoreResolution(const ScreenResolution& original) {
  // Since the desktop is only visible via a remote connection, the original
  // resolution of the desktop will never been seen and there's no point
  // restoring it; if we did, we'd just risk messing up the user's window
  // layout.
}

void DesktopResizerLinux::CreateMode(const char* name, int width, int height) {
  XRRModeInfo mode;
  memset(&mode, 0, sizeof(mode));
  mode.width = width;
  mode.height = height;
  mode.name = const_cast<char*>(name);
  mode.nameLength = strlen(name);
  XRRCreateMode(display_, root_, &mode);

  if (!resources_.Refresh(display_, root_)) {
    return;
  }
  RRMode mode_id = resources_.GetIdForMode(name);
  if (!mode_id) {
    return;
  }
  XRRAddOutputMode(display_, resources_.GetOutput(), mode_id);
}

void DesktopResizerLinux::DeleteMode(const char* name) {
  RRMode mode_id = resources_.GetIdForMode(name);
  if (mode_id) {
    XRRDeleteOutputMode(display_, resources_.GetOutput(), mode_id);
    XRRDestroyMode(display_, mode_id);
    resources_.Refresh(display_, root_);
  }
}

void DesktopResizerLinux::SwitchToMode(const char* name) {
  RRMode mode_id = None;
  RROutput* outputs = NULL;
  int number_of_outputs = 0;
  if (name) {
    mode_id = resources_.GetIdForMode(name);
    CHECK(mode_id);
    outputs = resources_.get()->outputs;
    number_of_outputs = resources_.get()->noutput;
  }
  XRRSetCrtcConfig(display_, resources_.get(), resources_.GetCrtc(),
                   CurrentTime, 0, 0, mode_id, 1, outputs, number_of_outputs);
}

scoped_ptr<DesktopResizer> DesktopResizer::Create() {
  return scoped_ptr<DesktopResizer>(new DesktopResizerLinux);
}

}  // namespace remoting
