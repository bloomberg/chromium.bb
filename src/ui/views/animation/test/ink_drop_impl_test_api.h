// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_TEST_INK_DROP_IMPL_TEST_API_H_
#define UI_VIEWS_ANIMATION_TEST_INK_DROP_IMPL_TEST_API_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "ui/compositor/test/multi_layer_animator_test_controller.h"
#include "ui/compositor/test/multi_layer_animator_test_controller_delegate.h"
#include "ui/views/animation/ink_drop_impl.h"

namespace ui {
class LayerAnimator;
}  // namespace ui

namespace views {
class InkDropHighlight;

namespace test {

// Test API to provide internal access to an InkDropImpl instance. This can also
// be used to control the InkDropRipple and InkDropHighlight animations via the
// ui::test::MultiLayerAnimatorTestController API.
class InkDropImplTestApi
    : public ui::test::MultiLayerAnimatorTestController,
      public ui::test::MultiLayerAnimatorTestControllerDelegate {
 public:
  // Highlight state that access the HighlightStateFactory during the Exit()
  // method.
  class AccessFactoryOnExitHighlightState : public InkDropImpl::HighlightState {
   public:
    // Installs an instance of |this| to the ink drop that owns |state_factory|.
    static void Install(InkDropImpl::HighlightStateFactory* state_factory);

    explicit AccessFactoryOnExitHighlightState(
        InkDropImpl::HighlightStateFactory* state_factory);

    // HighlightState:
    void Exit() override;
    void ShowOnHoverChanged() override;
    void OnHoverChanged() override;
    void ShowOnFocusChanged() override;
    void OnFocusChanged() override;
    void AnimationStarted(InkDropState ink_drop_state) override;
    void AnimationEnded(InkDropState ink_drop_state,
                        InkDropAnimationEndedReason reason) override;

   private:
    DISALLOW_COPY_AND_ASSIGN(AccessFactoryOnExitHighlightState);
  };

  // Highlight state that attempts to set a new highlight state during Exit().
  class SetStateOnExitHighlightState : public InkDropImpl::HighlightState {
   public:
    // Installs an instance of |this| to the ink drop that owns |state_factory|.
    static void Install(InkDropImpl::HighlightStateFactory* state_factory);

    explicit SetStateOnExitHighlightState(
        InkDropImpl::HighlightStateFactory* state_factory);

    // HighlightState:
    void Exit() override;
    void ShowOnHoverChanged() override;
    void OnHoverChanged() override;
    void ShowOnFocusChanged() override;
    void OnFocusChanged() override;
    void AnimationStarted(InkDropState ink_drop_state) override;
    void AnimationEnded(InkDropState ink_drop_state,
                        InkDropAnimationEndedReason reason) override;

   private:
    DISALLOW_COPY_AND_ASSIGN(SetStateOnExitHighlightState);
  };

  explicit InkDropImplTestApi(InkDropImpl* ink_drop);
  ~InkDropImplTestApi() override;

  // Ensures that |ink_drop_|->ShouldHighlight() returns the same value as
  // |should_highlight| by updating the hover/focus status of |ink_drop_|.
  void SetShouldHighlight(bool should_highlight);

  // Wrappers to InkDropImpl internals:
  InkDropImpl::HighlightStateFactory* state_factory() {
    return ink_drop_->highlight_state_factory_.get();
  }

  void SetHighlightState(
      std::unique_ptr<InkDropImpl::HighlightState> highlight_state);

  const InkDropHighlight* highlight() const;
  bool IsHighlightFadingInOrVisible() const;
  bool ShouldHighlight() const;

 protected:
  // MultiLayerAnimatorTestControllerDelegate:
  std::vector<ui::LayerAnimator*> GetLayerAnimators() override;

 private:
  // The InkDrop to provide internal access to.
  InkDropImpl* ink_drop_;

  DISALLOW_COPY_AND_ASSIGN(InkDropImplTestApi);
};

}  // namespace test
}  // namespace views

#endif  // UI_VIEWS_ANIMATION_TEST_INK_DROP_IMPL_TEST_API_H_
