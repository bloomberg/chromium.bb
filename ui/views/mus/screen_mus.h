// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_MUS_SCREEN_MUS_H_
#define UI_VIEWS_MUS_SCREEN_MUS_H_

#include "mojo/public/cpp/bindings/binding.h"
#include "services/ui/public/interfaces/display.mojom.h"
#include "ui/display/screen_base.h"
#include "ui/views/mus/mus_export.h"

namespace shell {
class Connector;
}

namespace views {

class ScreenMusDelegate;

// Screen implementation backed by ui::mojom::DisplayManager.
class VIEWS_MUS_EXPORT ScreenMus
    : public display::ScreenBase,
      public NON_EXPORTED_BASE(ui::mojom::DisplayManagerObserver) {
 public:
  // |delegate| can be nullptr.
  explicit ScreenMus(ScreenMusDelegate* delegate);
  ~ScreenMus() override;

  void Init(shell::Connector* connector);

 private:
  // display::Screen:
  gfx::Point GetCursorScreenPoint() override;
  bool IsWindowUnderCursor(gfx::NativeWindow window) override;

  // ui::mojom::DisplayManager:
  void OnDisplays(mojo::Array<ui::mojom::WsDisplayPtr> ws_displays) override;
  void OnDisplaysChanged(
      mojo::Array<ui::mojom::WsDisplayPtr> ws_displays) override;
  void OnDisplayRemoved(int64_t id) override;

  ScreenMusDelegate* delegate_;  // Can be nullptr.
  ui::mojom::DisplayManagerPtr display_manager_;
  mojo::Binding<ui::mojom::DisplayManagerObserver>
      display_manager_observer_binding_;

  DISALLOW_COPY_AND_ASSIGN(ScreenMus);
};

}  // namespace views

#endif  // UI_VIEWS_MUS_SCREEN_MUS_H_
