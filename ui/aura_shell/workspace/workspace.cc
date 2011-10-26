// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/workspace/workspace.h"

#include "base/logging.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/gfx/compositor/layer.h"

namespace {
// Horizontal margin between windows.
const int kWindowHorizontalMargin = 10;
}

namespace aura_shell {

Workspace::Workspace(WorkspaceManager* manager)
    : workspace_manager_(manager) {
  workspace_manager_->AddWorkspace(this);
}

Workspace::~Workspace() {
  workspace_manager_->RemoveWorkspace(this);
}

void Workspace::SetBounds(const gfx::Rect& bounds) {
  bool bounds_changed = bounds_ != bounds;
  bounds_ = bounds;
  if (bounds_changed)
    Layout(NULL);
}

gfx::Rect Workspace::GetWorkAreaBounds() const {
  return workspace_manager_->GetWorkAreaBounds(bounds_);
}

bool Workspace::AddWindowAfter(aura::Window* window, aura::Window* after) {
  if (!CanAdd(window))
    return false;
  DCHECK(!Contains(window));

  if (!after) {  // insert at the end.
    windows_.push_back(window);
  } else {
    DCHECK(Contains(after));
    aura::Window::Windows::iterator i =
        std::find(windows_.begin(), windows_.end(), after);
    windows_.insert(++i, window);
  }
  Layout(window);

  return true;
}

void Workspace::RemoveWindow(aura::Window* window) {
  DCHECK(Contains(window));
  windows_.erase(std::find(windows_.begin(), windows_.end(), window));
  Layout(NULL);
}


bool Workspace::Contains(aura::Window* window) const {
  return std::find(windows_.begin(), windows_.end(), window) != windows_.end();
}

void Workspace::Activate() {
  workspace_manager_->SetActiveWorkspace(this);
}

void Workspace::Layout(aura::Window* no_animation) {
  gfx::Rect work_area = workspace_manager_->GetWorkAreaBounds(bounds_);
  int total_width = 0;
  for (aura::Window::Windows::const_iterator i = windows_.begin();
       i != windows_.end();
       i++) {
    if (total_width)
      total_width += kWindowHorizontalMargin;
    // TODO(oshima): use restored bounds.
    total_width += (*i)->bounds().width();
  }

  if (total_width < work_area.width()) {
    int dx = (work_area.width() - total_width) / 2;
    for (aura::Window::Windows::iterator i = windows_.begin();
         i != windows_.end();
         i++) {
      MoveWindowTo(*i,
                   gfx::Point(work_area.x() + dx, work_area.y()),
                   no_animation != *i);
      dx += (*i)->bounds().width() + kWindowHorizontalMargin;
    }
  } else {
    DCHECK_LT(windows_.size(), 3U);
    // TODO(oshima): Figure out general algorithm to layout more than
    // 2 windows.
    MoveWindowTo(windows_[0], work_area.origin(), no_animation != windows_[0]);
    if (windows_.size() == 2) {
      MoveWindowTo(windows_[1],
                   gfx::Point(work_area.right() - windows_[1]->bounds().width(),
                              work_area.y()),
                   no_animation != windows_[1]);
    }
  }
}

int Workspace::GetIndexOf(aura::Window* window) const {
  aura::Window::Windows::const_iterator i =
      std::find(windows_.begin(), windows_.end(), window);
  return i == windows_.end() ? -1 : i - windows_.begin();
}

bool Workspace::CanAdd(aura::Window* window) const {
  // TODO(oshima): This should be based on available space and the
  // size of the |window|.
  NOTIMPLEMENTED();
  return windows_.size() < 2;
}

void Workspace::MoveWindowTo(
    aura::Window* window,
    const gfx::Point& origin,
    bool animate) {
  if (window->show_state() == ui::SHOW_STATE_FULLSCREEN)
    window->Fullscreen();
  else if (window->show_state() == ui::SHOW_STATE_MAXIMIZED)
    window->Maximize();
  else {
    gfx::Rect bounds = window->GetTargetBounds();
    bounds.set_origin(origin);
    if (animate)
      window->layer()->SetAnimation(aura::Window::CreateDefaultAnimation());
    window->SetBounds(bounds);
  }
}

}  // namespace aura_shell
