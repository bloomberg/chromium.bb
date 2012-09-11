// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_CLIENT_CURSOR_CLIENT_H_
#define UI_AURA_CLIENT_CURSOR_CLIENT_H_

#include "ui/aura/aura_export.h"
#include "ui/gfx/native_widget_types.h"

namespace aura {
class Window;
namespace client {

// An interface that receives cursor change events.
class AURA_EXPORT CursorClient {
 public:
  // Notes that |window| has requested the change to |cursor|.
  virtual void SetCursor(gfx::NativeCursor cursor) = 0;

  // Changes the visibility of the cursor.
  virtual void ShowCursor(bool show) = 0;

  // Gets whether the cursor is visible.
  virtual bool IsCursorVisible() const = 0;

  // Sets the device scale factor of the cursor.
  virtual void SetDeviceScaleFactor(float device_scale_factor) = 0;

 protected:
  virtual ~CursorClient() {}
};

// Sets/Gets the activation client for the specified window.
AURA_EXPORT void SetCursorClient(Window* window,
                                 CursorClient* client);
AURA_EXPORT CursorClient* GetCursorClient(Window* window);

}  // namespace client
}  // namespace aura

#endif  // UI_AURA_CLIENT_CURSOR_CLIENT_H_
