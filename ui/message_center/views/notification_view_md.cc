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
#include "ui/message_center/views/notification_button.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/native_cursor.h"
#include "ui/views/view_targeter.h"

namespace message_center {

namespace {

// Dimensions.
constexpr int kNotificationRightPadding = 13;
constexpr int kNotificationLeftPadding = 13;
constexpr int kNotificationTopPadding = 6;
constexpr int kNotificationBottomPadding = 10;
constexpr gfx::Insets kNotificationPadding(kNotificationTopPadding,
                                           kNotificationLeftPadding,
                                           kNotificationBottomPadding,
                                           kNotificationRightPadding);

constexpr int kMaxContextTitleLines = 1;

// Foreground of small icon image.
constexpr SkColor kSmallImageBackgroundColor = SK_ColorWHITE;
// Background of small icon image.
const SkColor kSmallImageColor = SkColorSetRGB(0x43, 0x43, 0x43);

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
  if (settings_button_)
    buttons.push_back(settings_button_.get());
  if (close_button_)
    buttons.push_back(close_button_.get());

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
}

NotificationViewMD::NotificationViewMD(MessageCenterController* controller,
                                       const Notification& notification)
    : MessageView(controller, notification),
      clickable_(notification.clickable()) {
  layout_ = new views::BoxLayout(views::BoxLayout::kVertical, gfx::Insets(), 2);
  layout_->set_inside_border_insets(kNotificationPadding);
  SetLayoutManager(layout_);

  // Create the top_view_, which collects into a vertical box all content
  // at the top of the notification (to the right of the icon) except for the
  // close button.
  top_view_ = new views::View();
  views::BoxLayout* top_box_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, gfx::Insets(1, 0), 5);
  top_box_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  top_view_->SetLayoutManager(top_box_layout);
  AddChildView(top_view_);

  main_view_ = new views::View();
  main_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical));
  AddChildView(main_view_);

  // Create the bottom_view_, which collects notification icon.
  bottom_view_ = new views::View();
  bottom_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical));
  AddChildView(bottom_view_);

  views::ImageView* small_image_view = new views::ImageView();
  small_image_view->SetImageSize(gfx::Size(kSmallImageSize, kSmallImageSize));
  small_image_view->set_owned_by_client();
  small_image_view_.reset(small_image_view);
  top_view_->AddChildView(small_image_view_.get());

  CreateOrUpdateViews(notification);

  SetEventTargeter(
      std::unique_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
}

NotificationViewMD::~NotificationViewMD() {}

void NotificationViewMD::Layout() {
  MessageView::Layout();

  // Before any resizing, set or adjust the number of message lines.
  int title_lines = 0;
  if (title_view_) {
    title_lines = title_view_->GetLinesForWidthAndLimit(title_view_->width(),
                                                        kMaxTitleLines);
  }
  if (message_view_) {
    message_view_->SetLineLimit(
        std::max(0, message_center::kMessageExpandedLineLimit - title_lines));
  }

  // Settings & Bottom views.
  if (settings_button_) {
    gfx::Rect content_bounds = GetContentsBounds();
    const gfx::Size settings_size(settings_button_->GetPreferredSize());
    int marginFromRight = settings_size.width() + kControlButtonPadding;
    if (close_button_)
      marginFromRight += close_button_->GetPreferredSize().width();
    gfx::Rect settings_rect(content_bounds.right() - marginFromRight,
                            GetContentsBounds().y() + kControlButtonPadding,
                            settings_size.width(), settings_size.height());
    settings_button_->SetBoundsRect(settings_rect);
  }

  // Close button.
  if (close_button_) {
    gfx::Rect content_bounds = GetContentsBounds();
    gfx::Size close_size(close_button_->GetPreferredSize());
    gfx::Rect close_rect(
        content_bounds.right() - close_size.width() - kControlButtonPadding,
        content_bounds.y() + kControlButtonPadding, close_size.width(),
        close_size.height());
    close_button_->SetBoundsRect(close_rect);
  }
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

  if (close_button_ && sender == close_button_.get()) {
    // Warning: This causes the NotificationViewMD itself to be deleted, so
    // don't do anything afterwards.
    OnCloseButtonPressed();
    return;
  }

  if (sender == settings_button_.get()) {
    controller()->ClickOnSettingsButton(id);
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
  if (!close_button_)
    return false;

  const views::FocusManager* focus_manager = GetFocusManager();
  return focus_manager &&
         focus_manager->GetFocusedView() == close_button_.get();
}

void NotificationViewMD::RequestFocusOnCloseButton() {
  if (close_button_)
    close_button_->RequestFocus();
}

void NotificationViewMD::CreateOrUpdateContextTitleView(
    const Notification& notification) {
  DCHECK(top_view_);

  const gfx::FontList& font_list = views::Label().font_list().Derive(
      -2, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);

  base::string16 sub_title = notification.display_source();
  if (!context_title_view_) {
    context_title_view_ = new BoundedLabel(sub_title, font_list);
    context_title_view_->SetLineHeight(kTitleLineHeight);
    context_title_view_->SetLineLimit(kMaxContextTitleLines);
    top_view_->AddChildView(context_title_view_);
  } else {
    context_title_view_->SetText(sub_title);
  }
}

void NotificationViewMD::CreateOrUpdateTitleView(
    const Notification& notification) {
  DCHECK(top_view_ != NULL);

  const gfx::FontList& font_list = views::Label().font_list().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);

  int title_character_limit =
      kNotificationWidth * kMaxTitleLines / kMinPixelsPerTitleCharacter;

  base::string16 title = gfx::TruncateString(
      notification.title(), title_character_limit, gfx::WORD_BREAK);
  if (!title_view_) {
    title_view_ = new BoundedLabel(title, font_list);
    title_view_->SetLineHeight(kMessageLineHeight);
    title_view_->SetColors(message_center::kRegularTextColor,
                           kDimTextBackgroundColor);
    main_view_->AddChildView(title_view_);
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

  DCHECK(top_view_ != NULL);

  base::string16 text = gfx::TruncateString(
      notification.message(), kMessageCharacterLimit, gfx::WORD_BREAK);

  const gfx::FontList& font_list = views::Label().font_list().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);

  if (!message_view_) {
    message_view_ = new BoundedLabel(text, font_list);
    message_view_->SetLineLimit(message_center::kMessageExpandedLineLimit);
    message_view_->SetColors(message_center::kDimTextColor,
                             kContextTextBackgroundColor);
    main_view_->AddChildView(message_view_);
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
  // TODO(yoshiki): Implement this.
}

void NotificationViewMD::CreateOrUpdateSmallIconView(
    const Notification& notification) {
  gfx::ImageSkia icon =
      notification.small_image().IsEmpty()
          ? GetProductIcon()
          : GetMaskedIcon(notification.small_image().AsImageSkia());

  small_image_view_->SetImage(icon);
}

void NotificationViewMD::CreateOrUpdateImageView(
    const Notification& notification) {
  // TODO(yoshiki): Implement this.
}

void NotificationViewMD::CreateOrUpdateActionButtonViews(
    const Notification& notification) {
  // TODO(yoshiki): Implement this.
}

void NotificationViewMD::CreateOrUpdateCloseButtonView(
    const Notification& notification) {
  if (!notification.pinned() && !close_button_) {
    close_button_ = base::MakeUnique<PaddedButton>(this);
    close_button_->SetImage(views::Button::STATE_NORMAL, GetCloseIcon());
    close_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_ACCESSIBLE_NAME));
    close_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_CLOSE_NOTIFICATION_BUTTON_TOOLTIP));
    close_button_->set_owned_by_client();
    AddChildView(close_button_.get());
    UpdateControlButtonsVisibility();
  } else if (notification.pinned() && close_button_) {
    close_button_.reset();
  }
}

void NotificationViewMD::CreateOrUpdateSettingsButtonView(
    const Notification& notification) {
  if (!settings_button_ && notification.delegate() &&
      notification.delegate()->ShouldDisplaySettingsButton()) {
    settings_button_ = base::MakeUnique<PaddedButton>(this);
    settings_button_->SetImage(views::Button::STATE_NORMAL, GetSettingsIcon());
    settings_button_->SetAccessibleName(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->SetTooltipText(l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_SETTINGS_BUTTON_ACCESSIBLE_NAME));
    settings_button_->set_owned_by_client();
    AddChildView(settings_button_.get());
  } else {
    settings_button_.reset();
  }
  UpdateControlButtonsVisibility();
}

void NotificationViewMD::UpdateControlButtonsVisibility() {
  const bool target_visibility =
      IsMouseHovered() || HasFocus() ||
      (close_button_ && close_button_->HasFocus()) ||
      (settings_button_ && settings_button_->HasFocus());

  if (close_button_) {
    if (target_visibility != close_button_->visible())
      close_button_->SetVisible(target_visibility);
  }

  if (settings_button_) {
    if (target_visibility != settings_button_->visible())
      settings_button_->SetVisible(target_visibility);
  }
}

}  // namespace message_center
