// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/touchui/touch_editing_menu.h"

#include "base/utf_string_conversions.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/custom_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace {

const int kMenuCommands[] = {IDS_APP_CUT,
                             IDS_APP_COPY,
                             IDS_APP_PASTE,
                             IDS_APP_DELETE,
                             IDS_APP_SELECT_ALL};
const int kSpacingBetweenButtons = 0;
const int kButtonSeparatorColor = SkColorSetARGB(13, 0, 0, 0);
const int kMenuButtonBorderThickness = 5;
const SkColor kMenuButtonColorNormal = SkColorSetARGB(102, 255, 255, 255);
const SkColor kMenuButtonColorHover = SkColorSetARGB(13, 0, 0, 0);

const char* kEllipsesButtonText = "...";
const int kEllipsesButtonTag = -1;
}  // namespace

namespace views {

class TouchEditingMenuButtonBackground : public Background {
 public:
  TouchEditingMenuButtonBackground() {}

  virtual void Paint(gfx::Canvas* canvas, View* view) const OVERRIDE {
    CustomButton::ButtonState state = static_cast<CustomButton*>(view)->state();
    SkColor background_color = (state == CustomButton::STATE_NORMAL)?
        kMenuButtonColorNormal : kMenuButtonColorHover;
    int w = view->width();
    int h = view->height();
    canvas->FillRect(gfx::Rect(1, 0, w, h), background_color);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TouchEditingMenuButtonBackground);
};

TouchEditingMenuView::TouchEditingMenuView(
    TouchEditingMenuController* controller,
    gfx::Point anchor_point,
    gfx::NativeView context)
    : BubbleDelegateView(NULL, views::BubbleBorder::BOTTOM_CENTER),
      controller_(controller) {
  set_anchor_point(anchor_point);
  set_shadow(views::BubbleBorder::SMALL_SHADOW);
  set_parent_window(context);
  set_margins(gfx::Insets());
  set_use_focusless(true);
  set_adjust_if_offscreen(true);

  SetLayoutManager(new BoxLayout(BoxLayout::kHorizontal, 0, 0,
      kSpacingBetweenButtons));
  CreateButtons();
  views::BubbleDelegateView::CreateBubble(this);
  GetBubbleFrameView()->set_background(NULL);
  Show();
}

TouchEditingMenuView::~TouchEditingMenuView() {
}

void TouchEditingMenuView::Close() {
  if (GetWidget()) {
    controller_ = NULL;
    GetWidget()->Close();
  }
}

void TouchEditingMenuView::WindowClosing() {
  views::BubbleDelegateView::WindowClosing();
  if (controller_)
    controller_->OnMenuClosed(this);
}

void TouchEditingMenuView::ButtonPressed(Button* sender,
                                         const ui::Event& event) {
  if (controller_) {
    if (sender->tag() != kEllipsesButtonTag)
      controller_->ExecuteCommand(sender->tag());
    else
      controller_->OpenContextMenu();
  }
}

void TouchEditingMenuView::OnPaint(gfx::Canvas* canvas) {
  BubbleDelegateView::OnPaint(canvas);

  // Draw separator bars.
  int x = 0;
  for (int i = 0; i < child_count() - 1; ++i) {
    View* child = child_at(i);
    x += child->width();
    canvas->FillRect(gfx::Rect(x, 0, 1, child->height()),
        kButtonSeparatorColor);
  }
}

void TouchEditingMenuView::CreateButtons() {
  RemoveAllChildViews(true);
  for (size_t i = 0; i < arraysize(kMenuCommands); i++) {
    int command_id = kMenuCommands[i];
    if (controller_ && controller_->IsCommandIdEnabled(command_id)) {
      Button* button = CreateButton(l10n_util::GetStringUTF16(command_id),
          command_id);
      AddChildView(button);
    }
  }

  // Finally, add ellipses button.
  AddChildView(CreateButton(
      UTF8ToUTF16(kEllipsesButtonText), kEllipsesButtonTag));
  Layout();
}

Button* TouchEditingMenuView::CreateButton(const string16& title, int tag) {
  LabelButton* button = new LabelButton(this, gfx::RemoveAcceleratorChar(
      title, '&', NULL, NULL));
  button->set_focusable(true);
  button->set_request_focus_on_press(false);
  button->set_background(new TouchEditingMenuButtonBackground);
  button->set_border(Border::CreateEmptyBorder(kMenuButtonBorderThickness,
                                               kMenuButtonBorderThickness,
                                               kMenuButtonBorderThickness,
                                               kMenuButtonBorderThickness));
  button->SetFont(ui::ResourceBundle::GetSharedInstance().GetFont(
      ui::ResourceBundle::SmallFont));
  button->set_tag(tag);
  return button;
}

}  // namespace views
