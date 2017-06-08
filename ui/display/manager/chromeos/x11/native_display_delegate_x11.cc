// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/display/manager/chromeos/x11/native_display_delegate_x11.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>

#include <algorithm>
#include <utility>

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "ui/display/manager/chromeos/x11/display_mode_x11.h"
#include "ui/display/manager/chromeos/x11/display_snapshot_x11.h"
#include "ui/display/manager/chromeos/x11/display_util_x11.h"
#include "ui/display/manager/chromeos/x11/native_display_event_dispatcher_x11.h"
#include "ui/display/types/native_display_observer.h"
#include "ui/display/util/x11/edid_parser_x11.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_error_tracker.h"
#include "ui/gfx/x/x11_types.h"

namespace display {

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
  ~HelperDelegateX11() override {}

  // NativeDisplayDelegateX11::HelperDelegate overrides:
  void UpdateXRandRConfiguration(const base::NativeEvent& event) override {
    XRRUpdateConfiguration(event);
  }
  std::vector<DisplaySnapshot*> GetCachedDisplays() const override {
    return delegate_->GetCachedDisplays();
  }
  void NotifyDisplayObservers() override {
    for (NativeDisplayObserver& observer : delegate_->observers_)
      observer.OnConfigurationChanged();
  }

 private:
  NativeDisplayDelegateX11* delegate_;

  DISALLOW_COPY_AND_ASSIGN(HelperDelegateX11);
};

////////////////////////////////////////////////////////////////////////////////
// NativeDisplayDelegateX11 implementation:

NativeDisplayDelegateX11::NativeDisplayDelegateX11()
    : display_(gfx::GetXDisplay()),
      window_(DefaultRootWindow(display_)),
      background_color_argb_(0) {}

NativeDisplayDelegateX11::~NativeDisplayDelegateX11() {
  if (ui::PlatformEventSource::GetInstance() &&
      platform_event_dispatcher_.get()) {
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(
        platform_event_dispatcher_.get());
  }
}

void NativeDisplayDelegateX11::Initialize() {
  int error_base_ignored = 0;
  int xrandr_event_base = 0;
  XRRQueryExtension(display_, &xrandr_event_base, &error_base_ignored);

  helper_delegate_.reset(new HelperDelegateX11(this));
  platform_event_dispatcher_.reset(new NativeDisplayEventDispatcherX11(
      helper_delegate_.get(), xrandr_event_base));

  if (ui::PlatformEventSource::GetInstance()) {
    ui::PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(
        platform_event_dispatcher_.get());
  }
}

void NativeDisplayDelegateX11::GrabServer() {
  CHECK(!screen_) << "Server already grabbed";
  XGrabServer(display_);
  screen_.reset(XRRGetScreenResources(display_, window_));
  CHECK(screen_);
}

void NativeDisplayDelegateX11::UngrabServer() {
  CHECK(screen_) << "Server not grabbed";
  screen_.reset();
  XUngrabServer(display_);
  // crbug.com/366125
  XFlush(display_);
}

void NativeDisplayDelegateX11::TakeDisplayControl(
    const DisplayControlCallback& callback) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateX11::RelinquishDisplayControl(
    const DisplayControlCallback& callback) {
  NOTIMPLEMENTED();
}

void NativeDisplayDelegateX11::SyncWithServer() {
  XSync(display_, 0);
}

void NativeDisplayDelegateX11::SetBackgroundColor(uint32_t color_argb) {
  background_color_argb_ = color_argb;
}

void NativeDisplayDelegateX11::ForceDPMSOn() {
  CHECK(DPMSEnable(display_));
  CHECK(DPMSForceLevel(display_, DPMSModeOn));
}

void NativeDisplayDelegateX11::GetDisplays(
    const GetDisplaysCallback& callback) {
  CHECK(screen_) << "Server not grabbed";

  cached_outputs_.clear();
  std::set<RRCrtc> last_used_crtcs;

  InitModes();
  for (int i = 0; i < screen_->noutput; ++i) {
    RROutput output_id = screen_->outputs[i];
    XRROutputInfo* output_info =
        XRRGetOutputInfo(display_, screen_.get(), output_id);
    if (output_info->connection == RR_Connected) {
      cached_outputs_.push_back(
          InitDisplaySnapshot(output_id, output_info, &last_used_crtcs, i));
    }
    XRRFreeOutputInfo(output_info);
  }

  callback.Run(GetCachedDisplays());
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

void NativeDisplayDelegateX11::Configure(const DisplaySnapshot& output,
                                         const DisplayMode* mode,
                                         const gfx::Point& origin,
                                         const ConfigureCallback& callback) {
  const DisplaySnapshotX11& x11_output =
      static_cast<const DisplaySnapshotX11&>(output);
  RRMode mode_id = None;
  if (mode)
    mode_id = static_cast<const DisplayModeX11*>(mode)->mode_id();

  callback.Run(ConfigureCrtc(x11_output.crtc(), mode_id, x11_output.output(),
                             origin.x(), origin.y()));
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
  if (XRRSetCrtcConfig(display_, screen_.get(), crtc, CurrentTime, x, y, mode,
                       RR_Rotate_0, (output && mode) ? &output : NULL,
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

FakeDisplayController* NativeDisplayDelegateX11::GetFakeDisplayController() {
  return nullptr;
}

std::vector<DisplaySnapshot*> NativeDisplayDelegateX11::GetCachedDisplays()
    const {
  std::vector<DisplaySnapshot*> snapshots(cached_outputs_.size());
  std::transform(
      cached_outputs_.cbegin(), cached_outputs_.cend(), snapshots.begin(),
      [](const std::unique_ptr<DisplaySnapshot>& item) { return item.get(); });
  return snapshots;
}

void NativeDisplayDelegateX11::InitModes() {
  CHECK(screen_) << "Server not grabbed";

  modes_.clear();

  for (int i = 0; i < screen_->nmode; ++i) {
    const XRRModeInfo& info = screen_->modes[i];
    float refresh_rate = 0.0f;
    if (info.hTotal && info.vTotal) {
      refresh_rate =
          static_cast<float>(info.dotClock) /
          (static_cast<float>(info.hTotal) * static_cast<float>(info.vTotal));
    }

    modes_.insert(std::make_pair(
        info.id, base::MakeUnique<DisplayModeX11>(
                     gfx::Size(info.width, info.height),
                     info.modeFlags & RR_Interlace, refresh_rate, info.id)));
  }
}

std::unique_ptr<DisplaySnapshotX11>
NativeDisplayDelegateX11::InitDisplaySnapshot(RROutput output,
                                              XRROutputInfo* info,
                                              std::set<RRCrtc>* last_used_crtcs,
                                              int index) {
  int64_t display_id = 0;
  EDIDParserX11 edid_parser(output);
  if (!edid_parser.GetDisplayId(static_cast<uint8_t>(index), &display_id))
    display_id = index;

  bool has_overscan = false;
  edid_parser.GetOutputOverscanFlag(&has_overscan);

  DisplayConnectionType type = GetDisplayConnectionTypeFromName(info->name);
  if (type == DISPLAY_CONNECTION_TYPE_UNKNOWN)
    LOG(ERROR) << "Unknown link type: " << info->name;

  RRMode native_mode_id = GetOutputNativeMode(info);
  RRMode current_mode_id = None;
  gfx::Point origin;
  if (info->crtc) {
    XRRCrtcInfo* crtc_info =
        XRRGetCrtcInfo(display_, screen_.get(), info->crtc);
    current_mode_id = crtc_info->mode;
    origin.SetPoint(crtc_info->x, crtc_info->y);
    XRRFreeCrtcInfo(crtc_info);
  }

  RRCrtc crtc = None;
  // Assign a CRTC that isn't already in use.
  for (int i = 0; i < info->ncrtc; ++i) {
    if (last_used_crtcs->find(info->crtcs[i]) == last_used_crtcs->end()) {
      crtc = info->crtcs[i];
      last_used_crtcs->insert(crtc);
      break;
    }
  }

  const DisplayMode* current_mode = NULL;
  const DisplayMode* native_mode = NULL;
  std::vector<std::unique_ptr<const DisplayMode>> display_modes;

  for (int i = 0; i < info->nmode; ++i) {
    const RRMode mode = info->modes[i];
    if (modes_.find(mode) != modes_.end()) {
      display_modes.push_back(modes_.at(mode)->Clone());

      if (mode == current_mode_id)
        current_mode = display_modes.back().get();
      if (mode == native_mode_id)
        native_mode = display_modes.back().get();
    } else {
      LOG(WARNING) << "Unable to find XRRModeInfo for mode " << mode;
    }
  }

  auto display_snapshot = base::MakeUnique<DisplaySnapshotX11>(
      display_id, origin, gfx::Size(info->mm_width, info->mm_height), type,
      IsOutputAspectPreservingScaling(output), has_overscan,
      edid_parser.GetDisplayName(), std::move(display_modes),
      edid_parser.edid(), current_mode, native_mode, output, crtc, index);

  VLOG(1) << "Found display " << cached_outputs_.size() << ":"
          << " output=" << output << " crtc=" << crtc
          << " current_mode=" << current_mode_id;

  return display_snapshot;
}

void NativeDisplayDelegateX11::GetHDCPState(
    const DisplaySnapshot& output,
    const GetHDCPStateCallback& callback) {
  HDCPState state = HDCP_STATE_UNDESIRED;
  bool success = GetHDCPState(output, &state);
  callback.Run(success, state);
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
  Atom prop = gfx::GetAtom(kContentProtectionAtomName);

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
  gfx::XScopedPtr<unsigned char> scoped_values(values);
  if (actual_type == None) {
    LOG(ERROR) << "Property '" << kContentProtectionAtomName
               << "' does not exist";
    return false;
  }

  if (success == Success && actual_type == XA_ATOM && actual_format == 32 &&
      nitems == 1) {
    Atom value = reinterpret_cast<Atom*>(values)[0];
    if (value == gfx::GetAtom(kProtectionUndesiredAtomName)) {
      *state = HDCP_STATE_UNDESIRED;
    } else if (value == gfx::GetAtom(kProtectionDesiredAtomName)) {
      *state = HDCP_STATE_DESIRED;
    } else if (value == gfx::GetAtom(kProtectionEnabledAtomName)) {
      *state = HDCP_STATE_ENABLED;
    } else {
      LOG(ERROR) << "Unknown " << kContentProtectionAtomName
                 << " value: " << value;
      return false;
    }
  } else {
    LOG(ERROR) << "XRRGetOutputProperty failed";
    return false;
  }

  VLOG(3) << "HDCP state: success," << *state;
  return true;
}

void NativeDisplayDelegateX11::SetHDCPState(
    const DisplaySnapshot& output,
    HDCPState state,
    const SetHDCPStateCallback& callback) {
  callback.Run(SetHDCPState(output, state));
}

bool NativeDisplayDelegateX11::SetHDCPState(const DisplaySnapshot& output,
                                            HDCPState state) {
  Atom name = gfx::GetAtom(kContentProtectionAtomName);
  Atom value = None;
  switch (state) {
    case HDCP_STATE_UNDESIRED:
      value = gfx::GetAtom(kProtectionUndesiredAtomName);
      break;
    case HDCP_STATE_DESIRED:
      value = gfx::GetAtom(kProtectionDesiredAtomName);
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
    for (const auto& snapshot : cached_outputs_) {
      DisplaySnapshotX11* x11_output =
          static_cast<DisplaySnapshotX11*>(snapshot.get());
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
  for (const auto& snapshot : cached_outputs_) {
    DisplaySnapshotX11* x11_output =
        static_cast<DisplaySnapshotX11*>(snapshot.get());
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
  Atom scaling_prop = gfx::GetAtom("scaling mode");
  Atom full_aspect_atom = gfx::GetAtom("Full aspect");
  if (scaling_prop == None || full_aspect_atom == None)
    return false;

  int nprop = 0;
  gfx::XScopedPtr<Atom[]> props(XRRListOutputProperties(display_, id, &nprop));
  for (int j = 0; j < nprop; j++) {
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
      gfx::XScopedPtr<unsigned char> scoped_value(values);
      if (success == Success && actual_type == XA_ATOM && actual_format == 32 &&
          nitems == 1) {
        Atom value = reinterpret_cast<Atom*>(values)[0];
        if (full_aspect_atom == value)
          return true;
      }
    }
  }
  return false;
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
  XRRSetCrtcGamma(display_, x11_output.crtc(),
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

bool NativeDisplayDelegateX11::SetColorCorrection(
    const DisplaySnapshot& output,
    const std::vector<GammaRampRGBEntry>& degamma_lut,
    const std::vector<GammaRampRGBEntry>& gamma_lut,
    const std::vector<float>& correction_matrix) {
  NOTIMPLEMENTED();
  return false;
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

}  // namespace display
