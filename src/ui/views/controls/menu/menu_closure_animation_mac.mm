// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_closure_animation_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/animation/linear_animation.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/widget/widget.h"

namespace {
static bool g_disable_animations_for_testing = false;
}

namespace views {

MenuClosureAnimationMac::MenuClosureAnimationMac(MenuItemView* item,
                                                 SubmenuView* menu,
                                                 base::OnceClosure callback)
    : callback_(std::move(callback)),
      item_(item),
      menu_(menu),
      step_(AnimationStep::kStart) {}

MenuClosureAnimationMac::~MenuClosureAnimationMac() {}

void MenuClosureAnimationMac::Start() {
  DCHECK_EQ(step_, AnimationStep::kStart);
  if (g_disable_animations_for_testing) {
    // Even when disabling animations, simulate the fact that the eventual
    // accept callback will happen after a runloop cycle by skipping to the end
    // of the animation.
    step_ = AnimationStep::kFading;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(&MenuClosureAnimationMac::AdvanceAnimation,
                                  base::Unretained(this)));
    return;
  }
  AdvanceAnimation();
}

// static
MenuClosureAnimationMac::AnimationStep MenuClosureAnimationMac::NextStepFor(
    MenuClosureAnimationMac::AnimationStep step) const {
  switch (step) {
    case AnimationStep::kStart:
      return item_ ? AnimationStep::kUnselected : AnimationStep::kFading;
    case AnimationStep::kUnselected:
      return AnimationStep::kSelected;
    case AnimationStep::kSelected:
      return AnimationStep::kFading;
    case AnimationStep::kFading:
      return AnimationStep::kFinish;
    case AnimationStep::kFinish:
      return AnimationStep::kFinish;
  }
}

void MenuClosureAnimationMac::AdvanceAnimation() {
  step_ = NextStepFor(step_);
  if (step_ == AnimationStep::kUnselected ||
      step_ == AnimationStep::kSelected) {
    item_->SetForcedVisualSelection(step_ == AnimationStep::kSelected);
    timer_.Start(FROM_HERE, base::TimeDelta::FromMilliseconds(80),
                 base::BindRepeating(&MenuClosureAnimationMac::AdvanceAnimation,
                                     base::Unretained(this)));
  } else if (step_ == AnimationStep::kFading) {
    auto fade = std::make_unique<gfx::LinearAnimation>(this);
    fade->SetDuration(base::TimeDelta::FromMilliseconds(200));
    fade_animation_ = std::move(fade);
    fade_animation_->Start();
  } else if (step_ == AnimationStep::kFinish) {
    std::move(callback_).Run();
  }
}

// static
void MenuClosureAnimationMac::DisableAnimationsForTesting() {
  g_disable_animations_for_testing = true;
}

void MenuClosureAnimationMac::AnimationProgressed(
    const gfx::Animation* animation) {
  NSWindow* window = menu_->GetWidget()->GetNativeWindow();
  [window setAlphaValue:animation->CurrentValueBetween(1.0, 0.0)];
}

void MenuClosureAnimationMac::AnimationEnded(const gfx::Animation* animation) {
  AdvanceAnimation();
}

void MenuClosureAnimationMac::AnimationCanceled(
    const gfx::Animation* animation) {
  NOTREACHED();
}

}  // namespace views
