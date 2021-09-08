// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/autotest_desks_api.h"

#include "ash/shell.h"
#include "ash/wm/desks/desk.h"
#include "ash/wm/desks/desk_animation_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_histogram_enums.h"
#include "ash/wm/desks/root_window_desk_switch_animator.h"
#include "base/callback.h"
#include "base/check.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/layer_animation_sequence.h"
#include "ui/compositor/layer_animator.h"

namespace ash {

namespace {

class DeskAnimationObserver : public DesksController::Observer {
 public:
  DeskAnimationObserver(base::OnceClosure on_desk_animation_complete)
      : on_desk_animation_complete_(std::move(on_desk_animation_complete)) {
    DesksController::Get()->AddObserver(this);
  }

  ~DeskAnimationObserver() override {
    DesksController::Get()->RemoveObserver(this);
  }

  DeskAnimationObserver(const DeskAnimationObserver& other) = delete;
  DeskAnimationObserver& operator=(const DeskAnimationObserver& rhs) = delete;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {}
  void OnDeskRemoved(const Desk* desk) override {}
  void OnDeskReordered(int old_index, int new_index) override {}
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {}
  void OnDeskSwitchAnimationLaunching() override {}
  void OnDeskSwitchAnimationFinished() override {
    std::move(on_desk_animation_complete_).Run();
    delete this;
  }

 private:
  base::OnceClosure on_desk_animation_complete_;
};

// Self deleting desk animation observer which takes a target index and then
// waits until the animation layer has scheduled a new animation (ending
// screenshot will have been taken at this point). If the next desk is not the
// target desk, activate the adjacent desk (replacing the current animation). If
// the next desk is the target desk, wait until the animation is finished, then
// run the given callback.
class ChainedDeskAnimationObserver : public ui::LayerAnimationObserver,
                                     public DesksController::Observer {
 public:
  ChainedDeskAnimationObserver(bool going_left,
                               int target_index,
                               base::OnceClosure on_desk_animation_complete)
      : going_left_(going_left),
        target_index_(target_index),
        on_desk_animation_complete_(std::move(on_desk_animation_complete)) {
    DesksController::Get()->AddObserver(this);
  }
  ChainedDeskAnimationObserver(const ChainedDeskAnimationObserver& other) =
      delete;
  ChainedDeskAnimationObserver& operator=(
      const ChainedDeskAnimationObserver& rhs) = delete;
  ~ChainedDeskAnimationObserver() override {
    DesksController::Get()->RemoveObserver(this);
  }

  // ui::LayerAnimationObserver:
  void OnLayerAnimationEnded(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationAborted(ui::LayerAnimationSequence* sequence) override {}
  void OnLayerAnimationScheduled(
      ui::LayerAnimationSequence* sequence) override {
    if ((sequence->properties() & ui::LayerAnimationElement::TRANSFORM) == 0)
      return;

    auto* controller = DesksController::Get();
    const bool activated = controller->ActivateAdjacentDesk(
        going_left_, DesksSwitchSource::kDeskSwitchShortcut);
    DCHECK(activated);

    // If the animation goes to the last expected desk, remove the observer so
    // that the next scheduled animation does not try activating another desk.
    // We will then wait for the entire desk switch animation to finish and then
    // run |on_desk_animation_complete_|.
    if (controller->GetDeskIndex(controller->GetTargetActiveDesk()) ==
        target_index_) {
      animation_layer_->GetAnimator()->RemoveObserver(this);
    }
  }

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override {}
  void OnDeskRemoved(const Desk* desk) override {}
  void OnDeskReordered(int old_index, int new_index) override {}
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override {
    // The first activation changed happens when the initial ending screenshot
    // is being taken. This is the first point when the animation layer we want
    // to observe is guaranteed to be set as a child to the root window layer.
    if (animation_layer_)
      return;

    auto* animation = DesksController::Get()->animation();
    DCHECK(animation);
    animation_layer_ = animation->GetDeskSwitchAnimatorAtIndexForTesting(0)
                           ->GetAnimationLayerForTesting();
    animation_layer_->GetAnimator()->AddObserver(this);
  }
  void OnDeskSwitchAnimationLaunching() override {}
  void OnDeskSwitchAnimationFinished() override {
    std::move(on_desk_animation_complete_).Run();
    delete this;
  }

 private:
  const bool going_left_;
  const int target_index_;
  base::OnceClosure on_desk_animation_complete_;
  ui::Layer* animation_layer_ = nullptr;
};

}  // namespace

AutotestDesksApi::AutotestDesksApi() = default;

AutotestDesksApi::~AutotestDesksApi() = default;

bool AutotestDesksApi::CreateNewDesk() {
  if (!DesksController::Get()->CanCreateDesks())
    return false;

  DesksController::Get()->NewDesk(DesksCreationRemovalSource::kButton);
  return true;
}

bool AutotestDesksApi::ActivateDeskAtIndex(int index,
                                           base::OnceClosure on_complete) {
  DCHECK(!on_complete.is_null());

  if (index < 0)
    return false;

  auto* controller = DesksController::Get();
  if (index >= int{controller->desks().size()})
    return false;

  const Desk* target_desk = controller->desks()[index].get();
  if (target_desk == controller->active_desk())
    return false;

  new DeskAnimationObserver(std::move(on_complete));
  controller->ActivateDesk(target_desk, DesksSwitchSource::kMiniViewButton);
  return true;
}

bool AutotestDesksApi::RemoveActiveDesk(base::OnceClosure on_complete) {
  DCHECK(!on_complete.is_null());

  auto* controller = DesksController::Get();
  if (!controller->CanRemoveDesks())
    return false;

  new DeskAnimationObserver(std::move(on_complete));
  controller->RemoveDesk(controller->active_desk(),
                         DesksCreationRemovalSource::kButton);
  return true;
}

bool AutotestDesksApi::ActivateAdjacentDesksToTargetIndex(
    int index,
    base::OnceClosure on_complete) {
  DCHECK(!on_complete.is_null());

  if (index < 0)
    return false;

  auto* controller = DesksController::Get();
  if (index >= int{controller->desks().size()})
    return false;

  const Desk* target_desk = controller->desks()[index].get();
  if (target_desk == controller->active_desk())
    return false;

  int active_index = controller->GetDeskIndex(controller->active_desk());
  const bool going_left = index < active_index;
  new ChainedDeskAnimationObserver(going_left, index, std::move(on_complete));
  const bool activated = controller->ActivateAdjacentDesk(
      going_left, DesksSwitchSource::kDeskSwitchShortcut);
  DCHECK(activated);
  return true;
}

bool AutotestDesksApi::IsWindowInDesk(aura::Window* window, int desk_index) {
  aura::Window* desk_container = DesksController::Get()->GetDeskContainer(
      window->GetRootWindow(), desk_index);
  return desk_container->Contains(window);
}

}  // namespace ash
