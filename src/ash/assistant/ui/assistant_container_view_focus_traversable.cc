// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/ui/assistant_container_view_focus_traversable.h"

#include "ash/assistant/ui/assistant_container_view.h"

namespace ash {

// AssistantContainerViewFocusSearch -------------------------------------------

AssistantContainerViewFocusSearch::AssistantContainerViewFocusSearch(
    AssistantContainerView* assistant_container_view)
    : views::FocusSearch(/*root=*/assistant_container_view,
                         /*cycle=*/true,
                         /*accessibility_mode=*/false),
      assistant_container_view_(assistant_container_view) {}

AssistantContainerViewFocusSearch::~AssistantContainerViewFocusSearch() =
    default;

views::View* AssistantContainerViewFocusSearch::FindNextFocusableView(
    views::View* starting_from,
    SearchDirection search_direction,
    TraversalDirection traversal_direction,
    StartingViewPolicy check_starting_view,
    AnchoredDialogPolicy can_go_into_anchored_dialog,
    views::FocusTraversable** focus_traversable,
    views::View** focus_traversable_view) {
  // NOTE: This FocusTraversable should only be used whenever the container does
  // not have anything currently focused *and* it has a reasonable default focus
  // override via |FindFirstFocusableView()|. This is ensured by
  // |AssistantContainerView::GetFocusTraversable()|.
  DCHECK(!assistant_container_view_->GetFocusManager() ||
         !assistant_container_view_->GetFocusManager()->GetFocusedView());

  auto* next_focusable_view =
      assistant_container_view_->FindFirstFocusableView();
  DCHECK(next_focusable_view);
  return next_focusable_view;
}

// AssistantContainerViewFocusTraversable --------------------------------------

AssistantContainerViewFocusTraversable::AssistantContainerViewFocusTraversable(
    AssistantContainerView* assistant_container_view)
    : assistant_container_view_(assistant_container_view),
      focus_search_(assistant_container_view) {}

AssistantContainerViewFocusTraversable::
    ~AssistantContainerViewFocusTraversable() = default;

views::FocusSearch* AssistantContainerViewFocusTraversable::GetFocusSearch() {
  return &focus_search_;
}

views::FocusTraversable*
AssistantContainerViewFocusTraversable::GetFocusTraversableParent() {
  return nullptr;
}

views::View*
AssistantContainerViewFocusTraversable::GetFocusTraversableParentView() {
  return assistant_container_view_;
}

}  // namespace ash
