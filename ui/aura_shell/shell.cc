// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/shell.h"

#include "base/bind.h"
#include "ui/aura/desktop.h"
#include "ui/aura/screen_aura.h"
#include "ui/aura/toplevel_window_container.h"
#include "ui/aura/window.h"
#include "ui/aura/window_types.h"
#include "ui/aura_shell/desktop_layout_manager.h"
#include "ui/aura_shell/launcher/launcher.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_factory.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/gfx/compositor/layer.h"
#include "views/widget/widget.h"

namespace aura_shell {

namespace {

// Creates each of the special window containers that holds windows of various
// types in the shell UI. They are added to |containers| from back to front in
// the z-index.
void CreateSpecialContainers(aura::Window::Windows* containers) {
  aura::Window* background_container = new aura::Window(NULL);
  background_container->set_id(
      internal::kShellWindowId_DesktopBackgroundContainer);
  containers->push_back(background_container);

  aura::Window* default_container = new aura::ToplevelWindowContainer;
  default_container->set_id(internal::kShellWindowId_DefaultContainer);
  containers->push_back(default_container);

  aura::Window* always_on_top_container = new aura::ToplevelWindowContainer;
  always_on_top_container->set_id(
      internal::kShellWindowId_AlwaysOnTopContainer);
  containers->push_back(always_on_top_container);

  aura::Window* launcher_container = new aura::Window(NULL);
  launcher_container->set_id(internal::kShellWindowId_LauncherContainer);
  containers->push_back(launcher_container);

  aura::Window* lock_container = new aura::Window(NULL);
  lock_container->set_stops_event_propagation(true);
  lock_container->set_id(internal::kShellWindowId_LockScreenContainer);
  containers->push_back(lock_container);

  aura::Window* status_container = new aura::Window(NULL);
  status_container->set_id(internal::kShellWindowId_StatusContainer);
  containers->push_back(status_container);

  aura::Window* menu_container = new aura::Window(NULL);
  menu_container->set_id(internal::kShellWindowId_MenusAndTooltipsContainer);
  containers->push_back(menu_container);
}

typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

void CalculateWindowBoundsAndScaleForTiling(
    const gfx::Size& containing_size,
    const aura::Window::Windows& windows,
    float* x_scale,
    float* y_scale,
    std::vector<WindowAndBoundsPair>* bounds) {
  *x_scale = 1.0f;
  *y_scale = 1.0f;
  int total_width = 0;
  int max_height = 0;
  int shown_window_count = 0;
  for (aura::Window::Windows::const_iterator i = windows.begin();
       i != windows.end(); ++i) {
    if ((*i)->IsVisible()) {
      total_width += (*i)->bounds().width();
      max_height = std::max((*i)->bounds().height(), max_height);
      shown_window_count++;
    }
  }

  if (shown_window_count == 0)
    return;

  const int kPadding = 10;
  total_width += (shown_window_count - 1) * kPadding;
  if (total_width > containing_size.width()) {
    *x_scale = static_cast<float>(containing_size.width()) /
        static_cast<float>(total_width);
  }
  if (max_height > containing_size.height()) {
    *y_scale = static_cast<float>(containing_size.height()) /
        static_cast<float>(max_height);
  }
  *x_scale = *y_scale = std::min(*x_scale, *y_scale);

  int x = std::max(0, static_cast<int>(
      (containing_size.width() * - total_width * *x_scale) / 2));
  for (aura::Window::Windows::const_iterator i = windows.begin();
       i != windows.end();
       ++i) {
    if ((*i)->IsVisible()) {
      const gfx::Rect& current_bounds((*i)->bounds());
      int y = (containing_size.height() -
               current_bounds.height() * *y_scale) / 2;
      bounds->push_back(std::make_pair(*i,
          gfx::Rect(x, y, current_bounds.width(), current_bounds.height())));
      x += static_cast<int>(
          static_cast<float>(current_bounds.width() + kPadding) * *x_scale);
    }
  }
}

}  // namespace

// static
Shell* Shell::instance_ = NULL;

////////////////////////////////////////////////////////////////////////////////
// Shell, public:

Shell::Shell()
    : ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)) {
  aura::Desktop::GetInstance()->SetDelegate(this);
}

Shell::~Shell() {
}

// static
Shell* Shell::GetInstance() {
  if (!instance_) {
    instance_ = new Shell;
    instance_->Init();
  }
  return instance_;
}

void Shell::Init() {
  aura::Window::Windows containers;
  CreateSpecialContainers(&containers);
  aura::Window::Windows::const_iterator i;
  aura::Desktop* desktop_window = aura::Desktop::GetInstance();
  for (i = containers.begin(); i != containers.end(); ++i) {
    (*i)->Init();
    desktop_window->AddChild(*i);
    (*i)->Show();
  }

  internal::DesktopLayoutManager* desktop_layout =
      new internal::DesktopLayoutManager(desktop_window);
  desktop_window->SetLayoutManager(desktop_layout);

  desktop_layout->set_background_widget(internal::CreateDesktopBackground());
  aura::ToplevelWindowContainer* toplevel_container =
      GetContainer(internal::kShellWindowId_DefaultContainer)->
          AsToplevelWindowContainer();
  launcher_.reset(new Launcher(toplevel_container));
  desktop_layout->set_launcher_widget(launcher_->widget());
  desktop_layout->set_status_area_widget(internal::CreateStatusArea());
  aura::Desktop::GetInstance()->screen()->set_work_area_insets(
      gfx::Insets(0, 0, launcher_->widget()->GetWindowScreenBounds().height(),
                  0));
}

void Shell::SetDelegate(ShellDelegate* delegate) {
  delegate_.reset(delegate);
}

aura::Window* Shell::GetContainer(int container_id) {
  return const_cast<aura::Window*>(
      const_cast<const aura_shell::Shell*>(this)->GetContainer(container_id));
}

const aura::Window* Shell::GetContainer(int container_id) const {
  return aura::Desktop::GetInstance()->GetChildById(container_id);
}

void Shell::TileWindows() {
  to_restore_.clear();
  aura::Window* window_container =
      aura::Desktop::GetInstance()->GetChildById(
          internal::kShellWindowId_DefaultContainer);
  const aura::Window::Windows& windows = window_container->children();
  if (windows.empty())
    return;
  float x_scale = 1.0f;
  float y_scale = 1.0f;
  std::vector<WindowAndBoundsPair> bounds;
  CalculateWindowBoundsAndScaleForTiling(window_container->bounds().size(),
                                         windows, &x_scale, &y_scale, &bounds);
  if (bounds.empty())
    return;
  ui::Transform transform;
  transform.SetScale(x_scale, y_scale);
  for (size_t i = 0; i < bounds.size(); ++i) {
    to_restore_.push_back(
        std::make_pair(bounds[i].first, bounds[i].first->bounds()));
    bounds[i].first->layer()->SetAnimation(
        aura::Window::CreateDefaultAnimation());
    bounds[i].first->SetBounds(bounds[i].second);
    bounds[i].first->layer()->SetTransform(transform);
    bounds[i].first->layer()->SetOpacity(0.5f);
  }

  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&Shell::RestoreTiledWindows, method_factory_.GetWeakPtr()),
      2000);
}

void Shell::RestoreTiledWindows() {
  ui::Transform identity_transform;
  for (size_t i = 0; i < to_restore_.size(); ++i) {
    to_restore_[i].first->layer()->SetAnimation(
        aura::Window::CreateDefaultAnimation());
    to_restore_[i].first->SetBounds(to_restore_[i].second);
    to_restore_[i].first->layer()->SetTransform(identity_transform);
    to_restore_[i].first->layer()->SetOpacity(1.0f);
  }
  to_restore_.clear();
}

////////////////////////////////////////////////////////////////////////////////
// Shell, aura::DesktopDelegate implementation:

void Shell::AddChildToDefaultParent(aura::Window* window) {
  aura::Window* parent = NULL;
  switch (window->type()) {
    case aura::kWindowType_Toplevel:
    case aura::kWindowType_Control:
      parent = GetContainer(internal::kShellWindowId_DefaultContainer);
      break;
    case aura::kWindowType_Menu:
    case aura::kWindowType_Tooltip:
      parent = GetContainer(internal::kShellWindowId_MenusAndTooltipsContainer);
      break;
    default:
      // This will crash for controls, since they can't be parented to anything.
      break;
  }
  parent->AddChild(window);
}

aura::Window* Shell::GetTopmostWindowToActivate(aura::Window* ignore) const {
  const aura::ToplevelWindowContainer* container =
      GetContainer(internal::kShellWindowId_DefaultContainer)->
          AsToplevelWindowContainer();
  return container->GetTopmostWindowToActivate(ignore);
}

}  // namespace aura_shell
