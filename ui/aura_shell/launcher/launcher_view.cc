// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher_view.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/aura_shell/launcher/launcher_model.h"
#include "ui/aura_shell/launcher/tabbed_launcher_button.h"
#include "ui/aura_shell/launcher/view_model.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "views/animation/bounds_animator.h"
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

namespace {

// AnimationDelegate that deletes a view when done. This is used when a launcher
// item is removed, which triggers a remove animation. When the animation is
// done we delete the view.
class DeleteViewAnimationDelegate :
      public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  DeleteViewAnimationDelegate(views::View* view) : view_(view) {}
  virtual ~DeleteViewAnimationDelegate() {}

 private:
  scoped_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(DeleteViewAnimationDelegate);
};

}  // namespace

LauncherView::LauncherView(LauncherModel* model)
    : model_(model),
      view_model_(new ViewModel),
      new_browser_button_(NULL),
      show_apps_button_(NULL) {
  DCHECK(model_);
  bounds_animator_.reset(new views::BoundsAnimator(this));
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
  for (LauncherItems::const_iterator i = items.begin(); i != items.end(); ++i) {
    views::View* child = CreateViewForItem(*i);
    view_model_->Add(child, static_cast<int>(i - items.begin()));
    AddChildView(child);
  }

  show_apps_button_ = new views::ImageButton(this);
  show_apps_button_->SetImage(
      views::CustomButton::BS_NORMAL,
      rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_APPLIST).ToSkBitmap());
  AddChildView(show_apps_button_);

  LayoutToIdealBounds();
}

void LauncherView::LayoutToIdealBounds() {
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);
  new_browser_button_->SetBoundsRect(ideal_bounds.new_browser_bounds);
  show_apps_button_->SetBoundsRect(ideal_bounds.show_apps_bounds);
  view_model_->SetViewBoundsToIdealBounds();
}

void LauncherView::CalculateIdealBounds(IdealBounds* bounds) {
  // new_browser_button first.
  int x = kLeadingInset;
  gfx::Size pref = new_browser_button_->GetPreferredSize();
  bounds->new_browser_bounds = gfx::Rect(
      x, (kPreferredHeight - pref.height()) / 2, pref.width(), pref.height());
  x += bounds->new_browser_bounds.width() + kHorizontalPadding;

  // Then launcher buttons.
  for (int i = 0; i < view_model_->view_size(); ++i) {
    pref = view_model_->view_at(i)->GetPreferredSize();
    view_model_->set_ideal_bounds(i, gfx::Rect(
        x, (kPreferredHeight - pref.height()) / 2, pref.width(),
        pref.height()));
    x += pref.width() + kHorizontalPadding;
  }

  // And the show_apps_button.
  pref = show_apps_button_->GetPreferredSize();
  // TODO(sky): -8 is a hack, remove when we get better images.
  bounds->show_apps_bounds = gfx::Rect(
      x - 8, (kPreferredHeight - pref.width()) / 2, pref.width(),
      pref.height());
}

void LauncherView::AnimateToIdealBounds() {
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);
  bounds_animator_->AnimateViewTo(new_browser_button_,
                                  ideal_bounds.new_browser_bounds);
  for (int i = 0; i < view_model_->view_size(); ++i) {
    bounds_animator_->AnimateViewTo(view_model_->view_at(i),
                                    view_model_->ideal_bounds(i));
  }
  bounds_animator_->AnimateViewTo(show_apps_button_,
                                  ideal_bounds.show_apps_bounds);
}

views::View* LauncherView::CreateViewForItem(const LauncherItem& item) {
  if (item.type == TYPE_TABBED) {
    TabbedLauncherButton* button = new TabbedLauncherButton(this);
    button->SetImages(item.tab_images);
    return button;
  }
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  const SkBitmap* image =
      rb.GetImageNamed(IDR_AURA_LAUNCHER_ICON_CHROME).ToSkBitmap();
  views::ImageButton* button = new views::ImageButton(this);
  button->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                            views::ImageButton::ALIGN_MIDDLE);
  button->SetPreferredSize(gfx::Size(image->width(), image->height()));
  button->SetImage(views::CustomButton::BS_NORMAL, &item.app_image);
  return button;
}

void LauncherView::Resize() {
  // TODO: we may want to force the width to a specific size.
  int y = GetWidget()->GetClientAreaScreenBounds().y();
  gfx::Size pref(GetPreferredSize());
  GetWidget()->SetBounds(gfx::Rect(0, y, pref.width(), pref.height()));
  Layout();
}

gfx::Size LauncherView::GetPreferredSize() {
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);
  return gfx::Size(ideal_bounds.show_apps_bounds.right() + kLeadingInset,
                   kPreferredHeight);
}

void LauncherView::LauncherItemAdded(int model_index) {
  views::View* view = CreateViewForItem(model_->items()[model_index]);
  AddChildView(view);
  view_model_->Add(view, model_index);

  // Update the bounds and reset the bounds of the newly created view to 0 width
  // so that it appears to animate open.
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);
  gfx::Rect bounds = view_model_->ideal_bounds(model_index);
  bounds.set_width(0);
  view->SetBoundsRect(bounds);

  // Resize and animate all the views.
  Resize();
  AnimateToIdealBounds();
}

void LauncherView::LauncherItemRemoved(int model_index) {
  views::View* view = view_model_->view_at(model_index);
  view_model_->Remove(model_index);
  Resize();
  AnimateToIdealBounds();
  gfx::Rect target_bounds = view->bounds();
  target_bounds.set_width(0);
  bounds_animator_->AnimateViewTo(view, target_bounds);
  bounds_animator_->SetAnimationDelegate(
      view, new DeleteViewAnimationDelegate(view), true);
}

void LauncherView::LauncherItemImagesChanged(int model_index) {
  const LauncherItem& item(model_->items()[model_index]);
  views::View* view = view_model_->view_at(model_index);
  if (item.type == TYPE_TABBED) {
    TabbedLauncherButton* button = static_cast<TabbedLauncherButton*>(view);
    gfx::Size pref = button->GetPreferredSize();
    button->SetImages(item.tab_images);
    if (pref != button->GetPreferredSize()) {
      Resize();
      AnimateToIdealBounds();
    } else {
      button->SchedulePaint();
    }
  } else {
    DCHECK_EQ(TYPE_APP, item.type);
    views::ImageButton* button = static_cast<views::ImageButton*>(view);
    button->SetImage(views::CustomButton::BS_NORMAL, &item.app_image);
    button->SchedulePaint();
  }
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
    int view_index = view_model_->GetIndexOfView(sender);
    // May be -1 while in the process of animating closed.
    if (view_index != -1)
      delegate->LauncherItemClicked(model_->items()[view_index]);
  }
}

}  // namespace internal
}  // namespace aura_shell
