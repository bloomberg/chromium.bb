// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_CURSOR_CLIENT_H_
#define UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_CURSOR_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/views/views_export.h"

namespace aura {
class RootWindow;
}

namespace ui {
class CursorLoader;
}

namespace views {

// A CursorClient that interacts with only one RootWindow. (Unlike the one in
// ash, which interacts with all the RootWindows.)
class VIEWS_EXPORT DesktopCursorClient : public aura::client::CursorClient {
 public:
  explicit DesktopCursorClient(aura::RootWindow* window);
  virtual ~DesktopCursorClient();

  // Overridden from client::CursorClient:
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual void ShowCursor() OVERRIDE;
  virtual void HideCursor() OVERRIDE;
  virtual bool IsCursorVisible() const OVERRIDE;
  virtual void EnableMouseEvents() OVERRIDE;
  virtual void DisableMouseEvents() OVERRIDE;
  virtual bool IsMouseEventsEnabled() const OVERRIDE;
  virtual void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;
  virtual void LockCursor() OVERRIDE;
  virtual void UnlockCursor() OVERRIDE;

 private:
  void SetCursorVisibility(bool visible);

  aura::RootWindow* root_window_;
  scoped_ptr<ui::CursorLoader> cursor_loader_;

  gfx::NativeCursor current_cursor_;
  bool cursor_visible_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCursorClient);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_DESKTOP_AURA_DESKTOP_CURSOR_CLIENT_H_
