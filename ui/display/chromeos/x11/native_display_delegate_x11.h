// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_X11_NATIVE_DISPLAY_DELEGATE_X11_H_
#define UI_DISPLAY_CHROMEOS_X11_NATIVE_DISPLAY_DELEGATE_X11_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/display/chromeos/native_display_delegate.h"

typedef XID Window;

struct _XDisplay;
typedef struct _XDisplay Display;
struct _XRROutputInfo;
typedef _XRROutputInfo XRROutputInfo;
struct _XRRScreenResources;
typedef _XRRScreenResources XRRScreenResources;

namespace ui {

class NativeDisplayEventDispatcherX11;

class NativeDisplayDelegateX11 : public NativeDisplayDelegate {
 public:
  // Helper class that allows NativeDisplayEventDispatcherX11 and
  // NativeDisplayDelegateX11::MessagePumpObserverX11 to interact with this
  // class or with mocks in tests.
  class HelperDelegate {
   public:
    virtual ~HelperDelegate() {}

    // Tells XRandR to update its configuration in response to |event|, an
    // RRScreenChangeNotify event.
    virtual void UpdateXRandRConfiguration(const base::NativeEvent& event) = 0;

    // Returns the list of current outputs. This is used to discard duplicate
    // events.
    virtual const std::vector<OutputConfigurator::OutputSnapshot>&
        GetCachedOutputs() const = 0;

    // Notify |observers_| that a change in configuration has occurred.
    virtual void NotifyDisplayObservers() = 0;
  };

  NativeDisplayDelegateX11();
  virtual ~NativeDisplayDelegateX11();

  // OutputConfigurator::Delegate overrides:
  virtual void Initialize() OVERRIDE;
  virtual void GrabServer() OVERRIDE;
  virtual void UngrabServer() OVERRIDE;
  virtual void SyncWithServer() OVERRIDE;
  virtual void SetBackgroundColor(uint32 color_argb) OVERRIDE;
  virtual void ForceDPMSOn() OVERRIDE;
  virtual std::vector<OutputConfigurator::OutputSnapshot> GetOutputs() OVERRIDE;
  virtual void AddMode(const OutputConfigurator::OutputSnapshot& output,
                       RRMode mode) OVERRIDE;
  virtual bool Configure(const OutputConfigurator::OutputSnapshot& output,
                         RRMode mode,
                         int x,
                         int y) OVERRIDE;
  virtual void CreateFrameBuffer(
      int width,
      int height,
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs) OVERRIDE;
  virtual bool GetHDCPState(const OutputConfigurator::OutputSnapshot& output,
                            ui::HDCPState* state) OVERRIDE;
  virtual bool SetHDCPState(const OutputConfigurator::OutputSnapshot& output,
                            ui::HDCPState state) OVERRIDE;

  virtual void AddObserver(NativeDisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(NativeDisplayObserver* observer) OVERRIDE;

 private:
  class HelperDelegateX11;
  class MessagePumpObserverX11;

  // Initializes |mode_info| to contain details corresponding to |mode|. Returns
  // true on success.
  bool InitModeInfo(RRMode mode, OutputConfigurator::ModeInfo* mode_info);

  // Helper method for GetOutputs() that returns an OutputSnapshot struct based
  // on the passed-in information. Further initialization is required (e.g.
  // |selected_mode|, |mirror_mode|, and |touch_device_id|).
  OutputConfigurator::OutputSnapshot InitOutputSnapshot(RROutput id,
                                                        XRROutputInfo* info,
                                                        RRCrtc* last_used_crtc,
                                                        int index);

  // Destroys unused CRTCs and parks used CRTCs in a way which allows a
  // framebuffer resize. This is faster than turning them off, resizing,
  // then turning them back on.
  void DestroyUnusedCrtcs(
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs);

  bool ConfigureCrtc(RRCrtc crtc, RRMode mode, RROutput output, int x, int y);

  // Returns whether |id| is configured to preserve aspect when scaling.
  bool IsOutputAspectPreservingScaling(RROutput id);

  Display* display_;
  Window window_;

  // Initialized when the server is grabbed and freed when it's ungrabbed.
  XRRScreenResources* screen_;

  // Every time GetOutputs() is called we cache the updated list of outputs in
  // |cached_outputs_| so that we can check for duplicate events rather than
  // propagate them.
  std::vector<OutputConfigurator::OutputSnapshot> cached_outputs_;

  scoped_ptr<HelperDelegate> helper_delegate_;

  // Processes X11 display events associated with the root window and notifies
  // |observers_| when a display change has occurred.
  scoped_ptr<NativeDisplayEventDispatcherX11> message_pump_dispatcher_;

  // Processes X11 display events that have no X11 window associated with it.
  scoped_ptr<MessagePumpObserverX11> message_pump_observer_;

  // List of observers waiting for display configuration change events.
  ObserverList<NativeDisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(NativeDisplayDelegateX11);
};

}  // namespace ui

#endif  // UI_DISPLAY_CHROMEOS_X11_NATIVE_DISPLAY_DELEGATE_X11_H_
