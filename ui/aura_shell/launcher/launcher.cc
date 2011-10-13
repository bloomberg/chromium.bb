// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher.h"

#include "ui/aura/toplevel_window_container.h"
#include "ui/aura_shell/launcher/launcher_model.h"
#include "ui/aura_shell/launcher/launcher_view.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/gfx/compositor/layer.h"
#include "views/widget/widget.h"

namespace aura_shell {

Launcher::Launcher(aura::ToplevelWindowContainer* window_container)
    : widget_(NULL),
      window_container_(window_container) {
  window_container->AddObserver(this);

  model_.reset(new LauncherModel);

  widget_ = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.parent = Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_LauncherContainer);
  internal::LauncherView* launcher_view =
      new internal::LauncherView(model_.get());
  launcher_view->Init();
  params.delegate = launcher_view;
  widget_->Init(params);
  widget_->GetNativeWindow()->layer()->SetOpacity(0.8f);
  gfx::Size pref = static_cast<views::View*>(launcher_view)->GetPreferredSize();
  widget_->SetBounds(gfx::Rect(0, 0, pref.width(), pref.height()));
  widget_->SetContentsView(launcher_view);
  widget_->Show();
  widget_->GetNativeView()->set_name("LauncherView");
}

Launcher::~Launcher() {
  window_container_->RemoveObserver(this);
}

void Launcher::OnWindowAdded(aura::Window* new_window) {
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  if (!delegate)
    return;
  LauncherItem item;
  item.window = new_window;
  if (!delegate->ConfigureLauncherItem(&item))
    return;  // The delegate doesn't want to show this item in the launcher.
  model_->Add(model_->items().size(), item);
}

void Launcher::OnWillRemoveWindow(aura::Window* window) {
  const LauncherItems& items(model_->items());
  for (LauncherItems::const_iterator i = items.begin(); i != items.end(); ++i) {
    if (i->window == window) {
      model_->RemoveItemAt(i - items.begin());
      break;
    }
  }
}

}  // namespace aura_shell
