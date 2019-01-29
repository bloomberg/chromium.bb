// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/cursor_manager_owner.h"

#include <memory>

#include "ui/aura/client/cursor_client.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/base/cursor/cursor.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/native_cursor_manager.h"
#include "ui/wm/core/native_cursor_manager_delegate.h"

namespace views {

namespace {

class NativeCursorManagerMus : public wm::NativeCursorManager {
 public:
  explicit NativeCursorManagerMus(aura::Window* window) : window_(window) {}
  ~NativeCursorManagerMus() override = default;

 private:
  // wm::NativeCursorManager:
  void SetDisplay(const display::Display& display,
                  wm::NativeCursorManagerDelegate* delegate) override {
    // We ignore this entirely, as cursor are set on the client.
  }
  void SetCursor(ui::Cursor cursor,
                 wm::NativeCursorManagerDelegate* delegate) override {
    aura::WindowPortMus::Get(window_)->SetCursor(cursor);
    delegate->CommitCursor(cursor);
  }

  void SetVisibility(bool visible,
                     wm::NativeCursorManagerDelegate* delegate) override {
    delegate->CommitVisibility(visible);

    if (visible) {
      SetCursor(delegate->GetCursor(), delegate);
    } else {
      aura::WindowPortMus::Get(window_)->SetCursor(
          ui::Cursor(ui::CursorType::kNone));
    }
  }
  void SetCursorSize(ui::CursorSize cursor_size,
                     wm::NativeCursorManagerDelegate* delegate) override {
    // TODO(erg): For now, ignore the difference between SET_NORMAL and
    // SET_LARGE here. This feels like a thing that mus should decide instead.
    //
    // Also, it's NOTIMPLEMENTED() in the desktop version!? Including not
    // acknowledging the call in the delegate.
    NOTIMPLEMENTED();
  }
  void SetMouseEventsEnabled(
      bool enabled,
      wm::NativeCursorManagerDelegate* delegate) override {
    // TODO(erg): How do we actually implement this?
    //
    // Mouse event dispatch is potentially done in a different process,
    // definitely in a different mojo service. Each app is fairly locked down.
    delegate->CommitMouseEventsEnabled(enabled);
    NOTIMPLEMENTED();
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(NativeCursorManagerMus);
};

}  // namespace

CursorManagerOwner::CursorManagerOwner(aura::Window* window)
    : cursor_manager_(std::make_unique<wm::CursorManager>(
          std::make_unique<NativeCursorManagerMus>(window))) {
  tracker_.Add(window);
  aura::client::SetCursorClient(window, cursor_manager_.get());
}

CursorManagerOwner::~CursorManagerOwner() {
  if (!tracker_.windows().empty())
    aura::client::SetCursorClient(tracker_.Pop(), nullptr);
}

}  // namespace views
