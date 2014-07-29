// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/chromeos/x11/native_display_delegate_x11.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XInput2.h>

#include <utility>

#include "base/logging.h"
#include "base/stl_util.h"
#include "ui/display/chromeos/x11/display_mode_x11.h"
#include "ui/display/chromeos/x11/display_snapshot_x11.h"
#include "ui/display/chromeos/x11/display_util_x11.h"
#include "ui/display/chromeos/x11/native_display_event_dispatcher_x11.h"
#include "ui/display/types/chromeos/native_display_observer.h"
#include "ui/display/util/x11/edid_parser_x11.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"

namespace ui {

namespace {

// DPI measurements.
const float kMmInInch = 25.4;
const float kDpi96 = 96.0;
const float kPixelsToMmScale = kMmInInch / kDpi96;

const char kContentProtectionAtomName[] = "Content Protection";
const char kProtectionUndesiredAtomName[] = "Undesired";
const char kProtectionDesiredAtomName[] = "Desired";
const char kProtectionEnabledAtomName[] = "Enabled";

RRMode GetOutputNativeMode(const XRROutputInfo* output_info) {
  return output_info->nmode > 0 ? output_info->modes[0] : None;
}

XRRCrtcGamma* ResampleGammaRamp(XRRCrtcGamma* gamma_ramp, int gamma_ramp_size) {
  if (gamma_ramp->size == gamma_ramp_size)
    return gamma_ramp;

#define RESAMPLE(array, i, r) \
  array[i] + (array[i + 1] - array[i]) * r / gamma_ramp_size

  XRRCrtcGamma* resampled = XRRAllocGamma(gamma_ramp_size);
  for (int i = 0; i < gamma_ramp_size; ++i) {
    int base_index = gamma_ramp->size * i / gamma_ramp_size;
    int remaining = gamma_ramp->size * i % gamma_ramp_size;
    if (base_index < gamma_ramp->size - 1) {
      resampled->red[i] = RESAMPLE(gamma_ramp->red, base_index, remaining);
      resampled->green[i] = RESAMPLE(gamma_ramp->green, base_index, remaining);
      resampled->blue[i] = RESAMPLE(gamma_ramp->blue, base_index, remaining);
    } else {
      resampled->red[i] = gamma_ramp->red[gamma_ramp->size - 1];
      resampled->green[i] = gamma_ramp->green[gamma_ramp->size - 1];
      resampled->blue[i] = gamma_ramp->blue[gamma_ramp->size - 1];
    }
  }

#undef RESAMPLE
  XRRFreeGamma(gamma_ramp);
  return resampled;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeDisplayDelegateX11::HelperDelegateX11

class NativeDisplayDelegateX11::HelperDelegateX11
    : public NativeDisplayDelegateX11::HelperDelegate {
 public:
  HelperDelegateX11(NativeDisplayDelegateX11* delegate) : delegate_(delegate) {}
  virtual ~HelperDelegateX11() {}

  // NativeDisplayDelegateX11::HelperDelegate overrides:
  virtual void UpdateXRandRConfiguration(const base::NativeEvent& event)
      OVERRIDE {
    XRRUpdateConfiguration(event);
  }
  virtual const std::vector<DisplaySnapshot*>& GetCachedDisplays() const
      OVERRIDE {
    return delegate_->cached_outputs_.get();
  }
  virtual void NotifyDisplayObservers() OVERRIDE {
    FOR_EACH_OBSERVER(
        NativeDisplayObserver, delegate_->observers_, OnConfigurationChanged());
  }

 private:
  NativeDisplayDelegateX11* delegate_;

  DISALLOW_COPY_AND_ASSIGN(HelperDelegateX11);
};

////////////////////////////////////////////////////////////////////////////////
// NativeDisplayDelegateX11::PlatformEventObserverX11

class NativeDisplayDelegateX11::PlatformEventObserverX11
    : public PlatformEventObserver {
 public:
  PlatformEventObserverX11(HelperDelegate* delegate);
  virtual ~PlatformEventObserverX11();

  // PlatformEventObserverX11:
  virtual void WillProcessEvent(const ui::PlatformEvent& event) OVERRIDE;
  virtual void DidProcessEvent(const ui::PlatformEvent& event) OVERRIDE;

 private:
  HelperDelegate* delegate_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(PlatformEventObserverX11);
};

NativeDisplayDelegateX11::PlatformEventObserverX11::PlatformEventObserverX11(
    HelperDelegate* delegate)
    : delegate_(delegate) {}

NativeDisplayDelegateX11::PlatformEventObserverX11::
    ~PlatformEventObserverX11() {}

void NativeDisplayDelegateX11::PlatformEventObserverX11::WillProcessEvent(
    const ui::PlatformEvent& event) {
  // XI_HierarchyChanged events are special. There is no window associated with
  // these events. So process them directly from here.
  if (event->type == GenericEvent &&
      event->xgeneric.evtype == XI_HierarchyChanged) {
    VLOG(1) << "Received XI_HierarchyChanged event";
    // Defer configuring outputs to not stall event processing.
    // This also takes care of same event being received twice.
    delegate_->NotifyDisplayObservers();
  }
}

void NativeDisplayDelegateX11::PlatformEventObserverX11::DidProcessEvent(
    const ui::PlatformEvent& event) {}

////////////////////////////////////////////////////////////////////////////////
// NativeDisplayDelegateX11 implementation:

NativeDisplayDelegateX11::NativeDisplayDelegateX11()
    : display_(gfx::GetXDisplay()),
      window_(DefaultRootWindow(display_)),
      screen_(NULL),
      background_color_argb_(0) {}

NativeDisplayDelegateX11::~NativeDisplayDelegateX11() {
  if (ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(
        platform_event_dispatcher_.get());
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(
        platform_event_observer_.get());
  }

  STLDeleteContainerPairSecondPointers(modes_.begin(), modes_.end());
}

void NativeDisplayDelegateX11::Initialize() {
  int error_base_ignored = 0;
  int xrandr_event_base = 0;
  XRRQueryExtension(display_, &xrandr_event_base, &error_base_ignored);

  helper_delegate_.reset(new HelperDelegateX11(this));
  platform_event_dispatcher_.reset(new NativeDisplayEventDispatcherX11(
      helper_delegate_.get(), xrandr_event_base));
  platform_event_observer_.reset(
      new PlatformEventObserverX11(helper_delegate_.get()));

  if (ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(
        platform_event_dispatcher_.get());

    // We can't do this with a root window listener because XI_HierarchyChanged
    // messages don't have a target window.
    ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(
        platform_event_observer_.get());
  }
}

void NativeDisplayDelegateX11::GrabServer() {
  CHECK(!screen_) << "Server already grabbed";
  XGrabServer(display_);
  screen_ = XRRGetScreenResources(display_, window_);
  CHECK(screen_);
}

void NativeDisplayDelegateX11::UngrabServer() {
  CHECK(screen_) << "Server not grabbed";
  XRRFreeScreenResources(screen_);
  screen_ = NULL;
  XUngrabServer(display_);
  // crbug.com/366125
  XFlush(display_);
}

void NativeDisplayDelegateX11::SyncWithServer() { XSync(display_, 0); }

void NativeDisplayDelegateX11::SetBackgroundColor(uint32_t color_argb) {
  background_color_argb_ = color_argb;
}

void NativeDisplayDelegateX11::ForceDPMSOn() {
  CHECK(DPMSEnable(display_));
  CHECK(DPMSForceLevel(display_, DPMSModeOn));
}

std::vector<DisplaySnapshot*> NativeDisplayDelegateX11::GetDisplays() {
  CHECK(screen_) << "Server not grabbed";

  cached_outputs_.clear();
  RRCrtc last_used_crtc = None;

  InitModes();
  for (int i = 0; i < screen_->noutput && cached_outputs_.size() < 2; ++i) {
    RROutput output_id = screen_->outputs[i];
    XRROutputInfo* output_info = XRRGetOutputInfo(display_, screen_, output_id);
    if (output_info->connection == RR_Connected) {
      DisplaySnapshotX11* output =
          InitDisplaySnapshot(output_id, output_info, &last_used_crtc, i);
      cached_outputs_.push_back(output);
    }
    XRRFreeOutputInfo(output_info);
  }

  return cached_outputs_.get();
}

void NativeDisplayDelegateX11::AddMode(const DisplaySnapshot& output,
                                       const DisplayMode* mode) {
  CHECK(screen_) << "Server not grabbed";
  CHECK(mode) << "Must add valid mode";

  const DisplaySnapshotX11& x11_output =
      static_cast<const DisplaySnapshotX11&>(output);
  RRMode mode_id = static_cast<const DisplayModeX11*>(mode)->mode_id();

  VLOG(1) << "AddDisplayMode: output=" << x11_output.output()
          << " mode=" << mode_id;
  XRRAddOutputMode(display_, x11_output.output(), mode_id);
}

bool NativeDisplayDelegateX11::Configure(const DisplaySnapshot& output,
                                         const DisplayMode* mode,
                                         const gfx::Point& origin) {
  const DisplaySnapshotX11& x11_output =
      static_cast<const DisplaySnapshotX11&>(output);
  RRMode mode_id = None;
  if (mode)
    mode_id = static_cast<const DisplayModeX11*>(mode)->mode_id();

  return ConfigureCrtc(
      x11_output.crtc(), mode_id, x11_output.output(), origin.x(), origin.y());
}

bool NativeDisplayDelegateX11::ConfigureCrtc(RRCrtc crtc,
                                             RRMode mode,
                                             RROutput output,
                                             int x,
                                             int y) {
  CHECK(screen_) << "Server not grabbed";
  VLOG(1) << "ConfigureCrtc: crtc=" << crtc << " mode=" << mode
          << " output=" << output << " x=" << x << " y=" << y;
  // Xrandr.h is full of lies. XRRSetCrtcConfig() is defined as returning a
  // Status, which is typically 0 for failure and 1 for success. In
  // actuality it returns a RRCONFIGSTATUS, which uses 0 for success.
  if (XRRSetCrtcConfig(display_,
                       screen_,
                       crtc,
                       CurrentTime,
                       x,
                       y,
                       mode,
                       RR_Rotate_0,
                       (output && mode) ? &output : NULL,
                       (output && mode) ? 1 : 0) != RRSetConfigSuccess) {
    LOG(WARNING) << "Unable to configure CRTC " << crtc << ":"
                 << " mode=" << mode << " output=" << output << " x=" << x
                 << " y=" << y;
    return false;
  }

  return true;
}

void NativeDisplayDelegateX11::CreateFrameBuffer(const gfx::Size& size) {
  CHECK(screen_) << "Server not grabbed";
  gfx::Size current_screen_size(
      DisplayWidth(display_, DefaultScreen(display_)),
      DisplayHeight(display_, DefaultScreen(display_)));

  VLOG(1) << "CreateFrameBuffer: new=" << size.ToString()
          << " current=" << current_screen_size.ToString();

  DestroyUnusedCrtcs();

  if (size == current_screen_size)
    return;

  gfx::Size min_screen_size(current_screen_size);
  min_screen_size.SetToMin(size);
  UpdateCrtcsForNewFramebuffer(min_screen_size);

  int mm_width = size.width() * kPixelsToMmScale;
  int mm_height = size.height() * kPixelsToMmScale;
  XRRSetScreenSize(
      display_, window_, size.width(), size.height(), mm_width, mm_height);
  // We don't wait for root window resize, therefore this end up with drawing
  // in the old window size, which we care during the boot.
  DrawBackground();

  // Don't redraw the background upon framebuffer change again. This should
  // happen only once after boot.
  background_color_argb_ = 0;
}

void NativeDisplayDelegateX11::AddObserver(NativeDisplayObserver* observer) {
  observers_.AddObserver(observer);
}

void NativeDisplayDelegateX11::RemoveObserver(NativeDisplayObserver* observer) {
  observers_.RemoveObserver(observer);
}

void NativeDisplayDelegateX11::InitModes() {
  CHECK(screen_) << "Server not grabbed";

  STLDeleteContainerPairSecondPointers(modes_.begin(), modes_.end());
  modes_.clear();

  for (int i = 0; i < screen_->nmode; ++i) {
    const XRRModeInfo& info = screen_->modes[i];
    float refresh_rate = 0.0f;
    if (info.hTotal && info.vTotal) {
      refresh_rate =
          static_cast<float>(info.dotClock) /
          (static_cast<float>(info.hTotal) * static_cast<float>(info.vTotal));
    }

    modes_.insert(
        std::make_pair(info.id,
                       new DisplayModeX11(gfx::Size(info.width, info.height),
                                          info.modeFlags & RR_Interlace,
                                          refresh_rate,
                                          info.id)));
  }
}

DisplaySnapshotX11* NativeDisplayDelegateX11::InitDisplaySnapshot(
    RROutput output,
    XRROutputInfo* info,
    RRCrtc* last_used_crtc,
    int index) {
  int64_t display_id = 0;
  bool has_display_id = GetDisplayId(
      output, static_cast<uint8_t>(index), &display_id);

  bool has_overscan = false;
  GetOutputOverscanFlag(output, &has_overscan);

  DisplayConnectionType type = GetDisplayConnectionTypeFromName(info->name);
  if (type == DISPLAY_CONNECTION_TYPE_UNKNOWN)
    LOG(ERROR) << "Unknown link type: " << info->name;

  // Use the index as a valid display ID even if the internal
  // display doesn't have valid EDID because the index
  // will never change.
  if (!has_display_id) {
    if (type == DISPLAY_CONNECTION_TYPE_INTERNAL)
      has_display_id = true;

    // Fallback to output index.
    display_id = index;
  }

  RRMode native_mode_id = GetOutputNativeMode(info);
  RRMode current_mode_id = None;
  gfx::Point origin;
  if (info->crtc) {
    XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(display_, screen_, info->crtc);
    current_mode_id = crtc_info->mode;
    origin.SetPoint(crtc_info->x, crtc_info->y);
    XRRFreeCrtcInfo(crtc_info);
  }

  RRCrtc crtc = None;
  // Assign a CRTC that isn't already in use.
  for (int i = 0; i < info->ncrtc; ++i) {
    if (info->crtcs[i] != *last_used_crtc) {
      crtc = info->crtcs[i];
      *last_used_crtc = crtc;
      break;
    }
  }

  const DisplayMode* current_mode = NULL;
  const DisplayMode* native_mode = NULL;
  std::vector<const DisplayMode*> display_modes;

  for (int i = 0; i < info->nmode; ++i) {
    const RRMode mode = info->modes[i];
    if (modes_.find(mode) != modes_.end()) {
      display_modes.push_back(modes_.at(mode));

      if (mode == current_mode_id)
        current_mode = display_modes.back();
      if (mode == native_mode_id)
        native_mode = display_modes.back();
    } else {
      LOG(WARNING) << "Unable to find XRRModeInfo for mode " << mode;
    }
  }

  DisplaySnapshotX11* display_snapshot =
      new DisplaySnapshotX11(display_id,
                             has_display_id,
                             origin,
                             gfx::Size(info->mm_width, info->mm_height),
                             type,
                             IsOutputAspectPreservingScaling(output),
                             has_overscan,
                             GetDisplayName(output),
                             display_modes,
                             current_mode,
                             native_mode,
                             output,
                             crtc,
                             index);

  VLOG(2) << "Found display " << cached_outputs_.size() << ":"
          << " output=" << output << " crtc=" << crtc
          << " current_mode=" << current_mode_id;

  return display_snapshot;
}

bool NativeDisplayDelegateX11::GetHDCPState(const DisplaySnapshot& output,
                                            HDCPState* state) {
  unsigned char* values = NULL;
  int actual_format = 0;
  unsigned long nitems = 0;
  unsigned long bytes_after = 0;
  Atom actual_type = None;
  int success = 0;
  RROutput output_id = static_cast<const DisplaySnapshotX11&>(output).output();
  // TODO(kcwu): Use X11AtomCache to save round trip time of XInternAtom.
  Atom prop = XInternAtom(display_, kContentProtectionAtomName, False);

  bool ok = true;
  // TODO(kcwu): Move this to x11_util (similar method calls in this file and
  // output_util.cc)
  success = XRRGetOutputProperty(display_,
                                 output_id,
                                 prop,
                                 0,
                                 100,
                                 False,
                                 False,
                                 AnyPropertyType,
                                 &actual_type,
                                 &actual_format,
                                 &nitems,
                                 &bytes_after,
                                 &values);
  if (actual_type == None) {
    LOG(ERROR) << "Property '" << kContentProtectionAtomName
               << "' does not exist";
    ok = false;
  } else if (success == Success && actual_type == XA_ATOM &&
             actual_format == 32 && nitems == 1) {
    Atom value = reinterpret_cast<Atom*>(values)[0];
    if (value == XInternAtom(display_, kProtectionUndesiredAtomName, False)) {
      *state = HDCP_STATE_UNDESIRED;
    } else if (value ==
               XInternAtom(display_, kProtectionDesiredAtomName, False)) {
      *state = HDCP_STATE_DESIRED;
    } else if (value ==
               XInternAtom(display_, kProtectionEnabledAtomName, False)) {
      *state = HDCP_STATE_ENABLED;
    } else {
      LOG(ERROR) << "Unknown " << kContentProtectionAtomName
                 << " value: " << value;
      ok = false;
    }
  } else {
    LOG(ERROR) << "XRRGetOutputProperty failed";
    ok = false;
  }
  if (values)
    XFree(values);

  VLOG(3) << "HDCP state: " << ok << "," << *state;
  return ok;
}

bool NativeDisplayDelegateX11::SetHDCPState(const DisplaySnapshot& output,
                                            HDCPState state) {
  Atom name = XInternAtom(display_, kContentProtectionAtomName, False);
  Atom value = None;
  switch (state) {
    case HDCP_STATE_UNDESIRED:
      value = XInternAtom(display_, kProtectionUndesiredAtomName, False);
      break;
    case HDCP_STATE_DESIRED:
      value = XInternAtom(display_, kProtectionDesiredAtomName, False);
      break;
    default:
      NOTREACHED() << "Invalid HDCP state: " << state;
      return false;
  }
  gfx::X11ErrorTracker err_tracker;
  unsigned char* data = reinterpret_cast<unsigned char*>(&value);
  RROutput output_id = static_cast<const DisplaySnapshotX11&>(output).output();
  XRRChangeOutputProperty(
      display_, output_id, name, XA_ATOM, 32, PropModeReplace, data, 1);
  if (err_tracker.FoundNewError()) {
    LOG(ERROR) << "XRRChangeOutputProperty failed";
    return false;
  } else {
    return true;
  }
}

void NativeDisplayDelegateX11::DestroyUnusedCrtcs() {
  CHECK(screen_) << "Server not grabbed";

  for (int i = 0; i < screen_->ncrtc; ++i) {
    bool in_use = false;
    for (ScopedVector<DisplaySnapshot>::const_iterator it =
             cached_outputs_.begin();
         it != cached_outputs_.end();
         ++it) {
      DisplaySnapshotX11* x11_output = static_cast<DisplaySnapshotX11*>(*it);
      if (screen_->crtcs[i] == x11_output->crtc()) {
        in_use = true;
        break;
      }
    }

    if (!in_use)
      ConfigureCrtc(screen_->crtcs[i], None, None, 0, 0);
  }
}

void NativeDisplayDelegateX11::UpdateCrtcsForNewFramebuffer(
    const gfx::Size& min_screen_size) {
  CHECK(screen_) << "Server not grabbed";
  // Setting the screen size will fail if any CRTC doesn't fit afterwards.
  // At the same time, turning CRTCs off and back on uses up a lot of time.
  // This function tries to be smart to avoid too many off/on cycles:
  // - We set the new modes on CRTCs, if they fit in both the old and new
  //   FBs, and park them at (0,0)
  // - We disable the CRTCs we will need but don't fit in the old FB. Those
  //   will be reenabled after the resize.
  // We don't worry about the cached state of the outputs here since we are
  // not interested in the state we are setting - we just try to get the CRTCs
  // out of the way so we can rebuild the frame buffer.
  gfx::Rect fb_rect(min_screen_size);
  for (ScopedVector<DisplaySnapshot>::const_iterator it =
           cached_outputs_.begin();
       it != cached_outputs_.end();
       ++it) {
    DisplaySnapshotX11* x11_output = static_cast<DisplaySnapshotX11*>(*it);
    const DisplayMode* mode_info = x11_output->current_mode();
    RROutput output = x11_output->output();
    RRMode mode = None;

    if (mode_info) {
      mode = static_cast<const DisplayModeX11*>(mode_info)->mode_id();

      if (!fb_rect.Contains(gfx::Rect(mode_info->size()))) {
        // In case our CRTC doesn't fit in common area of our current and about
        // to be resized framebuffer, disable it.
        // It'll get reenabled after we resize the framebuffer.
        mode = None;
        output = None;
        mode_info = NULL;
      }
    }

    ConfigureCrtc(x11_output->crtc(), mode, output, 0, 0);
  }
}

bool NativeDisplayDelegateX11::IsOutputAspectPreservingScaling(RROutput id) {
  bool ret = false;

  Atom scaling_prop = XInternAtom(display_, "scaling mode", False);
  Atom full_aspect_atom = XInternAtom(display_, "Full aspect", False);
  if (scaling_prop == None || full_aspect_atom == None)
    return false;

  int nprop = 0;
  Atom* props = XRRListOutputProperties(display_, id, &nprop);
  for (int j = 0; j < nprop && !ret; j++) {
    Atom prop = props[j];
    if (scaling_prop == prop) {
      unsigned char* values = NULL;
      int actual_format;
      unsigned long nitems;
      unsigned long bytes_after;
      Atom actual_type;
      int success;

      success = XRRGetOutputProperty(display_,
                                     id,
                                     prop,
                                     0,
                                     100,
                                     False,
                                     False,
                                     AnyPropertyType,
                                     &actual_type,
                                     &actual_format,
                                     &nitems,
                                     &bytes_after,
                                     &values);
      if (success == Success && actual_type == XA_ATOM && actual_format == 32 &&
          nitems == 1) {
        Atom value = reinterpret_cast<Atom*>(values)[0];
        if (full_aspect_atom == value)
          ret = true;
      }
      if (values)
        XFree(values);
    }
  }
  if (props)
    XFree(props);

  return ret;
}


std::vector<ColorCalibrationProfile>
NativeDisplayDelegateX11::GetAvailableColorCalibrationProfiles(
    const DisplaySnapshot& output) {
  // TODO(mukai|marcheu): Checks the system data and fills the result.
  // Note that the order would be Dynamic -> Standard -> Movie -> Reading.
  return std::vector<ColorCalibrationProfile>();
}

bool NativeDisplayDelegateX11::SetColorCalibrationProfile(
    const DisplaySnapshot& output,
    ColorCalibrationProfile new_profile) {
  const DisplaySnapshotX11& x11_output =
      static_cast<const DisplaySnapshotX11&>(output);

  XRRCrtcGamma* gamma_ramp = CreateGammaRampForProfile(x11_output, new_profile);

  if (!gamma_ramp)
    return false;

  int gamma_ramp_size = XRRGetCrtcGammaSize(display_, x11_output.crtc());
  XRRSetCrtcGamma(display_,
                  x11_output.crtc(),
                  ResampleGammaRamp(gamma_ramp, gamma_ramp_size));
  XRRFreeGamma(gamma_ramp);
  return true;
}

XRRCrtcGamma* NativeDisplayDelegateX11::CreateGammaRampForProfile(
    const DisplaySnapshotX11& x11_output,
    ColorCalibrationProfile new_profile) {
  // TODO(mukai|marcheu): Creates the appropriate gamma ramp data from the
  // profile enum. It would be served by the vendor.
  return NULL;
}

void NativeDisplayDelegateX11::DrawBackground() {
  if (!background_color_argb_)
    return;
  // Configuring CRTCs/Framebuffer clears the boot screen image.  Paint the
  // same background color after updating framebuffer to minimize the
  // duration of black screen at boot time.
  XColor color;
  Colormap colormap = DefaultColormap(display_, 0);
  // XColor uses 16 bits per color.
  color.red = (background_color_argb_ & 0x00FF0000) >> 8;
  color.green = (background_color_argb_ & 0x0000FF00);
  color.blue = (background_color_argb_ & 0x000000FF) << 8;
  color.flags = DoRed | DoGreen | DoBlue;
  XAllocColor(display_, colormap, &color);

  GC gc = XCreateGC(display_, window_, 0, 0);
  XSetForeground(display_, gc, color.pixel);
  XSetFillStyle(display_, gc, FillSolid);
  int width = DisplayWidth(display_, DefaultScreen(display_));
  int height = DisplayHeight(display_, DefaultScreen(display_));
  XFillRectangle(display_, window_, gc, 0, 0, width, height);
  XFreeGC(display_, gc);
  XFreeColors(display_, colormap, &color.pixel, 1, 0);
}

}  // namespace ui
