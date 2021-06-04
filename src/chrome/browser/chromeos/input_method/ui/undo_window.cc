// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/ui/undo_window.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/chromeos/input_method/ui/border_factory.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/wm/core/window_animations.h"

namespace ui {
namespace ime {

namespace {
const char16_t kUndoButtonText[] = u"Undo";
// TODO(crbug/1099044): Update and use cros_colors.json5
constexpr SkColor kButtonHighlightColor =
    SkColorSetA(SK_ColorBLACK, 0x0F);  // 6% Black.

}  // namespace

UndoWindow::UndoWindow(gfx::NativeView parent, AssistiveDelegate* delegate)
    : delegate_(delegate) {
  DialogDelegate::SetButtons(ui::DIALOG_BUTTON_NONE);
  SetCanActivate(false);
  DCHECK(parent);
  set_parent_window(parent);
  set_margins(gfx::Insets());

  SetArrow(views::BubbleBorder::Arrow::BOTTOM_LEFT);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  undo_button_ = AddChildView(std::make_unique<views::LabelButton>(
      base::BindRepeating(&UndoWindow::UndoButtonPressed,
                          base::Unretained(this)),
      kUndoButtonText));
  undo_button_->SetImageLabelSpacing(
      views::LayoutProvider::Get()->GetDistanceMetric(
          views::DistanceMetric::DISTANCE_RELATED_BUTTON_HORIZONTAL));
  undo_button_->SetBackground(nullptr);
  undo_button_->SetFocusBehavior(views::View::FocusBehavior::ACCESSIBLE_ONLY);
}

void UndoWindow::OnThemeChanged() {
  undo_button_->SetImage(
      views::Button::ButtonState::STATE_NORMAL,
      gfx::CreateVectorIcon(kAutocorrectUndoIcon,
                            GetNativeTheme()->GetSystemColor(
                                ui::NativeTheme::kColorId_DefaultIconColor)));
  BubbleDialogDelegateView::OnThemeChanged();
}

UndoWindow::~UndoWindow() = default;

views::Widget* UndoWindow::InitWidget() {
  views::Widget* widget = BubbleDialogDelegateView::CreateBubble(this);

  wm::SetWindowVisibilityAnimationTransition(widget->GetNativeView(),
                                             wm::ANIMATE_NONE);

  GetBubbleFrameView()->SetBubbleBorder(
      GetBorderForWindow(WindowBorderType::Undo));
  GetBubbleFrameView()->OnThemeChanged();
  return widget;
}

void UndoWindow::Hide() {
  GetWidget()->Close();
}

void UndoWindow::Show() {
  GetWidget()->Show();
}

void UndoWindow::SetBounds(const gfx::Rect& word_bounds) {
  SetAnchorRect(word_bounds);
}

void UndoWindow::SetButtonHighlighted(const AssistiveWindowButton& button,
                                      bool highlighted) {
  if (button.id != ButtonId::kUndo)
    return;

  bool currently_hightlighted = undo_button_->background() != nullptr;
  if (highlighted == currently_hightlighted)
    return;

  undo_button_->SetBackground(
      highlighted ? views::CreateSolidBackground(kButtonHighlightColor)
                  : nullptr);
}

views::Button* UndoWindow::GetUndoButtonForTesting() {
  return undo_button_;
}

void UndoWindow::UndoButtonPressed() {
  const AssistiveWindowButton button = {
      .id = ButtonId::kUndo, .window_type = AssistiveWindowType::kUndoWindow};
  SetButtonHighlighted(button, true);
  delegate_->AssistiveWindowButtonClicked(button);
}

BEGIN_METADATA(UndoWindow, views::BubbleDialogDelegateView)
END_METADATA

}  // namespace ime
}  // namespace ui
