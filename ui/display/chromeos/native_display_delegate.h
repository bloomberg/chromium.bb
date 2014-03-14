// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_DISPLAY_CHROMEOS_NATIVE_DISPLAY_DELEGATE_H_
#define UI_DISPLAY_CHROMEOS_NATIVE_DISPLAY_DELEGATE_H_

#include "ui/display/chromeos/output_configurator.h"
#include "ui/display/display_constants.h"

namespace ui {

class NativeDisplayObserver;

// Interface for classes that perform display configuration actions on behalf
// of OutputConfigurator.
// TODO(dnicoara) This interface will eventually be platform independent,
// however it first requires refactoring the datatypes: crbug.com/333413
class NativeDisplayDelegate {
 public:
  virtual ~NativeDisplayDelegate() {}

  // Initializes the XRandR extension, saving the base event ID to |event_base|.
  virtual void Initialize() = 0;

  // Grabs the X server and refreshes XRandR-related resources.  While
  // the server is grabbed, other clients are blocked.  Must be balanced
  // by a call to UngrabServer().
  virtual void GrabServer() = 0;

  // Ungrabs the server and frees XRandR-related resources.
  virtual void UngrabServer() = 0;

  // Flushes all pending requests and waits for replies.
  virtual void SyncWithServer() = 0;

  // Sets the window's background color to |color_argb|.
  virtual void SetBackgroundColor(uint32 color_argb) = 0;

  // Enables DPMS and forces it to the "on" state.
  virtual void ForceDPMSOn() = 0;

  // Returns information about the current outputs. This method may block for
  // 60 milliseconds or more. The returned outputs are not fully initialized;
  // the rest of the work happens in
  // OutputConfigurator::UpdateCachedOutputs().
  virtual std::vector<OutputConfigurator::OutputSnapshot> GetOutputs() = 0;

  // Adds |mode| to |output|.
  virtual void AddMode(const OutputConfigurator::OutputSnapshot& output,
                       RRMode mode) = 0;

  // Calls XRRSetCrtcConfig() with the given options but some of our default
  // output count and rotation arguments. Returns true on success.
  virtual bool Configure(const OutputConfigurator::OutputSnapshot& output,
                         RRMode mode,
                         int x,
                         int y) = 0;

  // Called to set the frame buffer (underlying XRR "screen") size.  Has
  // a side-effect of disabling all CRTCs.
  virtual void CreateFrameBuffer(
      int width,
      int height,
      const std::vector<OutputConfigurator::OutputSnapshot>& outputs) = 0;

  // Gets HDCP state of output.
  virtual bool GetHDCPState(const OutputConfigurator::OutputSnapshot& output,
                            HDCPState* state) = 0;

  // Sets HDCP state of output.
  virtual bool SetHDCPState(const OutputConfigurator::OutputSnapshot& output,
                            HDCPState state) = 0;

  virtual void AddObserver(NativeDisplayObserver* observer) = 0;

  virtual void RemoveObserver(NativeDisplayObserver* observer) = 0;
};

}  //  namespace ui

#endif  // UI_DISPLAY_CHROMEOS_NATIVE_DISPLAY_DELEGATE_H_
