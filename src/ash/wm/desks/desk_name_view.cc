// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_name_view.h"

#include <memory>

#include "ash/shell.h"
#include "ash/style/ash_color_provider.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_bar_view.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/text_constants.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/accessibility_paint_checks.h"
#include "ui/views/background.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/native_cursor.h"
#include "ui/views/widget/widget.h"

namespace ash {

constexpr int kDeskNameViewBorderRadius = 4;
constexpr int kDeskNameViewMinHeight = 24;
constexpr int kDeskNameViewHorizontalPadding = 6;

namespace {

bool IsDesksBarWidget(const views::Widget* widget) {
  if (!widget)
    return false;

  auto* overview_controller = Shell::Get()->overview_controller();
  if (!overview_controller->InOverviewSession())
    return false;

  auto* session = overview_controller->overview_session();
  for (const auto& grid : session->grid_list()) {
    if (widget == grid->desks_widget())
      return true;
  }

  return false;
}

}  // namespace

DeskNameView::DeskNameView(DeskMiniView* mini_view) : mini_view_(mini_view) {
  auto border = std::make_unique<WmHighlightItemBorder>(
      /*corner_radius=*/4, gfx::Insets(0, kDeskNameViewHorizontalPadding));
  border_ptr_ = border.get();
  SetBorder(std::move(border));

  SetCursorEnabled(true);
  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

  // TODO(crbug.com/1218186): Remove this, this is in place temporarily to be
  // able to submit accessibility checks, but this focusable View needs to
  // add a name so that the screen reader knows what to announce.
  SetProperty(views::kSkipAccessibilityPaintChecks, true);
}

DeskNameView::~DeskNameView() = default;

// static
constexpr size_t DeskNameView::kMaxLength;

// static
void DeskNameView::CommitChanges(views::Widget* widget) {
  DCHECK(IsDesksBarWidget(widget));

  auto* focus_manager = widget->GetFocusManager();
  focus_manager->ClearFocus();
  // Avoid having the focus restored to the same DeskNameView when the desks bar
  // widget is refocused, e.g. when the new desk button is pressed.
  focus_manager->SetStoredFocusView(nullptr);
}

void DeskNameView::SetTextAndElideIfNeeded(const std::u16string& text) {
  // Use the potential max size of this to calculate elision, not its current
  // size to avoid eliding names that don't need to be.
  SetText(
      gfx::ElideText(text, GetFontList(),
                     parent()->GetPreferredSize().width() - GetInsets().width(),
                     gfx::ELIDE_TAIL));
  full_text_ = text;
}

void DeskNameView::UpdateViewAppearance() {
  background()->SetNativeControlColor(GetBackgroundColor());
  UpdateBorderState();
}

const char* DeskNameView::GetClassName() const {
  return "DeskNameView";
}

gfx::Size DeskNameView::CalculatePreferredSize() const {
  const auto& text = GetText();
  int width = 0;
  int height = 0;
  gfx::Canvas::SizeStringInt(text, GetFontList(), &width, &height, 0,
                             gfx::Canvas::NO_ELLIPSIS);
  gfx::Size size{width + GetCaretBounds().width(), height};
  const auto insets = GetInsets();
  size.Enlarge(insets.width(), insets.height());
  size.SetToMax(gfx::Size(0, kDeskNameViewMinHeight));
  return size;
}

bool DeskNameView::SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) {
  // The default behavior of the tab key is that it moves the focus to the next
  // available DeskNameView.
  // We want that to be handled by OverviewHighlightController as part of moving
  // the highlight forward or backward when tab or shift+tab are pressed.
  if (event.key_code() == ui::VKEY_TAB)
    return true;

  return false;
}

void DeskNameView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  Textfield::GetAccessibleNodeData(node_data);
  // When Bento is enabled and the user creates a new desk, |full_text_| will be
  // empty but the accesssible name for |this| will be the default desk name.
  node_data->SetName(full_text_.empty() ? GetAccessibleName() : full_text_);
}

void DeskNameView::OnMouseEntered(const ui::MouseEvent& event) {
  UpdateViewAppearance();
}

void DeskNameView::OnMouseExited(const ui::MouseEvent& event) {
  UpdateViewAppearance();
}

void DeskNameView::OnThemeChanged() {
  Textfield::OnThemeChanged();
  SetBackground(views::CreateRoundedRectBackground(GetBackgroundColor(),
                                                   kDeskNameViewBorderRadius));
  AshColorProvider* color_provider = AshColorProvider::Get();
  const SkColor text_color = color_provider->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary);
  SetTextColor(text_color);
  SetSelectionTextColor(text_color);

  const SkColor selection_color = color_provider->GetControlsLayerColor(
      AshColorProvider::ControlsLayerType::kFocusAuraColor);
  SetSelectionBackgroundColor(selection_color);
  UpdateBorderState();
}

gfx::NativeCursor DeskNameView::GetCursor(const ui::MouseEvent& event) {
  return views::GetNativeIBeamCursor();
}

views::View* DeskNameView::GetView() {
  return this;
}

void DeskNameView::MaybeActivateHighlightedView() {
  RequestFocus();
}

void DeskNameView::MaybeCloseHighlightedView() {}

void DeskNameView::MaybeSwapHighlightedView(bool right) {}

void DeskNameView::OnViewHighlighted() {
  UpdateBorderState();
  mini_view_->owner_bar()->ScrollToShowMiniViewIfNecessary(mini_view_);
}

void DeskNameView::OnViewUnhighlighted() {
  UpdateBorderState();
}

void DeskNameView::UpdateBorderState() {
  border_ptr_->SetFocused(IsViewHighlighted() || HasFocus());
  SchedulePaint();
}

SkColor DeskNameView::GetBackgroundColor() const {
  return HasFocus() || IsMouseHovered()
             ? AshColorProvider::Get()->GetControlsLayerColor(
                   AshColorProvider::ControlsLayerType::
                       kControlBackgroundColorInactive)
             : SK_ColorTRANSPARENT;
}

}  // namespace ash
