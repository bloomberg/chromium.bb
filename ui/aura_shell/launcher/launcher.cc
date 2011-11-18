// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher.h"

#include "ui/aura/window.h"
#include "ui/aura_shell/launcher/launcher_model.h"
#include "ui/aura_shell/launcher/launcher_view.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/gfx/compositor/layer.h"
#include "views/widget/widget.h"

namespace aura_shell {

Launcher::Launcher(aura::Window* window_container)
    : widget_(NULL),
      window_container_(window_container) {
  window_container->AddObserver(this);

  model_.reset(new LauncherModel);

  widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  // All the content is drawn by the launcher buttons. Turn off the widget's
  // layer's texture to avoid unnecessary memory use.
  params.create_texture_for_layer = false;
  params.parent = Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_LauncherContainer);
  internal::LauncherView* launcher_view =
      new internal::LauncherView(model_.get());
  launcher_view->Init();
  params.delegate = launcher_view;
  widget_->Init(params);
  gfx::Size pref = static_cast<views::View*>(launcher_view)->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(0, 0, pref.width(), pref.height()));
  widget_->SetContentsView(launcher_view);
  widget_->Show();
  widget_->GetNativeView()->set_name("LauncherView");
}

Launcher::~Launcher() {
  widget_->CloseNow();
  window_container_->RemoveObserver(this);
  for (WindowMap::iterator i = known_windows_.begin();
       i != known_windows_.end(); ++i) {
    i->first->RemoveObserver(this);
  }
}

void Launcher::MaybeAdd(aura::Window* window) {
  if (known_windows_[window] == true)
    return;  // We already tried to add this window.

  known_windows_[window] = true;
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  if (!delegate)
    return;
  LauncherItem item;
  item.window = window;
  if (!delegate->ConfigureLauncherItem(&item))
    return;  // The delegate doesn't want to show this item in the launcher.
  model_->Add(model_->items().size(), item);
}

void Launcher::OnWindowAdded(aura::Window* new_window) {
  if (new_window->parent() != window_container_)
    return;

  DCHECK(known_windows_.find(new_window) == known_windows_.end());
  known_windows_[new_window] = false;
  new_window->AddObserver(this);
  // Windows are created initially invisible. Wait until the window is made
  // visible before asking, as othewise the delegate likely doesn't know about
  // window (it's still creating it).
  if (new_window->IsVisible())
    MaybeAdd(new_window);
}

void Launcher::OnWillRemoveWindow(aura::Window* window) {
  if (window->parent() != window_container_)
    return;

  window->RemoveObserver(this);
  known_windows_.erase(window);
  LauncherItems::const_iterator i = model_->ItemByWindow(window);
  if (i != model_->items().end())
    model_->RemoveItemAt(i - model_->items().begin());
}

void Launcher::OnWindowVisibilityChanged(aura::Window* window,
                                         bool visibile) {
  if (visibile && !known_windows_[window])
    MaybeAdd(window);
}

}  // namespace aura_shell
