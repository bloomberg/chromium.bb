// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/group_view.h"

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_tray.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/views/bounded_label.h"
#include "ui/message_center/views/constants.h"
#include "ui/message_center/views/notification_button.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

namespace {

// static
views::Border* MakeTextBorder(int padding, int top, int bottom) {
  // Split the padding between the top and the bottom, then add the extra space.
  return views::Border::CreateEmptyBorder(padding / 2 + top,
                                          message_center::kTextLeftPadding,
                                          (padding + 1) / 2 + bottom,
                                          message_center::kTextRightPadding);
}

}  // namespace

namespace message_center {

// GroupView ////////////////////////////////////////////////////////////

GroupView::GroupView(MessageCenterController* controller,
                     const NotifierId& notifier_id,
                     const Notification& last_notification,
                     const gfx::ImageSkia& group_icon,
                     int group_size)
    : MessageView(this,
                  last_notification.id(),
                  notifier_id,
                  last_notification.display_source()),
      controller_(controller),
      notifier_id_(notifier_id),
      display_source_(last_notification.display_source()),
      group_icon_(group_icon),
      group_size_(group_size),
      last_notification_id_(last_notification.id()),
      background_view_(NULL),
      top_view_(NULL),
      bottom_view_(NULL),
      title_view_(NULL),
      message_view_(NULL),
      context_message_view_(NULL),
      icon_view_(NULL)
{
  std::vector<string16> accessible_lines;
  // TODO (dimich): move to MessageView
  // Create the opaque background that's above the view's shadow.
  background_view_ = new views::View();
  background_view_->set_background(
      views::Background::CreateSolidBackground(
          message_center::kNotificationBackgroundColor));

  // Create the top_view_, which collects into a vertical box all content
  // at the top of the notification (to the right of the icon) except for the
  // close button.
  top_view_ = new views::View();
  top_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  top_view_->set_border(views::Border::CreateEmptyBorder(
      kTextTopPadding - 8, 0, kTextBottomPadding - 5, 0));

  const gfx::FontList default_label_font_list = views::Label().font_list();

  // Create the title view if appropriate.
  const gfx::FontList& font_list =
      default_label_font_list.DeriveFontListWithSizeDelta(2);
  int padding = kTitleLineHeight - font_list.GetHeight();

  title_view_ = new BoundedLabel(
      gfx::TruncateString(string16(last_notification.title()),
                          kTitleCharacterLimit),
                          font_list);
  accessible_lines.push_back(last_notification.title());
  title_view_->SetLineHeight(kTitleLineHeight);
  title_view_->SetLineLimit(message_center::kTitleLineLimit);
  title_view_->SetColors(message_center::kRegularTextColor,
                         kRegularTextBackgroundColor);
  title_view_->set_border(MakeTextBorder(padding, 3, 0));
  top_view_->AddChildView(title_view_);

  // Create the message view if appropriate.
  if (!last_notification.message().empty()) {
    int padding = kMessageLineHeight - default_label_font_list.GetHeight();
    message_view_ = new BoundedLabel(
        gfx::TruncateString(last_notification.message(),
                            kMessageCharacterLimit));
    message_view_->SetLineHeight(kMessageLineHeight);
    message_view_->SetColors(message_center::kRegularTextColor,
                             kDimTextBackgroundColor);
    message_view_->set_border(MakeTextBorder(padding, 4, 0));
    top_view_->AddChildView(message_view_);
    accessible_lines.push_back(last_notification.message());
  }

  // Create the context message view if appropriate.
  if (!last_notification.context_message().empty()) {
    int padding = kMessageLineHeight - default_label_font_list.GetHeight();
    context_message_view_ = new BoundedLabel(gfx::TruncateString(
        last_notification.context_message(), kContextMessageCharacterLimit),
        default_label_font_list);
    context_message_view_->SetLineLimit(
        message_center::kContextMessageLineLimit);
    context_message_view_->SetLineHeight(kMessageLineHeight);
    context_message_view_->SetColors(message_center::kDimTextColor,
                                     kContextTextBackgroundColor);
    context_message_view_->set_border(MakeTextBorder(padding, 4, 0));
    top_view_->AddChildView(context_message_view_);
    accessible_lines.push_back(last_notification.context_message());
  }

  // Create the notification icon view.
  icon_view_ =
      new ProportionalImageView(last_notification.icon().AsImageSkia());
  icon_view_->set_background(views::Background::CreateSolidBackground(
      kIconBackgroundColor));

  // Create the bottom_view_, which collects into a vertical box all content
  // below the notification icon except for the expandGroup button.
  bottom_view_ = new views::View();
  bottom_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

  // Create "N more.." action button
  views::View* separator = new views::ImageView();
  separator->set_border(views::Border::CreateSolidSidedBorder(
      1, 0, 0, 0, kButtonSeparatorColor));
  bottom_view_->AddChildView(separator);
  more_button_ = new NotificationButton(this);
  string16 button_title =
      l10n_util::GetStringFUTF16(IDS_MESSAGE_CENTER_MORE_FROM,
                                 base::IntToString16(group_size_),
                                 display_source_);
  more_button_->SetTitle(button_title);
  more_button_->SetIcon(group_icon_);
  bottom_view_->AddChildView(more_button_);

  // Put together the different content and control views. Layering those allows
  // for proper layout logic and it also allows the close button to
  // overlap the content as needed to provide large enough click and touch area.
  AddChildView(background_view_);
  AddChildView(top_view_);
  AddChildView(icon_view_);
  AddChildView(bottom_view_);
  AddChildView(close_button());
  set_accessible_name(JoinString(accessible_lines, '\n'));
}

GroupView::~GroupView() {
}

gfx::Size GroupView::GetPreferredSize() {
  int top_width = top_view_->GetPreferredSize().width();
  int bottom_width = bottom_view_->GetPreferredSize().width();
  int preferred_width = std::max(top_width, bottom_width) + GetInsets().width();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int GroupView::GetHeightForWidth(int width) {
  int content_width = width - GetInsets().width();
  int top_height = top_view_->GetHeightForWidth(content_width);
  int bottom_height = bottom_view_->GetHeightForWidth(content_width);
  int content_height = std::max(top_height, kIconSize) + bottom_height;

  // Adjust the height to make sure there is at least 16px of space below the
  // icon if there is any space there (<http://crbug.com/232966>).
  if (content_height > kIconSize)
    content_height = std::max(content_height,
                              kIconSize + message_center::kIconBottomPadding);

  return content_height + GetInsets().height();
}

void GroupView::Layout() {
  gfx::Insets insets = GetInsets();
  int content_width = width() - insets.width();
  int content_right = width() - insets.right();

  // Background.
  background_view_->SetBounds(insets.left(), insets.top(),
                              content_width, height() - insets.height());

  // Top views.
  int top_height = top_view_->GetHeightForWidth(content_width);
  top_view_->SetBounds(insets.left(), insets.top(), content_width, top_height);

  // Icon.
  icon_view_->SetBounds(insets.left(), insets.top(), kIconSize, kIconSize);

  // Bottom views.
  int bottom_y = insets.top() + std::max(top_height, kIconSize);
  int bottom_height = bottom_view_->GetHeightForWidth(content_width);
  bottom_view_->SetBounds(insets.left(), bottom_y,
                          content_width, bottom_height);

  // Close button.
  gfx::Size close_size(close_button()->GetPreferredSize());
  close_button()->SetBounds(content_right - close_size.width(), insets.top(),
                            close_size.width(), close_size.height());
}

void GroupView::OnFocus() {
  MessageView::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

gfx::NativeCursor GroupView::GetCursor(const ui::MouseEvent& event) {
// If we ever have non-Aura views environment, this will fail compilation.
#if defined(USE_AURA)
  return ui::kCursorHand;
#endif
}

void GroupView::ButtonPressed(views::Button* sender,
                              const ui::Event& event) {
  if (sender == more_button_) {
    controller_->ExpandGroup(notifier_id_);
    return;
  }
  // Let the superclass handle anything other than action buttons.
  // Warning: This may cause the GroupView itself to be deleted,
  // so don't do anything afterwards.
  MessageView::ButtonPressed(sender, event);
}

void GroupView::ClickOnNotification(const std::string& notification_id) {
  controller_->GroupBodyClicked(notification_id);
}

void GroupView::RemoveNotification(const std::string& notification_id,
                                   bool by_user) {
  controller_->RemoveGroup(notifier_id_);
}

void GroupView::DisableNotificationsFromThisSource(
    const NotifierId& notifier_id) {
  controller_->DisableNotificationsFromThisSource(notifier_id);
}

void GroupView::ShowNotifierSettingsBubble() {
  controller_->ShowNotifierSettingsBubble();
}

}  // namespace message_center
