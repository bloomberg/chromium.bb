// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_X11_NATIVE_DISPLAY_DELEGATE_X11_H_
#define UI_DISPLAY_CHROMEOS_X11_NATIVE_DISPLAY_DELEGATE_X11_H_

#include <stdint.h>

#include <map>
#include <vector>

#include "base/compiler_specific.h"
#include "base/event_types.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "ui/display/display_export.h"
#include "ui/display/types/chromeos/native_display_delegate.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"

// Forward declarations for Xlib and Xrandr.
// This is so unused X definitions don't pollute the namespace.
typedef unsigned long XID;
typedef XID RROutput;
typedef XID RRCrtc;
typedef XID RRMode;
typedef XID Window;

struct _XDisplay;
typedef struct _XDisplay Display;
struct _XRROutputInfo;
typedef _XRROutputInfo XRROutputInfo;
struct _XRRScreenResources;
typedef _XRRScreenResources XRRScreenResources;
struct _XRRCrtcGamma;
typedef _XRRCrtcGamma XRRCrtcGamma;

namespace ui {

class DisplayModeX11;
class DisplaySnapshotX11;
class NativeDisplayEventDispatcherX11;

class DISPLAY_EXPORT NativeDisplayDelegateX11 : public NativeDisplayDelegate {
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
    virtual const std::vector<DisplaySnapshot*>& GetCachedDisplays() const = 0;

    // Notify |observers_| that a change in configuration has occurred.
    virtual void NotifyDisplayObservers() = 0;
  };

  NativeDisplayDelegateX11();
  virtual ~NativeDisplayDelegateX11();

  // NativeDisplayDelegate overrides:
  virtual void Initialize() OVERRIDE;
  virtual void GrabServer() OVERRIDE;
  virtual void UngrabServer() OVERRIDE;
  virtual void SyncWithServer() OVERRIDE;
  virtual void SetBackgroundColor(uint32_t color_argb) OVERRIDE;
  virtual void ForceDPMSOn() OVERRIDE;
  virtual std::vector<DisplaySnapshot*> GetDisplays() OVERRIDE;
  virtual void AddMode(const DisplaySnapshot& output,
                       const DisplayMode* mode) OVERRIDE;
  virtual bool Configure(const DisplaySnapshot& output,
                         const DisplayMode* mode,
                         const gfx::Point& origin) OVERRIDE;
  virtual void CreateFrameBuffer(const gfx::Size& size) OVERRIDE;
  virtual bool GetHDCPState(const DisplaySnapshot& output,
                            HDCPState* state) OVERRIDE;
  virtual bool SetHDCPState(const DisplaySnapshot& output,
                            HDCPState state) OVERRIDE;
  virtual std::vector<ColorCalibrationProfile>
      GetAvailableColorCalibrationProfiles(
          const DisplaySnapshot& output) OVERRIDE;
  virtual bool SetColorCalibrationProfile(
      const DisplaySnapshot& output,
      ColorCalibrationProfile new_profile) OVERRIDE;
  virtual void AddObserver(NativeDisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(NativeDisplayObserver* observer) OVERRIDE;

 private:
  class HelperDelegateX11;
  class PlatformEventObserverX11;

  // Parses all the modes made available by |screen_|.
  void InitModes();

  // Helper method for GetOutputs() that returns an OutputSnapshot struct based
  // on the passed-in information.
  DisplaySnapshotX11* InitDisplaySnapshot(RROutput id,
                                          XRROutputInfo* info,
                                          RRCrtc* last_used_crtc,
                                          int index);

  // Destroys unused CRTCs.
  void DestroyUnusedCrtcs();

  // Parks used CRTCs in a way which allows a framebuffer resize. This is faster
  // than turning them off, resizing, then turning them back on.
  // |min_screen_size| represent the smallest size between the current
  // framebuffer size and the requested framebuffer size.
  void UpdateCrtcsForNewFramebuffer(const gfx::Size& min_screen_size);

  bool ConfigureCrtc(RRCrtc crtc, RRMode mode, RROutput output, int x, int y);

  // Returns whether |id| is configured to preserve aspect when scaling.
  bool IsOutputAspectPreservingScaling(RROutput id);

  // Creates the gamma ramp for |new_profile|, or NULL if it doesn't exist.
  // The caller should take the ownership.
  XRRCrtcGamma* CreateGammaRampForProfile(
      const DisplaySnapshotX11& x11_output,
      ColorCalibrationProfile new_profile);

  void DrawBackground();

  Display* display_;
  Window window_;

  // Initialized when the server is grabbed and freed when it's ungrabbed.
  XRRScreenResources* screen_;

  std::map<RRMode, DisplayModeX11*> modes_;

  // Every time GetOutputs() is called we cache the updated list of outputs in
  // |cached_outputs_| so that we can check for duplicate events rather than
  // propagate them.
  ScopedVector<DisplaySnapshot> cached_outputs_;

  scoped_ptr<HelperDelegate> helper_delegate_;

  // Processes X11 display events associated with the root window and notifies
  // |observers_| when a display change has occurred.
  scoped_ptr<NativeDisplayEventDispatcherX11> platform_event_dispatcher_;

  // Processes X11 display events that have no X11 window associated with it.
  scoped_ptr<PlatformEventObserverX11> platform_event_observer_;

  // List of observers waiting for display configuration change events.
  ObserverList<NativeDisplayObserver> observers_;

  // A background color used during boot time + multi displays.
  uint32_t background_color_argb_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateX11);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_X11_NATIVE_DISPLAY_DELEGATE_X11_H_
