// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ANIMATION_ANIMATION_DELEGATE_VIEWS_H_
#define UI_VIEWS_ANIMATION_ANIMATION_DELEGATE_VIEWS_H_

#include <memory>

#include "base/scoped_observer.h"
#include "ui/gfx/animation/animation_container_observer.h"
#include "ui/gfx/animation/animation_delegate.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"
#include "ui/views/views_export.h"

namespace ui {
class AnimationMetricsReporter;
}

namespace views {
class CompositorAnimationRunner;

// Provides default implementation to adapt CompositorAnimationRunner for
// Animation. Falls back to the default animation runner when |view| is nullptr.
class VIEWS_EXPORT AnimationDelegateViews
    : public gfx::AnimationDelegate,
      public ViewObserver,
      public gfx::AnimationContainerObserver {
 public:
  explicit AnimationDelegateViews(View* view);
  ~AnimationDelegateViews() override;

  // gfx::AnimationDelegate:
  void AnimationContainerWasSet(gfx::AnimationContainer* container) override;

  // ViewObserver:
  void OnViewAddedToWidget(View* observed_view) final;
  void OnViewRemovedFromWidget(View* observed_view) final;
  void OnViewIsDeleting(View* observed_view) final;

  // gfx::AnimationContainerObserver:
  void AnimationContainerProgressed(
      gfx::AnimationContainer* container) override {}
  void AnimationContainerEmpty(gfx::AnimationContainer* container) override {}
  void AnimationContainerShuttingDown(
      gfx::AnimationContainer* container) override;

  // Returns the expected animation duration for metrics reporting purposes.
  // Should be overriden to provide a non-zero value and used with
  // |set_animation_metrics_reporter()|.
  virtual base::TimeDelta GetAnimationDurationForReporting() const;

  void SetAnimationMetricsReporter(
      ui::AnimationMetricsReporter* animation_metrics_reporter);

  gfx::AnimationContainer* container() { return container_; }

 private:
  // Sets CompositorAnimationRunner to |container_| if possible. Otherwise,
  // clears AnimationRunner of |container_|.
  void UpdateAnimationRunner();

  View* view_;
  gfx::AnimationContainer* container_ = nullptr;

  ui::AnimationMetricsReporter* animation_metrics_reporter_ = nullptr;

  // The animation runner that |container_| uses.
  CompositorAnimationRunner* compositor_animation_runner_ = nullptr;

  ScopedObserver<View, ViewObserver> scoped_observer_{this};
};

}  // namespace views

#endif  // UI_VIEWS_ANIMATION_ANIMATION_DELEGATE_VIEWS_H_
