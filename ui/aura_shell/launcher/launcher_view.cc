// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura_shell/launcher/launcher_view.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/aura_shell/launcher/app_launcher_button.h"
#include "ui/aura_shell/launcher/launcher_model.h"
#include "ui/aura_shell/launcher/tabbed_launcher_button.h"
#include "ui/aura_shell/launcher/view_model.h"
#include "ui/aura_shell/launcher/view_model_utils.h"
#include "ui/aura_shell/shell.h"
#include "ui/aura_shell/shell_delegate.h"
#include "ui/base/animation/animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/image/image.h"
#include "views/animation/bounds_animator.h"
#include "views/controls/button/image_button.h"
#include "views/widget/widget.h"

using ui::Animation;
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

// Minimum distance before drag starts.
static const int kMinimumDragDistance = 8;

namespace {

// AnimationDelegate that deletes a view when done. This is used when a launcher
// item is removed, which triggers a remove animation. When the animation is
// done we delete the view.
class DeleteViewAnimationDelegate :
      public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  explicit DeleteViewAnimationDelegate(views::View* view) : view_(view) {}
  virtual ~DeleteViewAnimationDelegate() {}

 private:
  scoped_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(DeleteViewAnimationDelegate);
};

// AnimationDelegate used when inserting a new item. This steadily increases the
// opacity of the layer as the animation progress.
class FadeInAnimationDelegate :
      public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  explicit FadeInAnimationDelegate(views::View* view) : view_(view) {}
  virtual ~FadeInAnimationDelegate() {}

  // AnimationDelegate overrides:
  virtual void AnimationProgressed(const Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  virtual void AnimationEnded(const Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }
  virtual void AnimationCanceled(const Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1.0f);
    view_->layer()->ScheduleDraw();
  }

 private:
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(FadeInAnimationDelegate);
};

}  // namespace

// AnimationDelegate used when inserting a new item. This steadily decreased the
// opacity of the layer as the animation progress.
class LauncherView::FadeOutAnimationDelegate :
      public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  FadeOutAnimationDelegate(LauncherView* host, views::View* view)
      : launcher_view_(host),
        view_(view) {}
  virtual ~FadeOutAnimationDelegate() {}

  // AnimationDelegate overrides:
  virtual void AnimationProgressed(const Animation* animation) OVERRIDE {
    view_->layer()->SetOpacity(1 - animation->GetCurrentValue());
    view_->layer()->ScheduleDraw();
  }
  virtual void AnimationEnded(const Animation* animation) OVERRIDE {
    launcher_view_->AnimateToIdealBounds();
  }
  virtual void AnimationCanceled(const Animation* animation) OVERRIDE {
  }

 private:
  LauncherView* launcher_view_;
  scoped_ptr<views::View> view_;

  DISALLOW_COPY_AND_ASSIGN(FadeOutAnimationDelegate);
};

// AnimationDelegate used to trigger fading an element in. When an item is
// inserted this delegate is attached to the animation that expands the size of
// the item.  When done it kicks off another animation to fade the item in.
class LauncherView::StartFadeAnimationDelegate :
      public views::BoundsAnimator::OwnedAnimationDelegate {
 public:
  StartFadeAnimationDelegate(LauncherView* host,
                             views::View* view)
      : launcher_view_(host),
        view_(view) {}
  virtual ~StartFadeAnimationDelegate() {}

  // AnimationDelegate overrides:
  virtual void AnimationEnded(const Animation* animation) OVERRIDE {
    view_->SetVisible(true);
    launcher_view_->FadeIn(view_);
  }
  virtual void AnimationCanceled(const Animation* animation) OVERRIDE {
    view_->SetVisible(true);
  }

 private:
  LauncherView* launcher_view_;
  views::View* view_;

  DISALLOW_COPY_AND_ASSIGN(StartFadeAnimationDelegate);
};

LauncherView::LauncherView(LauncherModel* model)
    : model_(model),
      view_model_(new ViewModel),
      new_browser_button_(NULL),
      show_apps_button_(NULL),
      dragging_(NULL),
      drag_view_(NULL),
      drag_offset_(0),
      start_drag_index_(-1) {
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
    child->SetPaintToLayer(true);
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
  ViewModelUtils::SetViewBoundsToIdealBounds(*view_model_);
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
  views::View* view = NULL;
  if (item.type == TYPE_TABBED) {
    TabbedLauncherButton* button = new TabbedLauncherButton(this, this);
    button->SetImages(item.tab_images);
    view = button;
  } else {
    DCHECK_EQ(TYPE_APP, item.type);
    AppLauncherButton* button = new AppLauncherButton(this, this);
    button->SetAppImage(item.app_image);
    view = button;
  }
  view->SetPaintToLayer(true);
  return view;
}

void LauncherView::FadeIn(views::View* view) {
  view->SetVisible(true);
  view->layer()->SetOpacity(0);
  AnimateToIdealBounds();
  bounds_animator_->SetAnimationDelegate(
      view, new FadeInAnimationDelegate(view), true);
}

void LauncherView::PrepareForDrag(const views::MouseEvent& event) {
  DCHECK(drag_view_);
  dragging_ = true;
  start_drag_index_ = view_model_->GetIndexOfView(drag_view_);
  // Move the view to the front so that it appears on top of other views.
  ReorderChildView(drag_view_, -1);
  bounds_animator_->StopAnimatingView(drag_view_);
}

void LauncherView::ContinueDrag(const views::MouseEvent& event) {
  // TODO: I don't think this works correctly with RTL.
  gfx::Point drag_point(event.x(), 0);
  views::View::ConvertPointToView(drag_view_, this, &drag_point);
  int current_index = view_model_->GetIndexOfView(drag_view_);
  DCHECK_NE(-1, current_index);

  // Constrain the x location so that it doesn't overlap the two buttons.
  int x = std::max(view_model_->ideal_bounds(0).x(),
                   drag_point.x() - drag_offset_);
  x = std::min(view_model_->ideal_bounds(view_model_->view_size() - 1).right() -
               view_model_->ideal_bounds(current_index).width(),
               x);
  if (drag_view_->x() == x)
    return;

  drag_view_->SetX(x);
  int target_index =
      ViewModelUtils::DetermineMoveIndex(*view_model_, drag_view_, x);
  if (target_index == current_index)
    return;

  // Remove the observer while we mutate the model so that we don't attempt to
  // cancel the drag.
  model_->RemoveObserver(this);
  model_->Move(current_index, target_index);
  model_->AddObserver(this);
  view_model_->Move(current_index, target_index);
  AnimateToIdealBounds();
  bounds_animator_->StopAnimatingView(drag_view_);
}

void LauncherView::CancelDrag(views::View* deleted_view) {
  if (!drag_view_)
    return;
  bool was_dragging = dragging_;
  views::View* drag_view = drag_view_;
  dragging_ = false;
  drag_view_ = NULL;
  if (drag_view == deleted_view) {
    // The view that was being dragged is being deleted. Don't do anything.
    return;
  }
  if (!was_dragging)
    return;

  view_model_->Move(view_model_->GetIndexOfView(drag_view), start_drag_index_);
  AnimateToIdealBounds();
}

gfx::Size LauncherView::GetPreferredSize() {
  IdealBounds ideal_bounds;
  CalculateIdealBounds(&ideal_bounds);
  return gfx::Size(ideal_bounds.show_apps_bounds.right() + kLeadingInset,
                   kPreferredHeight);
}

void LauncherView::LauncherItemAdded(int model_index) {
  CancelDrag(NULL);

  views::View* view = CreateViewForItem(model_->items()[model_index]);
  AddChildView(view);
  // Hide the view, it'll be made visible when the animation is done.
  view->SetVisible(false);
  view_model_->Add(view, model_index);

  // The first animation moves all the views to their target position. |view| is
  // hidden, so it visually appears as though we are providing space for
  // it. When done we'll fade the view in.
  AnimateToIdealBounds();
  bounds_animator_->SetAnimationDelegate(
      view, new StartFadeAnimationDelegate(this, view), true);
}

void LauncherView::LauncherItemRemoved(int model_index) {
  views::View* view = view_model_->view_at(model_index);
  CancelDrag(view);
  view_model_->Remove(model_index);
  // The first animation fades out the view. When done we'll animate the rest of
  // the views to their target location.
  bounds_animator_->AnimateViewTo(view, view->bounds());
  bounds_animator_->SetAnimationDelegate(
      view, new FadeOutAnimationDelegate(this, view), true);
}

void LauncherView::LauncherItemImagesChanged(int model_index) {
  const LauncherItem& item(model_->items()[model_index]);
  views::View* view = view_model_->view_at(model_index);
  if (item.type == TYPE_TABBED) {
    TabbedLauncherButton* button = static_cast<TabbedLauncherButton*>(view);
    gfx::Size pref = button->GetPreferredSize();
    button->SetImages(item.tab_images);
    if (pref != button->GetPreferredSize())
      AnimateToIdealBounds();
    else
      button->SchedulePaint();
  } else {
    DCHECK_EQ(TYPE_APP, item.type);
    AppLauncherButton* button = static_cast<AppLauncherButton*>(view);
    button->SetAppImage(item.app_image);
    button->SchedulePaint();
  }
}

void LauncherView::LauncherItemMoved(int start_index, int target_index) {
  view_model_->Move(start_index, target_index);
  AnimateToIdealBounds();
}

void LauncherView::MousePressedOnButton(views::View* view,
                                        const views::MouseEvent& event) {
  if (view_model_->GetIndexOfView(view) == -1 || view_model_->view_size() <= 1)
    return;  // View is being deleted, ignore request.

  drag_view_ = view;
  drag_offset_ = event.x();
}

void LauncherView::MouseDraggedOnButton(views::View* view,
                                        const views::MouseEvent& event) {
  if (!dragging_ && drag_view_ &&
      abs(event.x() - drag_offset_) >= kMinimumDragDistance)
    PrepareForDrag(event);
  if (dragging_)
    ContinueDrag(event);
}

void LauncherView::MouseReleasedOnButton(views::View* view,
                                         bool canceled) {
  if (canceled) {
    CancelDrag(NULL);
  } else {
    dragging_ = false;
    drag_view_ = NULL;
    AnimateToIdealBounds();
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
