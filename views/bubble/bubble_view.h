// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_BUBBLE_BUBBLE_VIEW_H_
#define VIEWS_BUBBLE_BUBBLE_VIEW_H_
#pragma once

#include "ui/base/animation/animation_delegate.h"
#include "views/accelerator.h"
#include "views/window/client_view.h"

namespace ui {
class SlideAnimation;
}  // namespace ui

namespace views {

class VIEWS_EXPORT BubbleView : public ClientView,
                                public ui::AnimationDelegate {
 public:
  BubbleView(Widget* widget, View* contents_view);
  virtual ~BubbleView();

  // ClientView overrides:
  virtual BubbleView* AsBubbleView() OVERRIDE;
  virtual const BubbleView* AsBubbleView() const OVERRIDE;

  void set_close_on_esc(bool close_on_esc) { close_on_esc_ = close_on_esc; }

  // Starts a fade animation, fade out closes the widget upon completion.
  void StartFade(bool fade_in);

 protected:
  // View overrides:
  virtual bool AcceleratorPressed(const Accelerator& accelerator) OVERRIDE;

 private:
  // ui::AnimationDelegate overrides:
  virtual void AnimationEnded(const ui::Animation* animation);
  virtual void AnimationProgressed(const ui::Animation* animation);

  // Fade animation for bubble.
  scoped_ptr<ui::SlideAnimation> fade_animation_;

  // Should this bubble close on the escape key?
  bool close_on_esc_;

  DISALLOW_COPY_AND_ASSIGN(BubbleView);
};

}  // namespace views

#endif  // VIEWS_BUBBLE_BUBBLE_VIEW_H_
