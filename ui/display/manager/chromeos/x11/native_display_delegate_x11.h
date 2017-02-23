// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_MANAGER_CHROMEOS_X11_NATIVE_DISPLAY_DELEGATE_X11_H_
#define UI_DISPLAY_MANAGER_CHROMEOS_X11_NATIVE_DISPLAY_DELEGATE_X11_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/compiler_specific.h"
#include "base/event_types.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/manager/display_manager_export.h"
#include "ui/display/types/native_display_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/x/x11_types.h"

// Forward declarations for Xlib and Xrandr.
// This is so unused X definitions don't pollute the namespace.
typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;
typedef XID _Window;

struct _XRROutputInfo;
typedef _XRROutputInfo XRROutputInfo;
struct _XRRScreenResources;
typedef _XRRScreenResources XRRScreenResources;
struct _XRRCrtcGamma;
typedef _XRRCrtcGamma XRRCrtcGamma;

extern "C" {
void XRRFreeScreenResources(XRRScreenResources* resources);
}

namespace display {

class DisplayModeX11;
class DisplaySnapshotX11;
class NativeDisplayEventDispatcherX11;

class DISPLAY_MANAGER_EXPORT NativeDisplayDelegateX11
    : public NativeDisplayDelegate {
 public:
  // Helper class that allows NativeDisplayEventDispatcherX11 and
  // NativeDisplayDelegateX11::PlatformEventObserverX11 to interact with this
  // class or with mocks in tests.
  class HelperDelegate {
   public:
    virtual ~HelperDelegate() {}

    // Tells XRandR to update its configuration in response to |event|, an
    // RRScreenChangeNotify event.
    virtual void UpdateXRandRConfiguration(const base::NativeEvent& event) = 0;

    // Returns the list of current outputs. This is used to discard duplicate
    // events.
    virtual std::vector<DisplaySnapshot*> GetCachedDisplays() const = 0;

    // Notify |observers_| that a change in configuration has occurred.
    virtual void NotifyDisplayObservers() = 0;
  };

  NativeDisplayDelegateX11();
  ~NativeDisplayDelegateX11() override;

  // NativeDisplayDelegate overrides:
  void Initialize() override;
  void GrabServer() override;
  void UngrabServer() override;
  void TakeDisplayControl(const DisplayControlCallback& callback) override;
  void RelinquishDisplayControl(
      const DisplayControlCallback& callback) override;
  void SyncWithServer() override;
  void SetBackgroundColor(uint32_t color_argb) override;
  void ForceDPMSOn() override;
  void GetDisplays(const GetDisplaysCallback& callback) override;
  void AddMode(const DisplaySnapshot& output, const DisplayMode* mode) override;
  void Configure(const DisplaySnapshot& output,
                 const DisplayMode* mode,
                 const gfx::Point& origin,
                 const ConfigureCallback& callback) override;
  void CreateFrameBuffer(const gfx::Size& size) override;
  void GetHDCPState(const DisplaySnapshot& output,
                    const GetHDCPStateCallback& callback) override;
  void SetHDCPState(const DisplaySnapshot& output,
                    HDCPState state,
                    const SetHDCPStateCallback& callback) override;
  std::vector<ColorCalibrationProfile> GetAvailableColorCalibrationProfiles(
      const DisplaySnapshot& output) override;
  bool SetColorCalibrationProfile(const DisplaySnapshot& output,
                                  ColorCalibrationProfile new_profile) override;
  bool SetColorCorrection(const DisplaySnapshot& output,
                          const std::vector<GammaRampRGBEntry>& degamma_lut,
                          const std::vector<GammaRampRGBEntry>& gamma_lut,
                          const std::vector<float>& correction_matrix) override;
  void AddObserver(NativeDisplayObserver* observer) override;
  void RemoveObserver(NativeDisplayObserver* observer) override;
  FakeDisplayController* GetFakeDisplayController() override;

  std::vector<DisplaySnapshot*> GetCachedDisplays() const;

 private:
  class HelperDelegateX11;

  // Parses all the modes made available by |screen_|.
  void InitModes();

  // Helper method for GetOutputs() that returns an OutputSnapshot struct based
  // on the passed-in information.
  std::unique_ptr<DisplaySnapshotX11> InitDisplaySnapshot(
      RROutput id,
      XRROutputInfo* info,
      std::set<RRCrtc>* last_used_crtcs,
      int index);

  // Destroys unused CRTCs.
  void DestroyUnusedCrtcs();

  // Parks used CRTCs in a way which allows a framebuffer resize. This is faster
  // than turning them off, resizing, then turning them back on.
  // |min_screen_size| represent the smallest size between the current
  // framebuffer size and the requested framebuffer size.
  void UpdateCrtcsForNewFramebuffer(const gfx::Size& min_screen_size);

  bool ConfigureCrtc(RRCrtc crtc, RRMode mode, RROutput output, int x, int y);

  // Helper functions that perform the actual HDCP requests.
  bool GetHDCPState(const DisplaySnapshot& output, HDCPState* state);
  bool SetHDCPState(const DisplaySnapshot& output, HDCPState state);

  // Returns whether |id| is configured to preserve aspect when scaling.
  bool IsOutputAspectPreservingScaling(RROutput id);

  // Creates the gamma ramp for |new_profile|, or NULL if it doesn't exist.
  // The caller should take the ownership.
  XRRCrtcGamma* CreateGammaRampForProfile(const DisplaySnapshotX11& x11_output,
                                          ColorCalibrationProfile new_profile);

  void DrawBackground();

  XDisplay* display_;
  _Window window_;

  // Initialized when the server is grabbed and freed when it's ungrabbed.
  gfx::XScopedPtr<
      XRRScreenResources,
      gfx::XObjectDeleter<XRRScreenResources, void, XRRFreeScreenResources>>
      screen_;

  std::map<RRMode, std::unique_ptr<DisplayModeX11>> modes_;

  // Every time GetOutputs() is called we cache the updated list of outputs in
  // |cached_outputs_| so that we can check for duplicate events rather than
  // propagate them.
  std::vector<std::unique_ptr<DisplaySnapshot>> cached_outputs_;

  std::unique_ptr<HelperDelegate> helper_delegate_;

  // Processes X11 display events associated with the root window and notifies
  // |observers_| when a display change has occurred.
  std::unique_ptr<NativeDisplayEventDispatcherX11> platform_event_dispatcher_;

  // List of observers waiting for display configuration change events.
  base::ObserverList<NativeDisplayObserver> observers_;

  // A background color used during boot time + multi displays.
  uint32_t background_color_argb_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateX11);
};

}  // namespace display

#endif  // UI_DISPLAY_MANAGER_CHROMEOS_X11_NATIVE_DISPLAY_DELEGATE_X11_H_
