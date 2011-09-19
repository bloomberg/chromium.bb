// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_BUBBLE_BUBBLE_VIEW_H_
#define VIEWS_BUBBLE_BUBBLE_VIEW_H_
#pragma once

#include <string>

#include "base/gtest_prod_util.h"
#include "base/task.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/animation/animation_delegate.h"
#include "views/accelerator.h"
#include "views/focus/focus_manager.h"
#include "views/view.h"
#include "views/window/client_view.h"

namespace ui {
class SlideAnimation;
}  // namespace ui

namespace views {

// To show a bubble:
// - Call Show() explicitly. This will not start a fading animation.
// - Call StartFade() this starts a fading out sequence that will be
//   cut short on VKEY_ESCAPE.
class VIEWS_EXPORT BubbleView : public ClientView,
                                public ui::AnimationDelegate {
 public:
  BubbleView(Widget* widget, View* contents_view);
  virtual ~BubbleView();

  // Starts a fade (out) animation. Unless this method is called, bubble will
  // stay until ui::VKEY_ESCAPE is sent.
  void StartFade();

  // Shows the bubble.
  void Show();

  void set_animation_delegate(ui::AnimationDelegate* delegate);

  virtual BubbleView* AsBubbleView() OVERRIDE;
  virtual const BubbleView* AsBubbleView() const OVERRIDE;

 protected:
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;

  virtual bool AcceleratorPressed(const Accelerator& accelerator) OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

 private:
  FRIEND_TEST(BubbleViewTest, FadeAnimation);

  void InitAnimation();

  // Sets up the layout manager based on currently initialized views. Should be
  // called when a view is initialized or changed.
  void ResetLayoutManager();

  // Close bubble when animation ended.
  virtual void AnimationEnded(const ui::Animation* animation);

  // notify on animation progress.
  virtual void AnimationProgressed(const ui::Animation* animation);

  // Fade animation for bubble.
  scoped_ptr<ui::SlideAnimation> fade_animation_;

  // Not owned.
  ui::AnimationDelegate* animation_delegate_;

  bool registered_accelerator_;

  bool should_fade_;

  DISALLOW_COPY_AND_ASSIGN(BubbleView);
};

}  // namespace views

#endif  // VIEWS_BUBBLE_BUBBLE_VIEW_H_
