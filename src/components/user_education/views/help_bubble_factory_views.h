// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_VIEWS_H_
#define COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/user_education/common/help_bubble.h"
#include "components/user_education/common/help_bubble_factory.h"
#include "components/user_education/common/help_bubble_params.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/interaction/framework_specific_implementation.h"
#include "ui/color/color_id.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace user_education {

class HelpBubbleView;
class HelpBubbleDelegate;

// Views-specific implementation of the help bubble.
//
// Because this is a FrameworkSpecificImplementation, you can use:
//   help_bubble->AsA<HelpBubbleViews>()->bubble_view()
// to retrieve the underlying bubble view.
class HelpBubbleViews : public HelpBubble,
                        public views::WidgetObserver,
                        public ui::AcceleratorTarget {
 public:
  ~HelpBubbleViews() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // Retrieve the bubble view. If the bubble has been closed, this may return
  // null.
  HelpBubbleView* bubble_view() { return help_bubble_view_; }
  const HelpBubbleView* bubble_view() const { return help_bubble_view_; }

  // HelpBubble:
  bool ToggleFocusForAccessibility() override;
  void OnAnchorBoundsChanged() override;
  gfx::Rect GetBoundsInScreen() const override;
  ui::ElementContext GetContext() const override;

  // ui::AcceleratorTarget
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

 private:
  friend class HelpBubbleFactoryViews;
  friend class HelpBubbleFactoryMac;

  explicit HelpBubbleViews(HelpBubbleView* help_bubble_view);

  // Clean up properties on the anchor view, if applicable.
  void MaybeResetAnchorView();

  // HelpBubble:
  void CloseBubbleImpl() override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;

  raw_ptr<HelpBubbleView> help_bubble_view_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      scoped_observation_{this};
};

// Factory implementation for HelpBubbleViews.
class HelpBubbleFactoryViews : public HelpBubbleFactory {
 public:
  explicit HelpBubbleFactoryViews(const HelpBubbleDelegate* delegate);
  ~HelpBubbleFactoryViews() override;

  DECLARE_FRAMEWORK_SPECIFIC_METADATA()

  // HelpBubbleFactory:
  std::unique_ptr<HelpBubble> CreateBubble(ui::TrackedElement* element,
                                           HelpBubbleParams params) override;
  bool CanBuildBubbleForTrackedElement(
      const ui::TrackedElement* element) const override;

 private:
  base::raw_ptr<const HelpBubbleDelegate> delegate_;
};

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_VIEWS_HELP_BUBBLE_FACTORY_VIEWS_H_
