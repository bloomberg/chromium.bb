// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/single_display_manager.h"

#include <string>

#include "base/command_line.h"
#include "ui/aura/aura_switches.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

namespace aura {

using std::string;

namespace {
// Default bounds for the primary display.
const int kDefaultHostWindowX = 200;
const int kDefaultHostWindowY = 200;
const int kDefaultHostWindowWidth = 1280;
const int kDefaultHostWindowHeight = 1024;
}  // namespace

SingleDisplayManager::SingleDisplayManager()
    : root_window_(NULL) {
  Init();
}

SingleDisplayManager::~SingleDisplayManager() {
  // All displays must have been deleted when display manager is deleted.
  CHECK(!root_window_);
}

void SingleDisplayManager::OnNativeDisplaysChanged(
    const std::vector<gfx::Display>& displays) {
  DCHECK(displays.size() > 0);
  if (use_fullscreen_host_window()) {
    display_.SetSize(displays[0].bounds().size());
    NotifyBoundsChanged(display_);
  }
}

RootWindow* SingleDisplayManager::CreateRootWindowForDisplay(
    const gfx::Display& display) {
  DCHECK(!root_window_);
  DCHECK_EQ(display_.id(), display.id());
  root_window_ = new RootWindow(RootWindow::CreateParams(display.bounds()));
  root_window_->AddObserver(this);
  root_window_->Init();
  return root_window_;
}

gfx::Display* SingleDisplayManager::GetDisplayAt(size_t index) {
  return &display_;
}

size_t SingleDisplayManager::GetNumDisplays() const {
  return 1;
}

const gfx::Display& SingleDisplayManager::GetDisplayNearestWindow(
    const Window* window) const {
  return display_;
}

const gfx::Display& SingleDisplayManager::GetDisplayNearestPoint(
    const gfx::Point& point) const {
  return display_;
}

const gfx::Display& SingleDisplayManager::GetDisplayMatching(
    const gfx::Rect& match_rect) const {
  return display_;
}

std::string SingleDisplayManager::GetDisplayNameAt(size_t index) {
  return "Display";
}

void SingleDisplayManager::OnWindowBoundsChanged(
    Window* window, const gfx::Rect& old_bounds, const gfx::Rect& new_bounds) {
  if (!use_fullscreen_host_window()) {
    Update(new_bounds.size());
    NotifyBoundsChanged(display_);
  }
}

void SingleDisplayManager::OnWindowDestroying(Window* window) {
  if (root_window_ == window)
    root_window_ = NULL;
}

void SingleDisplayManager::Init() {
  const string size_str = CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
      switches::kAuraHostWindowSize);
  display_ = CreateDisplayFromSpec(size_str);
}

void SingleDisplayManager::Update(const gfx::Size size) {
  display_.SetSize(size);
}

}  // namespace aura
