// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_SCREEN_POSITION_CLIENT_MUS_H_
#define UI_VIEWS_MUS_SCREEN_POSITION_CLIENT_MUS_H_

#include "base/macros.h"
#include "ui/views/mus/mus_export.h"
#include "ui/views/widget/desktop_aura/desktop_screen_position_client.h"

namespace aura {
class WindowTreeHostMus;
}

namespace views {

// ScreenPositionClient for mus. Ensures unnecessary conversions to pixels
// doesn't happen.
class VIEWS_MUS_EXPORT ScreenPositionClientMus
    : public DesktopScreenPositionClient {
 public:
  explicit ScreenPositionClientMus(aura::WindowTreeHostMus* host);
  ~ScreenPositionClientMus() override;

  // DesktopScreenPositionClient:
  gfx::Point GetOriginInScreen(const aura::Window* window) override;

 private:
  aura::WindowTreeHostMus* host_;

  DISALLOW_COPY_AND_ASSIGN(ScreenPositionClientMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_SCREEN_POSITION_CLIENT_MUS_H_
