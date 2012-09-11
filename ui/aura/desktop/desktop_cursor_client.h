// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_DESKTOP_CURSOR_CLIENT_H_
#define UI_AURA_DESKTOP_DESKTOP_CURSOR_CLIENT_H_

#include "base/compiler_specific.h"
#include "ui/aura/aura_export.h"

#include "ui/aura/client/cursor_client.h"

namespace aura {
class RootWindow;

// A CursorClient that interacts with only one RootWindow. (Unlike the one in
// ash, which interacts with all the RootWindows.)
class AURA_EXPORT DesktopCursorClient : public client::CursorClient {
 public:
  explicit DesktopCursorClient(aura::RootWindow* window);
  virtual ~DesktopCursorClient();

  // Overridden from client::CursorClient:
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual void ShowCursor(bool show) OVERRIDE;
  virtual bool IsCursorVisible() const OVERRIDE;
  virtual void SetDeviceScaleFactor(float device_scale_factor) OVERRIDE;

 private:
  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(DesktopCursorClient);
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_DESKTOP_CURSOR_CLIENT_H_
