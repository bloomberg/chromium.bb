// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCREEN_POSITION_CLIENT_H_
#define UI_AURA_SCREEN_POSITION_CLIENT_H_

#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

namespace gfx {
class Display;
class Rect;
}

namespace aura {
class Window;
namespace client {

// An interface implemented by an object that changes coordinates within a root
// Window into system coordinates.
class AURA_EXPORT ScreenPositionClient {
 public:
  virtual ~ScreenPositionClient() {}

  // Converts the |screen_point| from a given |window|'s coordinate space
  // into screen coordinate space.
  // TODO(crbug.com/773331): remove int version of point conversion when
  // current usage are changed to use float version.
  virtual void ConvertPointToScreen(const Window* window,
                                    gfx::PointF* point) = 0;
  virtual void ConvertPointFromScreen(const Window* window,
                                      gfx::PointF* point) = 0;
  void ConvertPointToScreen(const Window* window, gfx::Point* point);
  void ConvertPointFromScreen(const Window* window, gfx::Point* point);
  // Converts the |screen_point| from root window host's coordinate of
  // into screen coordinate space.
  // A typical example of using this function instead of ConvertPointToScreen is
  // when X's native input is captured by a drag operation.
  // See the comments for ash::GetRootWindowRelativeToWindow for details.
  virtual void ConvertHostPointToScreen(Window* root_window,
                                        gfx::Point* point) = 0;
  // Sets the bounds of the window. The implementation is responsible
  // for finding out and translating the right coordinates for the |window|.
  virtual void SetBounds(Window* window,
                         const gfx::Rect& bounds,
                         const display::Display& display) = 0;
};

// Sets/Gets the activation client on the Window.
AURA_EXPORT void SetScreenPositionClient(Window* root_window,
                                         ScreenPositionClient* client);
AURA_EXPORT ScreenPositionClient* GetScreenPositionClient(
    const Window* root_window);

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_SCREEN_POSITION_CLIENT_H_
