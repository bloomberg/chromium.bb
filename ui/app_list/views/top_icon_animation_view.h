// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_APP_LIST_VIEWS_TOP_ICON_ANIMATION_VIEW_H_
#define UI_APP_LIST_VIEWS_TOP_ICON_ANIMATION_VIEW_H_

#include "base/observer_list.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/view.h"

namespace views {
class ImageView;
}

namespace app_list {

// Observer for top icon animation completion.
class TopIconAnimationObserver {
 public:
  // Called when top icon animation completes.
  virtual void OnTopIconAnimationsComplete() {}

 protected:
  TopIconAnimationObserver() {}
  virtual ~TopIconAnimationObserver() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TopIconAnimationObserver);
};

// Transitional view used for top item icons animation when opening or closing
// a folder. Owns itself.
class TopIconAnimationView : public views::View,
                             public ui::ImplicitAnimationObserver {
 public:
  // |icon|: The icon image of the item icon of full scale size.
  // |scaled_rect|: Bounds of the small icon inside folder icon.
  // |open_folder|: Specify open/close folder animation to perform.
  // The view will be self-cleaned by the end of animation.
  TopIconAnimationView(const gfx::ImageSkia& icon,
                       const gfx::Rect& scaled_rect,
                       bool open_folder);
  virtual ~TopIconAnimationView();

  void AddObserver(TopIconAnimationObserver* observer);
  void RemoveObserver(TopIconAnimationObserver* observer);

  // When opening a folder, transform the top item icon from the small icon
  // inside folder icon to the full scale icon at the target location.
  // When closing a folder, transform the full scale item icon from its
  // location to the small icon inside the folder icon.
  void TransformView();

 private:
  // views::View overrides:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual void Layout() OVERRIDE;

  // ui::ImplicitAnimationObserver overrides:
  virtual void OnImplicitAnimationsCompleted() OVERRIDE;
  virtual bool RequiresNotificationWhenAnimatorDestroyed() const OVERRIDE;

  gfx::Size icon_size_;
  views::ImageView* icon_;  // Owned by views hierarchy.
  // Rect of the scaled down top item icon inside folder icon's ink bubble.
  gfx::Rect scaled_rect_;
  // true: opening folder; false: closing folder.
  bool open_folder_;

  ObserverList<TopIconAnimationObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(TopIconAnimationView);
};

}  // namespace app_list

#endif  // UI_APP_LIST_VIEWS_TOP_ICON_ANIMATION_VIEW_H_
