// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view_md.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/bounded_label.h"
#include "ui/message_center/views/constants.h"
#include "ui/message_center/views/message_center_controller.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/native_cursor.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"

namespace message_center {

namespace {

// Dimensions.
constexpr gfx::Insets kContentRowPadding(4, 12, 12, 12);
constexpr gfx::Insets kActionsRowPadding(8, 8, 8, 8);
constexpr int kActionsRowHorizontalSpacing = 8;
constexpr gfx::Insets kImageContainerPadding(0, 12, 12, 12);

// Foreground of small icon image.
constexpr SkColor kSmallImageBackgroundColor = SK_ColorWHITE;
// Background of small icon image.
const SkColor kSmallImageColor = SkColorSetRGB(0x43, 0x43, 0x43);
// Background of inline actions area.
const SkColor kActionsRowBackgroundColor = SkColorSetRGB(0xee, 0xee, 0xee);

// Max number of lines for message_view_.
constexpr int kMaxLinesForMessageView = 1;
constexpr int kMaxLinesForExpandedMessageView = 4;

const gfx::ImageSkia CreateSolidColorImage(int width,
                                           int height,
                                           SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

// Take the alpha channel of icon, mask it with the foreground,
// then add the masked foreground on top of the background
const gfx::ImageSkia GetMaskedIcon(const gfx::ImageSkia& icon) {
  int width = icon.width();
  int height = icon.height();

  // Background color grey
  const gfx::ImageSkia background = CreateSolidColorImage(
      width, height, message_center::kSmallImageBackgroundColor);
  // Foreground color white
  const gfx::ImageSkia foreground =
      CreateSolidColorImage(width, height, message_center::kSmallImageColor);
  const gfx::ImageSkia masked_small_image =
      gfx::ImageSkiaOperations::CreateMaskedImage(foreground, icon);
  return gfx::ImageSkiaOperations::CreateSuperimposedImage(background,
                                                           masked_small_image);
}

const gfx::ImageSkia GetProductIcon() {
  return gfx::CreateVectorIcon(kProductIcon, kSmallImageColor);
}

}  // anonymous namespace

// ////////////////////////////////////////////////////////////
// NotificationViewMD
// ////////////////////////////////////////////////////////////

views::View* NotificationViewMD::TargetForRect(views::View* root,
                                               const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  // TODO(tdanderson): Modify this function to support rect-based event
  // targeting. Using the center point of |rect| preserves this function's
  // expected behavior for the time being.
  gfx::Point point = rect.CenterPoint();

  // Want to return this for underlying views, otherwise GetCursor is not
  // called. But buttons are exceptions, they'll have their own event handlings.
  std::vector<views::View*> buttons(action_buttons_.begin(),
                                    action_buttons_.end());
  if (header_row_->settings_button())
    buttons.push_back(header_row_->settings_button());
  if (header_row_->close_button())
    buttons.push_back(header_row_->close_button());
  if (header_row_->expand_button())
    buttons.push_back(header_row_->expand_button());
  buttons.push_back(header_row_);

  for (size_t i = 0; i < buttons.size(); ++i) {
    gfx::Point point_in_child = point;
    ConvertPointToTarget(this, buttons[i], &point_in_child);
    if (buttons[i]->HitTestPoint(point_in_child))
      return buttons[i]->GetEventHandlerForPoint(point_in_child);
  }

  return root;
}

void NotificationViewMD::CreateOrUpdateViews(const Notification& notification) {
  CreateOrUpdateContextTitleView(notification);
  CreateOrUpdateTitleView(notification);
  CreateOrUpdateMessageView(notification);
  CreateOrUpdateProgressBarView(notification);
  CreateOrUpdateListItemViews(notification);
  CreateOrUpdateIconView(notification);
  CreateOrUpdateSmallIconView(notification);
  CreateOrUpdateImageView(notification);
  CreateOrUpdateActionButtonViews(notification);
  CreateOrUpdateCloseButtonView(notification);
  CreateOrUpdateSettingsButtonView(notification);
  UpdateViewForExpandedState(expanded_);
}

NotificationViewMD::NotificationViewMD(MessageCenterController* controller,
                                       const Notification& notification)
    : MessageView(controller, notification),
      clickable_(notification.clickable()) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(), 0));

  // |header_row_| contains app_icon, app_name, control buttons, etc...
  header_row_ = new NotificationHeaderView(this);
  AddChildView(header_row_);

  // |content_row_| contains title, message, image, progressbar, etc...
  content_row_ = new views::View();
  views::BoxLayout* content_row_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, kContentRowPadding, 0);
  content_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  content_row_->SetLayoutManager(content_row_layout);
  AddChildView(content_row_);

  // |left_content_| contains most contents like title, message, etc...
  left_content_ = new views::View();
  left_content_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(), 0));
  content_row_layout->SetFlexForView(left_content_, 1);
  content_row_->AddChildView(left_content_);

  // |right_content_| contains notification icon and small image.
  right_content_ = new views::View();
  right_content_->SetLayoutManager(new views::FillLayout());
  content_row_->AddChildView(right_content_);

  // |action_row_| contains inline action button.
  actions_row_ = new views::View();
  actions_row_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, kActionsRowPadding,
                           kActionsRowHorizontalSpacing));
  actions_row_->SetBackground(
      views::CreateSolidBackground(kActionsRowBackgroundColor));
  actions_row_->SetVisible(false);
  AddChildView(actions_row_);

  CreateOrUpdateViews(notification);

  SetEventTargeter(
      std::unique_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
}

NotificationViewMD::~NotificationViewMD() {}

void NotificationViewMD::Layout() {
  MessageView::Layout();

  // We need to call IsExpandable() at the end of Layout() call, since whether
  // we should show expand button or not depends on the current view layout.
  // (e.g. Show expand button when |message_view_| exceeds one line.)
  header_row_->SetExpandButtonEnabled(IsExpandable());
}

void NotificationViewMD::OnFocus() {
  MessageView::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

void NotificationViewMD::ScrollRectToVisible(const gfx::Rect& rect) {
  // Notification want to show the whole notification when a part of it (like
  // a button) gets focused.
  views::View::ScrollRectToVisible(GetLocalBounds());
}

gfx::NativeCursor NotificationViewMD::GetCursor(const ui::MouseEvent& event) {
  if (!clickable_ || !controller()->HasClickedListener(notification_id()))
    return views::View::GetCursor(event);

  return views::GetNativeHandCursor();
}

void NotificationViewMD::OnMouseMoved(const ui::MouseEvent& event) {
  MessageView::OnMouseMoved(event);
  UpdateControlButtonsVisibility();
}

void NotificationViewMD::OnMouseEntered(const ui::MouseEvent& event) {
  MessageView::OnMouseEntered(event);
  UpdateControlButtonsVisibility();
}

void NotificationViewMD::OnMouseExited(const ui::MouseEvent& event) {
  MessageView::OnMouseExited(event);
  UpdateControlButtonsVisibility();
}

void NotificationViewMD::UpdateWithNotification(
    const Notification& notification) {
  MessageView::UpdateWithNotification(notification);

  CreateOrUpdateViews(notification);
  Layout();
  SchedulePaint();
}

void NotificationViewMD::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  // Certain operations can cause |this| to be destructed, so copy the members
  // we send to other parts of the code.
  // TODO(dewittj): Remove this hack.
  std::string id(notification_id());

  if (header_row_->IsCloseButtonEnabled() &&
      sender == header_row_->close_button()) {
    // Warning: This causes the NotificationViewMD itself to be deleted, so
    // don't do anything afterwards.
    OnCloseButtonPressed();
    return;
  }

  if (header_row_->IsSettingsButtonEnabled() &&
      sender == header_row_->settings_button()) {
    controller()->ClickOnSettingsButton(id);
    return;
  }

  // Tapping anywhere on |header_row_| can expand the notification, though only
  // |expand_button| can be focused by TAB.
  if (IsExpandable() &&
      (sender == header_row_ || sender == header_row_->expand_button())) {
    ToggleExpanded();
    Layout();
    SchedulePaint();
    return;
  }

  // See if the button pressed was an action button.
  for (size_t i = 0; i < action_buttons_.size(); ++i) {
    if (sender == action_buttons_[i]) {
      controller()->ClickOnNotificationButton(id, i);
      return;
    }
  }
}

bool NotificationViewMD::IsCloseButtonFocused() const {
  if (!header_row_->IsCloseButtonEnabled())
    return false;

  const views::FocusManager* focus_manager = GetFocusManager();
  return focus_manager &&
         focus_manager->GetFocusedView() == header_row_->close_button();
}

void NotificationViewMD::RequestFocusOnCloseButton() {
  if (header_row_->IsCloseButtonEnabled())
    header_row_->close_button()->RequestFocus();
}

void NotificationViewMD::CreateOrUpdateContextTitleView(
    const Notification& notification) {
  header_row_->SetAppName(notification.display_source());
}

void NotificationViewMD::CreateOrUpdateTitleView(
    const Notification& notification) {
  const gfx::FontList& font_list = views::Label().font_list().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);

  int title_character_limit =
      kNotificationWidth * kMaxTitleLines / kMinPixelsPerTitleCharacter;

  base::string16 title = gfx::TruncateString(
      notification.title(), title_character_limit, gfx::WORD_BREAK);
  if (!title_view_) {
    title_view_ = new views::Label(title);
    title_view_->SetFontList(font_list);
    title_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_view_->SetEnabledColor(message_center::kRegularTextColor);
    left_content_->AddChildView(title_view_);
  } else {
    title_view_->SetText(title);
  }
}

void NotificationViewMD::CreateOrUpdateMessageView(
    const Notification& notification) {
  if (notification.message().empty()) {
    // Deletion will also remove |context_message_view_| from its parent.
    delete message_view_;
    message_view_ = nullptr;
    return;
  }

  base::string16 text = gfx::TruncateString(
      notification.message(), kMessageCharacterLimit, gfx::WORD_BREAK);

  const gfx::FontList& font_list = views::Label().font_list().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);

  if (!message_view_) {
    message_view_ = new BoundedLabel(text, font_list);
    message_view_->SetLineLimit(kMaxLinesForMessageView);
    message_view_->SetColors(message_center::kDimTextColor,
                             kContextTextBackgroundColor);
    left_content_->AddChildView(message_view_);
  } else {
    message_view_->SetText(text);
  }
}

void NotificationViewMD::CreateOrUpdateProgressBarView(
    const Notification& notification) {
  // TODO(yoshiki): Implement this.
}

void NotificationViewMD::CreateOrUpdateListItemViews(
    const Notification& notification) {
  // TODO(yoshiki): Implement this.
}

void NotificationViewMD::CreateOrUpdateIconView(
    const Notification& notification) {
  gfx::Size image_view_size(30, 30);
  if (!icon_view_) {
    icon_view_ = new ProportionalImageView(image_view_size);
    right_content_->AddChildView(icon_view_);
  }

  gfx::ImageSkia icon = notification.icon().AsImageSkia();
  icon_view_->SetImage(icon, icon.size());
}

void NotificationViewMD::CreateOrUpdateSmallIconView(
    const Notification& notification) {
  gfx::ImageSkia icon =
      notification.small_image().IsEmpty()
          ? GetProductIcon()
          : GetMaskedIcon(notification.small_image().AsImageSkia());
  header_row_->SetAppIcon(icon);
}

void NotificationViewMD::CreateOrUpdateImageView(
    const Notification& notification) {
  // |image_view_| is the view representing the area covered by the
  // notification's image, including background and border.  Its size can be
  // specified in advance and images will be scaled to fit including a border if
  // necessary.
  if (notification.image().IsEmpty()) {
    left_content_->RemoveChildView(image_container_);
    image_container_ = NULL;
    image_view_ = NULL;
    return;
  }

  gfx::Size ideal_size(kNotificationPreferredImageWidth,
                       kNotificationPreferredImageHeight);

  if (!image_container_) {
    image_container_ = new views::View();
    image_container_->SetLayoutManager(new views::FillLayout());
    image_container_->SetBorder(
        views::CreateEmptyBorder(kImageContainerPadding));
    image_container_->SetBackground(
        views::CreateSolidBackground(message_center::kImageBackgroundColor));

    image_view_ = new message_center::ProportionalImageView(ideal_size);
    image_container_->AddChildView(image_view_);
    // Insert the created image container just after the |content_row_|.
    AddChildViewAt(image_container_, GetIndexOf(content_row_) + 1);
  }

  DCHECK(image_view_);
  image_view_->SetImage(notification.image().AsImageSkia(), ideal_size);
}

void NotificationViewMD::CreateOrUpdateActionButtonViews(
    const Notification& notification) {
  std::vector<ButtonInfo> buttons = notification.buttons();
  bool new_buttons = action_buttons_.size() != buttons.size();

  if (new_buttons || buttons.size() == 0) {
    for (auto* item : action_buttons_)
      delete item;
    action_buttons_.clear();
  }

  DCHECK_EQ(this, actions_row_->parent());

  for (size_t i = 0; i < buttons.size(); ++i) {
    ButtonInfo button_info = buttons[i];
    if (new_buttons) {
      views::LabelButton* button = new views::LabelButton(
          this, button_info.title, views::style::CONTEXT_BUTTON_MD);
      button->SetFocusForPlatform();
      action_buttons_.push_back(button);
      actions_row_->AddChildView(button);
    } else {
      action_buttons_[i]->SetText(button_info.title);
      action_buttons_[i]->SchedulePaint();
      action_buttons_[i]->Layout();
    }
  }

  if (new_buttons) {
    // TODO(fukino): Investigate if this Layout() is necessary.
    Layout();
    views::Widget* widget = GetWidget();
    if (widget != NULL) {
      widget->SetSize(widget->GetContentsView()->GetPreferredSize());
      GetWidget()->SynthesizeMouseMoveEvent();
    }
  }
}

void NotificationViewMD::CreateOrUpdateCloseButtonView(
    const Notification& notification) {
  if (!notification.pinned()) {
    header_row_->SetCloseButtonEnabled(true);
  } else {
    header_row_->SetCloseButtonEnabled(false);
  }
}

void NotificationViewMD::CreateOrUpdateSettingsButtonView(
    const Notification& notification) {
  if (notification.delegate() &&
      notification.delegate()->ShouldDisplaySettingsButton())
    header_row_->SetSettingsButtonEnabled(true);
  else
    header_row_->SetSettingsButtonEnabled(false);
}

bool NotificationViewMD::IsExpandable() {
  // Expandable if the message exceeds one line.
  if (message_view_ && message_view_->visible() &&
      message_view_->GetLinesForWidthAndLimit(message_view_->width(), -1) > 1) {
    return true;
  }
  // Expandable if there is at least one inline action.
  if (actions_row_->has_children())
    return true;

  // Expandable if the notification has image.
  if (image_view_)
    return true;

  // TODO(fukino): Expandable if both progress bar and message exist.

  return false;
}

void NotificationViewMD::ToggleExpanded() {
  expanded_ = !expanded_;
  UpdateViewForExpandedState(expanded_);
  content_row_->InvalidateLayout();
  if (controller())
    controller()->UpdateNotificationSize(notification_id());
}

void NotificationViewMD::UpdateViewForExpandedState(bool expanded) {
  header_row_->SetExpanded(expanded);
  if (message_view_) {
    message_view_->SetLineLimit(expanded ? kMaxLinesForExpandedMessageView
                                         : kMaxLinesForMessageView);
  }
  if (image_container_)
    image_container_->SetVisible(expanded);
  actions_row_->SetVisible(expanded && actions_row_->has_children());
}

void NotificationViewMD::UpdateControlButtonsVisibility() {
  const bool target_visibility = IsMouseHovered() || HasFocus() ||
                                 (header_row_->IsExpandButtonEnabled() &&
                                  header_row_->expand_button()->HasFocus()) ||
                                 (header_row_->IsCloseButtonEnabled() &&
                                  header_row_->close_button()->HasFocus()) ||
                                 (header_row_->IsSettingsButtonEnabled() &&
                                  header_row_->settings_button()->HasFocus());

  header_row_->SetControlButtonsVisible(target_visibility);
}

}  // namespace message_center
