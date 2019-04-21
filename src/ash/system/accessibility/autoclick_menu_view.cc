// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/autoclick_menu_view.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/top_shortcut_button.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/vector_icon_types.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Constants for panel sizing, positioning and coloring.
const int kPanelPositionButtonSize = 36;
const int kPanelPositionButtonPadding = 14;
const int kSeparatorHeight = 16;
const SkColor kSeparatorColor = SkColorSetARGB(0x1A, 255, 255, 255);
const SkColor kAutoclickMenuButtonColorActive = SkColorSetRGB(138, 180, 248);
const SkColor kAutoclickMenuButtonIconColorActive = SkColorSetRGB(32, 33, 36);

}  // namespace

// A button used in the Automatic clicks on-screen menu. The button is
// togglable.
class AutoclickMenuButton : public TopShortcutButton {
 public:
  AutoclickMenuButton(views::ButtonListener* listener,
                      const gfx::VectorIcon& icon,
                      int accessible_name_id,
                      int size = kTrayItemSize)
      : TopShortcutButton(listener, accessible_name_id),
        icon_(&icon),
        size_(size) {
    SetPreferredSize(gfx::Size(size_, size_));
    UpdateImage();
  }

  ~AutoclickMenuButton() override = default;

  // Set the vector icon shown in a circle.
  void SetVectorIcon(const gfx::VectorIcon& icon) {
    icon_ = &icon;
    UpdateImage();
  }

  // Change the toggle state.
  void SetToggled(bool toggled) {
    toggled_ = toggled;
    UpdateImage();
    SchedulePaint();
  }

  bool IsToggled() { return toggled_; }

  // TopShortcutButton:
  void PaintButtonContents(gfx::Canvas* canvas) override {
    gfx::Rect rect(GetContentsBounds());
    cc::PaintFlags flags;
    flags.setAntiAlias(true);
    flags.setColor(toggled_ ? kAutoclickMenuButtonColorActive
                            : kUnifiedMenuButtonColor);
    flags.setStyle(cc::PaintFlags::kFill_Style);
    canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), size_ / 2, flags);

    views::ImageButton::PaintButtonContents(canvas);
  }

  void GetAccessibleNodeData(ui::AXNodeData* node_data) override {
    if (!enabled())
      return;
    TopShortcutButton::GetAccessibleNodeData(node_data);
    node_data->role = ax::mojom::Role::kToggleButton;
    node_data->SetCheckedState(toggled_ ? ax::mojom::CheckedState::kTrue
                                        : ax::mojom::CheckedState::kFalse);
  }

  void SetId(AutoclickMenuView::ButtonId id) {
    views::View::set_id(static_cast<int>(id));
  }

 private:
  void UpdateImage() {
    SetImage(views::Button::STATE_NORMAL,
             gfx::CreateVectorIcon(
                 *icon_, toggled_ ? kAutoclickMenuButtonIconColorActive
                                  : kUnifiedMenuIconColor));
    SetImage(views::Button::STATE_DISABLED,
             gfx::CreateVectorIcon(
                 *icon_, toggled_ ? kAutoclickMenuButtonIconColorActive
                                  : kUnifiedMenuIconColor));
  }

  const gfx::VectorIcon* icon_;
  // True if the button is currently toggled.
  bool toggled_ = false;
  int size_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickMenuButton);
};

// ------ AutoclickMenuBubbleView  ------ //

AutoclickMenuBubbleView::AutoclickMenuBubbleView(
    TrayBubbleView::InitParams init_params)
    : TrayBubbleView(init_params) {}

AutoclickMenuBubbleView::~AutoclickMenuBubbleView() {}

bool AutoclickMenuBubbleView::IsAnchoredToStatusArea() const {
  return false;
}

void AutoclickMenuBubbleView::MoveToPosition(const gfx::Rect& rect) {
  // TODO(katie): Animate to the new position.
  SetAnchorRect(rect);
}

// ------ AutoclickMenuView  ------ //

AutoclickMenuView::AutoclickMenuView(mojom::AutoclickEventType type,
                                     mojom::AutoclickMenuPosition position)
    : left_click_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickLeftClickIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_LEFT_CLICK)),
      right_click_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickRightClickIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_RIGHT_CLICK)),
      double_click_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickDoubleClickIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_DOUBLE_CLICK)),
      drag_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickDragIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_DRAG_AND_DROP)),
      pause_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickPauseIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_NO_ACTION)),
      position_button_(
          new AutoclickMenuButton(this,
                                  kAutoclickPositionBottomLeftIcon,
                                  IDS_ASH_AUTOCLICK_OPTION_CHANGE_POSITION,
                                  kPanelPositionButtonSize)) {
  // Set view IDs for testing.
  left_click_button_->SetId(ButtonId::kLeftClick);
  right_click_button_->SetId(ButtonId::kRightClick);
  double_click_button_->SetId(ButtonId::kDoubleClick);
  drag_button_->SetId(ButtonId::kDragAndDrop);
  pause_button_->SetId(ButtonId::kPause);
  position_button_->SetId(ButtonId::kPosition);

  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(), 0);
  layout->set_cross_axis_alignment(views::BoxLayout::CROSS_AXIS_ALIGNMENT_END);
  SetLayoutManager(std::move(layout));

  // The action control buttons all have the same spacing.
  views::View* action_button_container = new views::View();
  action_button_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, kUnifiedMenuItemPadding,
      kUnifiedTopShortcutSpacing));
  action_button_container->AddChildView(left_click_button_);
  action_button_container->AddChildView(right_click_button_);
  action_button_container->AddChildView(double_click_button_);
  action_button_container->AddChildView(drag_button_);
  action_button_container->AddChildView(pause_button_);
  AddChildView(action_button_container);

  views::Separator* separator = new views::Separator();
  separator->SetColor(kSeparatorColor);
  separator->SetPreferredHeight(kSeparatorHeight);
  int total_height = kUnifiedTopShortcutSpacing * 2 + kTrayItemSize;
  int separator_spacing = (total_height - kSeparatorHeight) / 2;
  separator->SetBorder(views::CreateEmptyBorder(
      separator_spacing - kUnifiedTopShortcutSpacing, 0, separator_spacing, 0));
  AddChildView(separator);

  views::View* position_button_container = new views::View();
  position_button_container->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal,
          gfx::Insets(0, kPanelPositionButtonPadding,
                      kPanelPositionButtonPadding, kPanelPositionButtonPadding),
          kPanelPositionButtonPadding));
  position_button_container->AddChildView(position_button_);
  AddChildView(position_button_container);

  UpdateEventType(type);
  UpdatePosition(position);
}

void AutoclickMenuView::UpdateEventType(mojom::AutoclickEventType type) {
  left_click_button_->SetToggled(type == mojom::AutoclickEventType::kLeftClick);
  right_click_button_->SetToggled(type ==
                                  mojom::AutoclickEventType::kRightClick);
  double_click_button_->SetToggled(type ==
                                   mojom::AutoclickEventType::kDoubleClick);
  drag_button_->SetToggled(type == mojom::AutoclickEventType::kDragAndDrop);
  pause_button_->SetToggled(type == mojom::AutoclickEventType::kNoAction);
  if (type != mojom::AutoclickEventType::kNoAction)
    event_type_ = type;
}

void AutoclickMenuView::UpdatePosition(mojom::AutoclickMenuPosition position) {
  switch (position) {
    case mojom::AutoclickMenuPosition::kBottomRight:
      position_button_->SetVectorIcon(kAutoclickPositionBottomRightIcon);
      return;
    case mojom::AutoclickMenuPosition::kBottomLeft:
      position_button_->SetVectorIcon(kAutoclickPositionBottomLeftIcon);
      return;
    case mojom::AutoclickMenuPosition::kTopLeft:
      position_button_->SetVectorIcon(kAutoclickPositionTopLeftIcon);
      return;
    case mojom::AutoclickMenuPosition::kTopRight:
      position_button_->SetVectorIcon(kAutoclickPositionTopRightIcon);
      return;
  }
}

void AutoclickMenuView::ButtonPressed(views::Button* sender,
                                      const ui::Event& event) {
  if (sender == position_button_) {
    mojom::AutoclickMenuPosition new_position;
    // Rotate clockwise throughout the screen positions.
    switch (
        Shell::Get()->accessibility_controller()->GetAutoclickMenuPosition()) {
      case mojom::AutoclickMenuPosition::kBottomRight:
        new_position = mojom::AutoclickMenuPosition::kBottomLeft;
        break;
      case mojom::AutoclickMenuPosition::kBottomLeft:
        new_position = mojom::AutoclickMenuPosition::kTopLeft;
        break;
      case mojom::AutoclickMenuPosition::kTopLeft:
        new_position = mojom::AutoclickMenuPosition::kTopRight;
        break;
      case mojom::AutoclickMenuPosition::kTopRight:
        new_position = mojom::AutoclickMenuPosition::kBottomRight;
        break;
    }
    Shell::Get()->accessibility_controller()->SetAutoclickMenuPosition(
        new_position);
    base::RecordAction(base::UserMetricsAction(
        "Accessibility.CrosAutoclick.TrayMenu.ChangePosition"));
    return;
  }
  mojom::AutoclickEventType type;
  if (sender == left_click_button_) {
    type = mojom::AutoclickEventType::kLeftClick;
  } else if (sender == right_click_button_) {
    type = mojom::AutoclickEventType::kRightClick;
  } else if (sender == double_click_button_) {
    type = mojom::AutoclickEventType::kDoubleClick;
  } else if (sender == drag_button_) {
    type = mojom::AutoclickEventType::kDragAndDrop;
  } else if (sender == pause_button_) {
    // If the pause button was already selected, tapping it again turns off
    // pause and returns to the previous type.
    if (pause_button_->IsToggled())
      type = event_type_;
    else
      type = mojom::AutoclickEventType::kNoAction;
  } else {
    return;
  }

  Shell::Get()->accessibility_controller()->SetAutoclickEventType(type);
  UMA_HISTOGRAM_ENUMERATION("Accessibility.CrosAutoclick.TrayMenu.ChangeAction",
                            type);
}

}  // namespace ash
