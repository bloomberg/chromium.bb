// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_DELEGATE_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_DELEGATE_H_

#include <stdint.h>

namespace gfx {
class Rect;
}

namespace display {

class PlatformScreen;

// The PlatformScreenDelegate will be informed of changes to the physical
// and/or virtual displays by PlatformScreen.
class PlatformScreenDelegate {
 public:
  // TODO(kylechar): We need to provide more than just the window bounds when
  // displays are added or modified.

  // Called when a display is added. |bounds| is in DIP.
  virtual void OnDisplayAdded(PlatformScreen* platform_screen,
                              int64_t id,
                              const gfx::Rect& bounds) = 0;

  // Called when a display is removed.
  virtual void OnDisplayRemoved(int64_t id) = 0;

  // Called when a display is modified. |bounds| is in DIP.
  virtual void OnDisplayModified(int64_t id, const gfx::Rect& bounds) = 0;

 protected:
  virtual ~PlatformScreenDelegate() {}
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_DELEGATE_H_
