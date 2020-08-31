// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_
#define ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_

#include <deque>
#include <memory>
#include <utility>
#include <vector>

#include "ash/assistant/ui/main_stage/animated_container_view.h"
#include "base/component_export.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "ui/views/view_observer.h"

namespace ash {

class AssistantUiElementViewFactory;
class AssistantViewDelegate;

// UiElementContainerView is the child of AssistantMainView concerned with
// laying out Assistant UI element views in response to Assistant interaction
// model events.
class COMPONENT_EXPORT(ASSISTANT_UI) UiElementContainerView
    : public AnimatedContainerView {
 public:
  explicit UiElementContainerView(AssistantViewDelegate* delegate);
  ~UiElementContainerView() override;

  // AnimatedContainerView:
  const char* GetClassName() const override;
  gfx::Size CalculatePreferredSize() const override;
  int GetHeightForWidth(int width) const override;
  gfx::Size GetMinimumSize() const override;
  void Layout() override;
  void OnContentsPreferredSizeChanged(views::View* content_view) override;
  void OnCommittedQueryChanged(const AssistantQuery& query) override;

 private:
  void InitLayout();

  // AnimatedContainerView:
  std::unique_ptr<ElementAnimator> HandleUiElement(
      const AssistantUiElement* ui_element) override;
  void OnAllViewsAnimatedIn() override;
  void OnScrollBarUpdated(views::ScrollBar* scroll_bar,
                          int viewport_size,
                          int content_size,
                          int content_scroll_offset) override;
  void OnScrollBarVisibilityChanged(views::ScrollBar* scroll_bar,
                                    bool is_visible) override;

  void UpdateScrollIndicator(bool can_scroll);

  views::View* scroll_indicator_ = nullptr;  // Owned by view hierarchy.

  // Factory instance used to construct views for modeled UI elements.
  std::unique_ptr<AssistantUiElementViewFactory> view_factory_;

  DISALLOW_COPY_AND_ASSIGN(UiElementContainerView);
};

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_MAIN_STAGE_UI_ELEMENT_CONTAINER_VIEW_H_
