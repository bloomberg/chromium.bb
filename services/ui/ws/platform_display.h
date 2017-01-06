// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_PLATFORM_DISPLAY_H_
#define SERVICES_UI_WS_PLATFORM_DISPLAY_H_

#include <stdint.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "services/ui/display/viewport_metrics.h"
#include "services/ui/public/interfaces/cursor.mojom.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Rect;
}

namespace ui {

struct TextInputState;

namespace ws {

class FrameGenerator;
class PlatformDisplayDelegate;
class PlatformDisplayFactory;
struct PlatformDisplayInitParams;

// PlatformDisplay is used to connect the root ServerWindow to a display.
class PlatformDisplay {
 public:
  virtual ~PlatformDisplay() {}

  static std::unique_ptr<PlatformDisplay> Create(
      const PlatformDisplayInitParams& init_params);

  virtual int64_t GetId() const = 0;

  virtual void Init(PlatformDisplayDelegate* delegate) = 0;

  virtual void SetViewportSize(const gfx::Size& size) = 0;

  virtual void SetTitle(const base::string16& title) = 0;

  virtual void SetCapture() = 0;

  virtual void ReleaseCapture() = 0;

  virtual void SetCursorById(mojom::Cursor cursor) = 0;

  virtual void UpdateTextInputState(const ui::TextInputState& state) = 0;
  virtual void SetImeVisibility(bool visible) = 0;

  virtual gfx::Rect GetBounds() const = 0;

  // Updates the viewport metrics for the display, returning true if any
  // metrics have changed.
  virtual bool UpdateViewportMetrics(
      const display::ViewportMetrics& metrics) = 0;

  virtual const display::ViewportMetrics& GetViewportMetrics() const = 0;

  // Returns the AcceleratedWidget associated with the Display. It can return
  // kNullAcceleratedWidget if the accelerated widget is not available yet.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() const = 0;

  virtual FrameGenerator* GetFrameGenerator() = 0;

  // Overrides factory for testing. Default (NULL) value indicates regular
  // (non-test) environment.
  static void set_factory_for_testing(PlatformDisplayFactory* factory) {
    PlatformDisplay::factory_ = factory;
  }

 private:
  // Static factory instance (always NULL for non-test).
  static PlatformDisplayFactory* factory_;
};


}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_PLATFORM_DISPLAY_H_
