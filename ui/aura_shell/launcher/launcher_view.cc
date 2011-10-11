// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher_view.h"

#include "base/utf_string_conversions.h"
#include "ui/aura/desktop.h"
#include "ui/aura_shell/aura_shell_export.h"
#include "ui/aura_shell/launcher/launcher_model.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_window_ids.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/compositor/layer.h"
#include "views/widget/widget.h"

using views::View;

namespace aura_shell {
namespace internal {

// Padding between each view.
static const int kHorizontalPadding = 12;

// Height of the LauncherView. Hard coded to avoid resizing as items are
// added/removed.
static const int kPreferredHeight = 64;

LauncherView::LauncherView() : model_(NULL) {
}

LauncherView::~LauncherView() {
  // The model owns the views.
  RemoveAllChildViews(false);

  model_->RemoveObserver(this);
}

void LauncherView::Init(LauncherModel* model) {
  DCHECK(!model_);
  model_ = model;
  model_->AddObserver(this);
  for (int i = 0; i < model_->item_count(); ++i)
    AddChildView(model_->view_at(i));
}

void LauncherView::Layout() {
  // TODO: need to deal with overflow.
  int x = 0;
  for (int i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child->IsVisible()) {
      gfx::Size pref_size = child->GetPreferredSize();
      int y = (height() - pref_size.height()) / 2;
      child->SetBounds(x, y, pref_size.width(), pref_size.height());
      x += child->width() + kHorizontalPadding;
    }
  }
}

gfx::Size LauncherView::GetPreferredSize() {
  int x = 0;
  for (int i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child->IsVisible()) {
      gfx::Size pref_size = child->GetPreferredSize();
      x += pref_size.width() + kHorizontalPadding;
    }
  }
  if (x > 0)
    x -= kHorizontalPadding;
  return gfx::Size(x, kPreferredHeight);
}

void LauncherView::LauncherItemAdded(int index) {
  // TODO: to support animations is going to require coordinate conversions.
  AddChildViewAt(model_->view_at(index), index);
}

void LauncherView::LauncherItemRemoved(int index) {
  // TODO: to support animations is going to require coordinate conversions.
  RemoveChildView(child_at(index));
}

void LauncherView::LauncherSelectionChanged() {
}

views::Widget* CreateLauncher(LauncherModel* model) {
  views::Widget* launcher_widget = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.bounds = gfx::Rect(0, 0, 300, kPreferredHeight);
  params.parent = Shell::GetInstance()->GetContainer(
      aura_shell::internal::kShellWindowId_LauncherContainer);
  LauncherView* launcher_view = new LauncherView;
  launcher_view->Init(model);
  params.delegate = launcher_view;
  launcher_widget->Init(params);
  launcher_widget->GetNativeWindow()->layer()->SetOpacity(0.8f);
  gfx::Size pref = static_cast<views::View*>(launcher_view)->GetPreferredSize();
  launcher_widget->SetBounds(gfx::Rect(0, 0, pref.width(), pref.height()));
  launcher_widget->SetContentsView(launcher_view);
  launcher_widget->Show();
  launcher_widget->GetNativeView()->set_name("LauncherView");
  return launcher_widget;
}

}  // namespace internal
}  // namespace aura_shell
