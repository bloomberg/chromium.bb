// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SCREEN_POSITION_CLIENT_H_
#define UI_AURA_SCREEN_POSITION_CLIENT_H_
#pragma once

#include "ui/aura/aura_export.h"
#include "ui/aura/window.h"

namespace aura {

class Event;
class Window;

namespace client {

// An interface implemented by an object that changes coordinates on a
// RootWindow into system coordinates.
class AURA_EXPORT ScreenPositionClient {
 public:
  // Converts the |screen_point| from a given RootWindow's coordinates space
  // into screen coordinate space.
  virtual void ConvertToScreenPoint(gfx::Point* screen_point) = 0;

  virtual ~ScreenPositionClient() {}
};

// Sets/Gets the activation client on the Window.
AURA_EXPORT void SetScreenPositionClient(Window* window,
                                         ScreenPositionClient* client);
AURA_EXPORT ScreenPositionClient* GetScreenPositionClient(
    Window* window);

}  // namespace clients
}  // namespace aura

#endif  // UI_AURA_SCREEN_POSITION_CLIENT_H_
