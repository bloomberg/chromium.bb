// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_FRAME_GENERATOR_DELEGATE_H_
#define SERVICES_UI_WS_FRAME_GENERATOR_DELEGATE_H_

#include "base/macros.h"
#include "services/ui/ws/viewport_metrics.h"

namespace ui {
namespace ws {

class ServerWindow;

class FrameGeneratorDelegate {
 public:
  // Returns the root window of the display.
  virtual ServerWindow* GetRootWindow() = 0;

  // Called when a compositor frame is finished drawing.
  virtual void OnCompositorFrameDrawn() = 0;

  virtual bool IsInHighContrastMode() = 0;

  virtual const ViewportMetrics& GetViewportMetrics() = 0;

 protected:
  virtual ~FrameGeneratorDelegate() {}
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_FRAME_GENERATOR_DELEGATE_H_
