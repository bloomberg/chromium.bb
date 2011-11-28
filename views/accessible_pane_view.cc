// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/accessible_pane_view.h"

#include "base/message_loop.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/widget/widget.h"

namespace views {

AccessiblePaneView::AccessiblePaneView()
    : pane_has_focus_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      focus_manager_(NULL),
      home_key_(ui::VKEY_HOME, false, false, false),
      end_key_(ui::VKEY_END, false, false, false),
      escape_key_(ui::VKEY_ESCAPE, false, false, false),
      left_key_(ui::VKEY_LEFT, false, false, false),
      right_key_(ui::VKEY_RIGHT, false, false, false) {
  focus_search_.reset(new views::FocusSearch(this, true, true));
}

AccessiblePaneView::~AccessiblePaneView() {
  if (pane_has_focus_) {
    focus_manager_->RemoveFocusChangeListener(this);
  }
}

bool AccessiblePaneView::SetPaneFocus(views::View* initial_focus) {
  if (!IsVisible())
    return false;

  if (!focus_manager_)
    focus_manager_ = GetFocusManager();

  focus_manager_->StoreFocusedView();

  // Use the provided initial focus if it's visible and enabled, otherwise
  // use the first focusable child.
  if (!initial_focus ||
      !Contains(initial_focus) ||
      !initial_focus->IsVisible() ||
      !initial_focus->IsEnabled()) {
    initial_focus = GetFirstFocusableChild();
  }

  // Return false if there are no focusable children.
  if (!initial_focus)
    return false;

  focus_manager_->SetFocusedView(initial_focus);

  // If we already have pane focus, we're done.
  if (pane_has_focus_)
    return true;

  // Otherwise, set accelerators and start listening for focus change events.
  pane_has_focus_ = true;
  focus_manager_->RegisterAccelerator(home_key_, this);
  focus_manager_->RegisterAccelerator(end_key_, this);
  focus_manager_->RegisterAccelerator(escape_key_, this);
  focus_manager_->RegisterAccelerator(left_key_, this);
  focus_manager_->RegisterAccelerator(right_key_, this);
  focus_manager_->AddFocusChangeListener(this);

  return true;
}

bool AccessiblePaneView::SetPaneFocusAndFocusDefault() {
  return SetPaneFocus(GetDefaultFocusableChild());
}

views::View* AccessiblePaneView::GetDefaultFocusableChild() {
  return NULL;
}

void AccessiblePaneView::RemovePaneFocus() {
  focus_manager_->RemoveFocusChangeListener(this);
  pane_has_focus_ = false;

  focus_manager_->UnregisterAccelerator(home_key_, this);
  focus_manager_->UnregisterAccelerator(end_key_, this);
  focus_manager_->UnregisterAccelerator(escape_key_, this);
  focus_manager_->UnregisterAccelerator(left_key_, this);
  focus_manager_->UnregisterAccelerator(right_key_, this);
}

views::View* AccessiblePaneView::GetFirstFocusableChild() {
  FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return focus_search_->FindNextFocusableView(
      NULL, false, views::FocusSearch::DOWN, false,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

views::View* AccessiblePaneView::GetLastFocusableChild() {
  FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return focus_search_->FindNextFocusableView(
      this, true, views::FocusSearch::DOWN, false,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

////////////////////////////////////////////////////////////////////////////////
// View overrides:

views::FocusTraversable* AccessiblePaneView::GetPaneFocusTraversable() {
  if (pane_has_focus_)
    return this;
  else
    return NULL;
}

bool AccessiblePaneView::AcceleratorPressed(
    const ui::Accelerator& accelerator) {

  const views::View* focused_view = focus_manager_->GetFocusedView();
  if (!Contains(focused_view))
    return false;

  switch (accelerator.key_code()) {
    case ui::VKEY_ESCAPE:
      RemovePaneFocus();
      focus_manager_->RestoreFocusedView();
      return true;
    case ui::VKEY_LEFT:
      focus_manager_->AdvanceFocus(true);
      return true;
    case ui::VKEY_RIGHT:
      focus_manager_->AdvanceFocus(false);
      return true;
    case ui::VKEY_HOME:
      focus_manager_->SetFocusedViewWithReason(
          GetFirstFocusableChild(), views::FocusManager::kReasonFocusTraversal);
      return true;
    case ui::VKEY_END:
      focus_manager_->SetFocusedViewWithReason(
          GetLastFocusableChild(), views::FocusManager::kReasonFocusTraversal);
      return true;
    default:
      return false;
  }
}

void AccessiblePaneView::SetVisible(bool flag) {
  if (IsVisible() && !flag && pane_has_focus_) {
    RemovePaneFocus();
    focus_manager_->RestoreFocusedView();
  }
  View::SetVisible(flag);
}

void AccessiblePaneView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PANE;
}

////////////////////////////////////////////////////////////////////////////////
// FocusChangeListener overrides:

void AccessiblePaneView::OnWillChangeFocus(views::View* focused_before,
                                           views::View* focused_now) {
  //  Act when focus has changed.
}

void AccessiblePaneView::OnDidChangeFocus(views::View* focused_before,
                                          views::View* focused_now) {
  if (!focused_now)
    return;

  views::FocusManager::FocusChangeReason reason =
      focus_manager_->focus_change_reason();

  if (!Contains(focused_now) ||
      reason == views::FocusManager::kReasonDirectFocusChange) {
    // We should remove pane focus (i.e. make most of the controls
    // not focusable again) because the focus has left the pane,
    // or because the focus changed within the pane due to the user
    // directly focusing to a specific view (e.g., clicking on it).
    RemovePaneFocus();
  }
}

////////////////////////////////////////////////////////////////////////////////
// FocusTraversable overrides:

views::FocusSearch* AccessiblePaneView::GetFocusSearch() {
  DCHECK(pane_has_focus_);
  return focus_search_.get();
}

views::FocusTraversable* AccessiblePaneView::GetFocusTraversableParent() {
  DCHECK(pane_has_focus_);
  return NULL;
}

views::View* AccessiblePaneView::GetFocusTraversableParentView() {
  DCHECK(pane_has_focus_);
  return NULL;
}

}  // namespace views
