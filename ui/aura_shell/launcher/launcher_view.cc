// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher_view.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/aura_shell/launcher/launcher_model.h"
#include "ui/aura_shell/launcher/tabbed_launcher_button.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "views/controls/button/image_button.h"
#include "views/widget/widget.h"

using views::View;

namespace aura_shell {
namespace internal {

// Padding between each view.
static const int kHorizontalPadding = 12;

// Amount content is inset on the left edge.
static const int kLeadingInset = 8;

// Height of the LauncherView. Hard coded to avoid resizing as items are
// added/removed.
static const int kPreferredHeight = 48;

LauncherView::LauncherView(LauncherModel* model)
    : model_(model),
      new_browser_button_(NULL),
      show_apps_button_(NULL) {
  DCHECK(model_);
}

LauncherView::~LauncherView() {
  model_->RemoveObserver(this);
}

void LauncherView::Init() {
  model_->AddObserver(this);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  new_browser_button_ = new views::ImageButton(this);
  new_browser_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_CHROME).ToSkBitmap());
  AddChildView(new_browser_button_);

  const LauncherItems& items(model_->items());
  for (LauncherItems::const_iterator i = items.begin(); i != items.end(); ++i)
    AddChildView(CreateViewForItem(*i));

  show_apps_button_ = new views::ImageButton(this);
  show_apps_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_APPLIST).ToSkBitmap());
  AddChildView(show_apps_button_);
}

views::View* LauncherView::CreateViewForItem(const LauncherItem& item) {
  if (item.type == TYPE_TABBED) {
    TabbedLauncherButton* button = new TabbedLauncherButton(this);
    button->SetImages(item.tab_images);
    return button;
  }
  views::ImageButton* button = new views::ImageButton(this);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  button->SetImage(
      views::CustomButton::BS_NORMAL,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_TABBED_BROWSER).ToSkBitmap());
  return button;
}

void LauncherView::Resize() {
  int y = GetWidget()->GetClientAreaScreenBounds().y();
  gfx::Size pref(GetPreferredSize());
  GetWidget()->SetBounds(gfx::Rect(0, y, pref.width(), pref.height()));
}

void LauncherView::Layout() {
  int x = kLeadingInset;
  for (int i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child->IsVisible()) {
      gfx::Size pref_size = child->GetPreferredSize();
      int y = (height() - pref_size.height()) / 2;
      child->SetBounds(x, y, pref_size.width(), pref_size.height());
      x += child->width() + kHorizontalPadding;
    }
  }
  // TODO: remove this when we get a better image.
  show_apps_button_->SetX(show_apps_button_->x() - 8);
}

gfx::Size LauncherView::GetPreferredSize() {
  int x = kLeadingInset;
  for (int i = 0; i < child_count(); ++i) {
    View* child = child_at(i);
    if (child->IsVisible()) {
      gfx::Size pref_size = child->GetPreferredSize();
      x += pref_size.width() + kHorizontalPadding;
    }
  }
  // TODO: remove this when we get a better image.
  x -= 10;
  return gfx::Size(x, kPreferredHeight);
}

void LauncherView::LauncherItemAdded(int index) {
  // TODO: to support animations is going to require coordinate conversions.
  AddChildViewAt(CreateViewForItem(model_->items()[index]), index + 1);
  Resize();
}

void LauncherView::LauncherItemRemoved(int index) {
  // TODO: to support animations is going to require coordinate conversions.
  RemoveChildView(child_at(index + 1));
  Resize();
}

void LauncherView::LauncherItemImagesChanged(int index) {
  // TODO: implement me.
}

void LauncherView::ButtonPressed(views::Button* sender,
                                 const views::Event& event) {
  ShellDelegate* delegate = Shell::GetInstance()->delegate();
  if (!delegate)
    return;
  if (sender == new_browser_button_) {
    delegate->CreateNewWindow();
  } else if (sender == show_apps_button_) {
    delegate->ShowApps();
  } else {
    int index = GetIndexOf(sender);
    DCHECK_NE(-1, index);
    // TODO: animations will require coordinate transforms.
    delegate->LauncherItemClicked(model_->items()[index - 1]);
  }
}

}  // namespace internal
}  // namespace aura_shell
