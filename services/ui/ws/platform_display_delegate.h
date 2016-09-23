// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_PLATFORM_DISPLAY_DELEGATE_H_
#define SERVICES_UI_WS_PLATFORM_DISPLAY_DELEGATE_H_

namespace gfx {
class Size;
}

namespace ui {

class Event;

namespace ws {

class ServerWindow;
struct ViewportMetrics;

// PlatformDisplayDelegate is implemented by an object that manages the
// lifetime of a PlatformDisplay, forwards events to the appropriate windows,
/// and responses to changes in viewport size.
class PlatformDisplayDelegate {
 public:
  // Creates the root window for this display. The new root window will have
  // |size| in DIP initially.
  virtual void CreateRootWindow(const gfx::Size& size) = 0;

  // Returns the root window of this display.
  virtual ServerWindow* GetRootWindow() = 0;

  virtual bool IsInHighContrastMode() = 0;

  // Called when an event arrives.
  virtual void OnEvent(const ui::Event& event) = 0;

  // Called when the Display loses capture.
  virtual void OnNativeCaptureLost() = 0;

  // Signals that the metrics of this display's viewport has changed.
  virtual void OnViewportMetricsChanged(const ViewportMetrics& old_metrics,
                                        const ViewportMetrics& new_metrics) = 0;

  // Called when a compositor frame is finished drawing.
  virtual void OnCompositorFrameDrawn() = 0;

 protected:
  virtual ~PlatformDisplayDelegate() {}
};

}  // namespace ws

}  // namespace ui

#endif  // SERVICES_UI_WS_PLATFORM_DISPLAY_DELEGATE_H_
