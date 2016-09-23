// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_DELEGATE_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_DELEGATE_H_

#include <stdint.h>

namespace gfx {
class Rect;
class Size;
}

namespace display {

class PlatformScreen;

// The PlatformScreenDelegate will be informed of changes to the physical
// and/or virtual displays by PlatformScreen.
class PlatformScreenDelegate {
 public:
  // Called when a display is added. |id| is the display id for the new display,
  // |bounds| is the display origin and size in DIP, |pixel_size| is the size
  // of the display in DDP and |device_scale_factor| is the output device pixel
  // scale factor.
  virtual void OnDisplayAdded(int64_t id,
                              const gfx::Rect& bounds,
                              const gfx::Size& pixel_size,
                              float device_scale_factor) = 0;

  // Called when a display is removed. |id| is the display id for the display
  // that was removed.
  virtual void OnDisplayRemoved(int64_t id) = 0;

  // Called when a display is modified. See OnDisplayAdded() for parameter
  // information.
  virtual void OnDisplayModified(int64_t id,
                                 const gfx::Rect& bounds,
                                 const gfx::Size& pixel_size,
                                 float device_scale_factor) = 0;

 protected:
  virtual ~PlatformScreenDelegate() {}
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_DELEGATE_H_
