// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/compositor_switches.h"

#include "base/command_line.h"

namespace switches {

// Enable compositing individual elements via hardware overlays when
// permitted by device.
// Setting the flag to "single-fullscreen" will try to promote a single
// fullscreen overlay and use it as main framebuffer where possible.
const char kEnableHardwareOverlays[] = "enable-hardware-overlays";

// Forces tests to produce pixel output when they normally wouldn't.
const char kEnablePixelOutputInTests[] = "enable-pixel-output-in-tests";

// Limits the compositor to output a certain number of frames per second,
// maximum.
const char kLimitFps[] = "limit-fps";

const char kUIEnableRGBA4444Textures[] = "ui-enable-rgba-4444-textures";

const char kUIEnableZeroCopy[] = "ui-enable-zero-copy";

const char kUIShowPaintRects[] = "ui-show-paint-rects";

const char kUISlowAnimations[] = "ui-slow-animations";

// If enabled, all draw commands recorded on canvas are done in pixel aligned
// measurements. This also enables scaling of all elements in views and layers
// to be done via corner points. See https://goo.gl/Dqig5s
const char kEnablePixelCanvasRecording[] = "enable-pixel-canvas-recording";

}  // namespace switches

namespace ui {

bool IsUIZeroCopyEnabled() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
  return command_line.HasSwitch(switches::kUIEnableZeroCopy);
}

bool IsPixelCanvasRecordingEnabled() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnablePixelCanvasRecording);
}

}  // namespace ui
