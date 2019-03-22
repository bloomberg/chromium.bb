// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_QUIT_INSTRUCTION_BUBBLE_H_
#define CHROME_BROWSER_UI_VIEWS_QUIT_INSTRUCTION_BUBBLE_H_

#include <memory>

#include "base/macros.h"
#include "chrome/browser/ui/views/quit_instruction_bubble_base.h"
#include "ui/gfx/animation/animation_delegate.h"

namespace gfx {
class Animation;
class SlideAnimation;
}  // namespace gfx

namespace views {
class Widget;
}  // namespace views

// Manages showing and hiding a notification bubble that gives instructions for
// how to quit using keyboard shortcuts.
class QuitInstructionBubble : public QuitInstructionBubbleBase,
                              public gfx::AnimationDelegate {
 public:
  QuitInstructionBubble();
  ~QuitInstructionBubble() override;

  void Show() override;
  void Hide() override;

 private:
  // gfx::AnimationDelegate:
  void AnimationProgressed(const gfx::Animation* animation) override;
  void AnimationEnded(const gfx::Animation* animation) override;

  // Animation controlling showing/hiding of the bubble.
  std::unique_ptr<gfx::SlideAnimation> const animation_;

  std::unique_ptr<views::Widget> popup_;

  DISALLOW_COPY_AND_ASSIGN(QuitInstructionBubble);
};

#endif  // CHROME_BROWSER_UI_VIEWS_QUIT_INSTRUCTION_BUBBLE_H_
