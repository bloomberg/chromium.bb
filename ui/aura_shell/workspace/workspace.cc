// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/workspace/workspace.h"

#include <algorithm>

#include "base/logging.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/desktop.h"
#include "ui/aura/window.h"
#include "ui/aura_shell/property_util.h"
#include "ui/aura_shell/workspace/workspace_manager.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animator.h"

namespace {
// Horizontal margin between windows.
const int kWindowHorizontalMargin = 10;

// Maximum number of windows a workspace can have.
size_t g_max_windows_per_workspace = 2;

// Returns the bounds of the window that should be used to calculate
// the layout. It uses the restore bounds if exits, or
// the target bounds of the window. The target bounds is the
// final destination of |window| if the window's layer is animating,
// or the current bounds of the window of no animation is currently
// in progress.
gfx::Rect GetLayoutBounds(aura::Window* window) {
  const gfx::Rect* restore_bounds = aura_shell::GetRestoreBounds(window);
  return restore_bounds ? *restore_bounds : window->GetTargetBounds();
}

// Returns the width of the window that should be used to calculate
// the layout. See |GetLayoutBounds| for more details.
int GetLayoutWidth(aura::Window* window) {
  return GetLayoutBounds(window).width();
}

}  // namespace

namespace aura_shell {
namespace internal {

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

aura::Window* Workspace::FindRotateWindowForLocation(
    const gfx::Point& position) {
  aura::Window* active = aura::Desktop::GetInstance()->active_window();
  if (GetTotalWindowsWidth() < bounds_.width()) {
    // If all windows fit to the width of the workspace, it returns the
    // window which contains |position|'s x coordinate.
    for (aura::Window::Windows::const_iterator i = windows_.begin();
         i != windows_.end();
         ++i) {
      if (active == *i)
        continue;
      gfx::Rect bounds = (*i)->GetTargetBounds();
      if (bounds.x() < position.x() && position.x() < bounds.right())
        return *i;
    }
  } else if (bounds_.x() < position.x() && position.x() < bounds_.right()) {
    // If windows are overlapping, it divides the workspace into
    // regions with the same width, and returns the Nth window that
    // corresponds to the region that contains the |position|.
    int width = bounds_.width() / windows_.size();
    size_t index = (position.x() - bounds_.x()) / width;
    DCHECK(index < windows_.size());
    aura::Window* window = windows_[index];
    if (window != active)
      return window;
  }
  return NULL;
}

void Workspace::RotateWindows(aura::Window* source, aura::Window* target) {
  DCHECK(Contains(source));
  DCHECK(Contains(target));
  aura::Window::Windows::iterator source_iter =
      std::find(windows_.begin(), windows_.end(), source);
  aura::Window::Windows::iterator target_iter =
      std::find(windows_.begin(), windows_.end(), target);
  DCHECK(source_iter != target_iter);
  if (source_iter < target_iter)
    std::rotate(source_iter, source_iter + 1, target_iter + 1);
  else
    std::rotate(target_iter, source_iter, source_iter + 1);
  Layout(NULL);
}

aura::Window* Workspace::ShiftWindows(aura::Window* insert,
                                      aura::Window* until,
                                      aura::Window* target,
                                      ShiftDirection direction) {
  DCHECK(until);
  DCHECK(!Contains(insert));

  bool shift_reached_until = GetIndexOf(until) >= 0;
  if (shift_reached_until) {
    // Calling RemoveWindow here causes the animation set in Layout below
    // to be ignored.  See crbug.com/102413.
    windows_.erase(std::find(windows_.begin(), windows_.end(), until));
  }
  aura::Window* pushed = NULL;
  if (direction == SHIFT_TO_RIGHT) {
    aura::Window::Windows::iterator iter =
        std::find(windows_.begin(), windows_.end(), target);
    // Insert at |target| position, or at the begining.
    if (iter == windows_.end())
      iter = windows_.begin();
    windows_.insert(iter, insert);
    if (!shift_reached_until) {
      pushed = windows_.back();
      windows_.erase(--windows_.end());
    }
  } else {
    aura::Window::Windows::iterator iter =
        std::find(windows_.begin(), windows_.end(), target);
    // Insert after |target|, or at the end.
    if (iter != windows_.end())
      ++iter;
    windows_.insert(iter, insert);
    if (!shift_reached_until) {
      pushed = windows_.front();
      windows_.erase(windows_.begin());
    }
  }
  Layout(NULL);
  return pushed;
}

void Workspace::Activate() {
  workspace_manager_->SetActiveWorkspace(this);
}

void Workspace::Layout(aura::Window* no_animation) {
  aura::Window* ignore = workspace_manager_->ignored_window();
  workspace_manager_->set_layout_in_progress(true);
  gfx::Rect work_area = workspace_manager_->GetWorkAreaBounds(bounds_);
  int total_width = GetTotalWindowsWidth();
  if (total_width < work_area.width()) {
    int dx = (work_area.width() - total_width) / 2;
    for (aura::Window::Windows::iterator i = windows_.begin();
         i != windows_.end();
         ++i) {
      if (*i != ignore) {
        MoveWindowTo(*i,
                     gfx::Point(work_area.x() + dx, work_area.y()),
                     no_animation != *i);
      }
      dx += GetLayoutWidth(*i) + kWindowHorizontalMargin;
    }
  } else {
    DCHECK_LT(windows_.size(), 3U);
    // TODO(oshima): This is messy. Figure out general algorithm to
    // layout more than 2 windows.
    if (windows_[0] != ignore) {
      MoveWindowTo(windows_[0],
                   work_area.origin(),
                   no_animation != windows_[0]);
    }
    if (windows_.size() == 2 && windows_[1] != ignore) {
      MoveWindowTo(windows_[1],
                   gfx::Point(work_area.right() - GetLayoutWidth(windows_[1]),
                              work_area.y()),
                   no_animation != windows_[1]);
    }
  }
  workspace_manager_->set_layout_in_progress(false);
}

bool Workspace::ContainsFullscreenWindow() const {
  for (aura::Window::Windows::const_iterator i = windows_.begin();
       i != windows_.end();
       ++i) {
    aura::Window* w = *i;
    if (w->IsVisible() &&
        w->GetIntProperty(aura::kShowStateKey) == ui::SHOW_STATE_FULLSCREEN)
      return true;
  }
  return false;
}

int Workspace::GetIndexOf(aura::Window* window) const {
  aura::Window::Windows::const_iterator i =
      std::find(windows_.begin(), windows_.end(), window);
  return i == windows_.end() ? -1 : i - windows_.begin();
}

bool Workspace::CanAdd(aura::Window* window) const {
  // TODO(oshima): This should be based on available space and the
  // size of the |window|.
  //NOTIMPLEMENTED();
  return windows_.size() < g_max_windows_per_workspace;
}

void Workspace::MoveWindowTo(
    aura::Window* window,
    const gfx::Point& origin,
    bool animate) {
  gfx::Rect bounds = GetLayoutBounds(window);
  gfx::Rect work_area = GetWorkAreaBounds();
  // Make sure the window isn't bigger than the workspace size.
  bounds.SetRect(origin.x(), origin.y(),
                 std::min(work_area.width(), bounds.width()),
                 std::min(work_area.height(), bounds.height()));
  if (animate) {
    ui::LayerAnimator::ScopedSettings settings(window->layer()->GetAnimator());
    window->SetBounds(bounds);
  } else {
    window->SetBounds(bounds);
  }
}

int Workspace::GetTotalWindowsWidth() const {
  int total_width = 0;
  for (aura::Window::Windows::const_iterator i = windows_.begin();
       i != windows_.end();
       ++i) {
    if (total_width)
      total_width += kWindowHorizontalMargin;
    total_width += GetLayoutWidth(*i);
  }
  return total_width;
}

// static
size_t Workspace::SetMaxWindowsCount(size_t max) {
  int old = g_max_windows_per_workspace;
  g_max_windows_per_workspace = max;
  return old;
}

}  // namespace internal
}  // namespace aura_shell
