// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_COMMON_WINDOW_PLATFORM_WINDOW_COMPAT_H_
#define UI_OZONE_COMMON_WINDOW_PLATFORM_WINDOW_COMPAT_H_

#include "ui/base/cursor/cursor.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/ozone_export.h"
#include "ui/platform_window/platform_window.h"

namespace gfx {
class Point;
class Rect;
}

namespace ui {

class PlatformWindowDelegate;

// This is just transitional code. Will be removed shortly.
class OZONE_EXPORT PlatformWindowCompat : public PlatformWindow,
                                          public ui::PlatformEventDispatcher {
 public:
  PlatformWindowCompat(PlatformWindowDelegate* delegate,
                       const gfx::Rect& bounds);
  virtual ~PlatformWindowCompat();

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE;
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE;

  // PlatformWindow:
  virtual gfx::Rect GetBounds() OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void ToggleFullscreen() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;

 private:
  PlatformWindowDelegate* delegate_;
  gfx::AcceleratedWidget widget_;
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(PlatformWindowCompat);
};

}  // namespace ui

#endif  // UI_OZONE_COMMON_WINDOW_PLATFORM_WINDOW_COMPAT_H_
