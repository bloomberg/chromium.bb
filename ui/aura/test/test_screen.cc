// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_screen.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/display_util.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/screen.h"

namespace aura {

TestScreen::TestScreen() : root_window_(NULL) {
  const std::string size_str =
      CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kAuraHostWindowSize);
  display_ = aura::CreateDisplayFromSpec(size_str);
}

TestScreen::~TestScreen() {
}

RootWindow* TestScreen::CreateRootWindowForPrimaryDisplay() {
  DCHECK(!root_window_);
  root_window_ = new RootWindow(RootWindow::CreateParams(display_.bounds()));
  root_window_->AddObserver(this);
  root_window_->Init();
  if (UseFullscreenHostWindow())
    root_window_->ConfineCursorToWindow();
  return root_window_;
}

bool TestScreen::IsDIPEnabled() {
  return true;
}

void TestScreen::OnWindowBoundsChanged(
    Window* window, const gfx::Rect& old_bounds, const gfx::Rect& new_bounds) {
  if (!UseFullscreenHostWindow()) {
    DCHECK_EQ(root_window_, window);
    display_.SetSize(new_bounds.size());
  }
}

void TestScreen::OnWindowDestroying(Window* window) {
  if (root_window_ == window)
    root_window_ = NULL;
}

gfx::Point TestScreen::GetCursorScreenPoint() {
  return Env::GetInstance()->last_mouse_location();
}

gfx::NativeWindow TestScreen::GetWindowAtCursorScreenPoint() {
  const gfx::Point point = GetCursorScreenPoint();
  return root_window_->GetTopWindowContainingPoint(point);
}

int TestScreen::GetNumDisplays() {
  return 1;
}

gfx::Display TestScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

gfx::Display TestScreen::GetDisplayNearestPoint(const gfx::Point& point) const {
  return display_;
}

gfx::Display TestScreen::GetDisplayMatching(const gfx::Rect& match_rect) const {
  return display_;
}

gfx::Display TestScreen::GetPrimaryDisplay() const {
  return display_;
}

void TestScreen::AddObserver(gfx::DisplayObserver* observer) {
}

void TestScreen::RemoveObserver(gfx::DisplayObserver* observer) {
}

}  // namespace aura
