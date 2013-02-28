// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/notification_view.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_simple_view.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace {

// Dimensions.
const int kIconColumnWidth = message_center::kNotificationIconSize;
const int kIconToTextPadding = 16;
const int kTextTopPadding = 6;
const int kTextBottomPadding = 6;
const int kTextRightPadding = 23;
const int kItemTitleToMessagePadding = 3;
const int kActionButtonHorizontalPadding = 16;
const int kActionButtonVecticalPadding = 0;
const int kActionButtonIconTopPadding = 12;
const int kActionButtonIconToTitlePadding = 16;
const int kActionButtonTitleTopPadding = 0;

// Notification colors. The text background colors below are used only to keep
// view::Label from modifying the text color and will not actually be drawn.
// See view::Label's SetEnabledColor() and SetBackgroundColor() for details.
const SkColor kBackgroundColor = SkColorSetRGB(255, 255, 255);
const SkColor kTitleColor = SkColorSetRGB(68, 68, 68);
const SkColor kTitleBackgroundColor = SK_ColorWHITE;
const SkColor kMessageColor = SkColorSetRGB(136, 136, 136);
const SkColor kMessageBackgroundColor = SK_ColorBLACK;
const SkColor kButtonSeparatorColor = SkColorSetRGB(234, 234, 234);

// Static function to create an empty border to be used as padding.
views::Border* MakePadding(int top, int left, int bottom, int right) {
  return views::Border::CreateEmptyBorder(top, left, bottom, right);
}

// Static function to create an empty border to be used as padding.
views::Background* MakeBackground(SkColor color) {
  return views::Background::CreateSolidBackground(color);
}

// ItemViews are responsible for drawing each list notification item's title and
// message next to each other within a single column.
class ItemView : public views::View {
 public:
  ItemView(const message_center::NotificationItem& item);
  virtual ~ItemView();

 private:
  DISALLOW_COPY_AND_ASSIGN(ItemView);
};

ItemView::ItemView(const message_center::NotificationItem& item) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        0, 0, kItemTitleToMessagePadding));

  views::Label* title = new views::Label(item.title);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetElideBehavior(views::Label::ELIDE_AT_END);
  title->SetEnabledColor(kTitleColor);
  title->SetBackgroundColor(kTitleBackgroundColor);
  AddChildView(title);

  views::Label* message = new views::Label(item.message);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetElideBehavior(views::Label::ELIDE_AT_END);
  message->SetEnabledColor(kMessageColor);
  message->SetBackgroundColor(kMessageBackgroundColor);
  AddChildView(message);

  PreferredSizeChanged();
  SchedulePaint();
}

ItemView::~ItemView() {
}

// ProportionalImageViews match their heights to their widths to preserve the
// proportions of their images.
class ProportionalImageView : public views::ImageView {
 public:
  ProportionalImageView();
  virtual ~ProportionalImageView();

  // Overridden from views::View.
  virtual int GetHeightForWidth(int width) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ProportionalImageView);
};

ProportionalImageView::ProportionalImageView() {
}

ProportionalImageView::~ProportionalImageView() {
}

int ProportionalImageView::GetHeightForWidth(int width) {
  int height = 0;
  gfx::ImageSkia image = GetImage();
  if (image.width() > 0 && image.height() > 0) {
    double proportion = image.height() / (double) image.width();
    height = 0.5 + width * proportion;
    if (height > message_center::kNotificationMaximumImageHeight) {
      height = message_center::kNotificationMaximumImageHeight;
      width = 0.5 + height / proportion;
    }
    SetImageSize(gfx::Size(width, height));
  }
  return height;
}

// NotificationsButtons render the action buttons of notifications.
class NotificationButton : public views::CustomButton {
 public:
  NotificationButton(views::ButtonListener* listener);
  virtual ~NotificationButton();

  void SetIcon(const gfx::ImageSkia& icon);
  void SetTitle(const string16& title);

 private:
  views::ImageView* icon_;
  views::Label* title_;
};

NotificationButton::NotificationButton(views::ButtonListener* listener)
    : views::CustomButton(listener),
      icon_(NULL),
      title_(NULL) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        kActionButtonHorizontalPadding,
                                        kActionButtonVecticalPadding,
                                        kActionButtonIconToTitlePadding));
}

NotificationButton::~NotificationButton() {
}

void NotificationButton::SetIcon(const gfx::ImageSkia& image) {
  if (icon_ != NULL)
    delete icon_;  // This removes the icon from this view's children.
  if (image.isNull()) {
    icon_ = NULL;
  } else {
    icon_ = new views::ImageView();
    icon_->SetImageSize(gfx::Size(message_center::kNotificationButtonIconSize,
                                  message_center::kNotificationButtonIconSize));
    icon_->SetImage(image);
    icon_->SetHorizontalAlignment(views::ImageView::LEADING);
    icon_->SetVerticalAlignment(views::ImageView::LEADING);
    icon_->set_border(MakePadding(kActionButtonIconTopPadding, 0, 0, 0));
    AddChildViewAt(icon_, 0);
  }
}

void NotificationButton::SetTitle(const string16& title) {
  if (title_ != NULL)
    delete title_;  // This removes the title from this view's children.
  if (title.empty()) {
    title_ = NULL;
  } else {
    title_ = new views::Label(title);
    title_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_->SetElideBehavior(views::Label::ELIDE_AT_END);
    title_->SetEnabledColor(kTitleColor);
    title_->SetBackgroundColor(kTitleBackgroundColor);
    title_->set_border(MakePadding(kActionButtonTitleTopPadding, 0, 0, 0));
    AddChildView(title_);
  }
}

}  // namespace

namespace message_center {

// static
MessageView* NotificationView::Create(
    const Notification& notification,
    NotificationList::Delegate* list_delegate) {
  // For the time being, use MessageSimpleView for simple notifications unless
  // one of the use-the-new-style flags are set. This preserves the appearance
  // of notifications created by existing code that uses webkitNotifications.
  if (notification.type() == NOTIFICATION_TYPE_SIMPLE &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          message_center::switches::kEnableRichNotifications) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          message_center::switches::kEnableNewSimpleNotifications)) {
    return new MessageSimpleView(list_delegate, notification);
  }

  switch (notification.type()) {
    case NOTIFICATION_TYPE_BASE_FORMAT:
    case NOTIFICATION_TYPE_IMAGE:
    case NOTIFICATION_TYPE_MULTIPLE:
    case NOTIFICATION_TYPE_SIMPLE:
      break;
    default:
      // If the caller asks for an unrecognized kind of view (entirely possible
      // if an application is running on an older version of this code that
      // doesn't have the requested kind of notification template), we'll fall
      // back to a notification instance that will provide at least basic
      // functionality.
      LOG(WARNING) << "Unable to fulfill request for unrecognized "
                   << "notification type " << notification.type() << ". "
                   << "Falling back to simple notification type.";
  }

  // Currently all roads lead to the generic NotificationView.
  return new NotificationView(list_delegate, notification);
}

NotificationView::NotificationView(NotificationList::Delegate* list_delegate,
                                   const Notification& notification)
    : MessageView(list_delegate, notification) {
  // This view is composed of two layers: The first layer has the notification
  // content (text, images, action buttons, ...). This is overlaid by a second
  // layer that has the notification close button and will later also have the
  // expand button. This allows the close and expand buttons to overlap the
  // content as needed to provide a large enough click area
  // (<http://crbug.com/168822> and touch area <http://crbug.com/168856>).
  AddChildView(MakeContentView(notification));
  AddChildView(close_button());
}

NotificationView::~NotificationView() {
}

void NotificationView::Layout() {
  if (content_view_) {
    gfx::Rect contents_bounds = GetContentsBounds();
    content_view_->SetBoundsRect(contents_bounds);
    if (close_button()) {
      gfx::Size size(close_button()->GetPreferredSize());
      close_button()->SetBounds(contents_bounds.right() - size.width(), 0,
                                size.width(), size.height());
    }
  }
}

gfx::Size NotificationView::GetPreferredSize() {
  if (!content_view_)
    return gfx::Size();
  gfx::Size size = content_view_->GetPreferredSize();
  if (border()) {
    gfx::Insets border_insets = border()->GetInsets();
    size.Enlarge(border_insets.width(), border_insets.height());
  }
  return size;
}

void NotificationView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  for (size_t i = 0; i < action_buttons_.size(); ++i) {
    if (action_buttons_[i] == sender) {
      list_delegate()->OnButtonClicked(notification_id(), i);
      return;
    }
  }
  MessageView::ButtonPressed(sender, event);
}

views::View* NotificationView::MakeContentView(
    const Notification& notification) {
  content_view_ = new views::View();
  content_view_->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));

  // The top part of the content view is composed of an icon view on the left
  // and a certain number of text views on the right (title and message or list
  // items), followed by a padding view. Laying out the icon view will require
  // information about the text views, so these are created first and collected
  // in this vector.
  std::vector<views::View*> texts;

  // Title if it exists.
  if (!notification.title().empty()) {
    views::Label* title = new views::Label(notification.title());
    title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title->SetElideBehavior(views::Label::ELIDE_AT_END);
    title->SetFont(title->font().DeriveFont(4));
    title->SetEnabledColor(kTitleColor);
    title->SetBackgroundColor(kTitleBackgroundColor);
    title->set_border(MakePadding(kTextTopPadding, 0, 3, kTextRightPadding));
    texts.push_back(title);
  }

  // Message if appropriate.
  if (notification.items().size() == 0 &&
      !notification.message().empty()) {
    views::Label* message = new views::Label(notification.message());
    message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    message->SetMultiLine(true);
    message->SetEnabledColor(kMessageColor);
    message->SetBackgroundColor(kMessageBackgroundColor);
    message->set_border(MakePadding(0, 0, 3, kTextRightPadding));
    texts.push_back(message);
  }

  // List notification items up to a maximum.
  int items = std::min(notification.items().size(),
                       kNotificationMaximumItems);
  for (int i = 0; i < items; ++i) {
    ItemView* item = new ItemView(notification.items()[i]);
    item->set_border(MakePadding(0, 0, 4, kTextRightPadding));
    texts.push_back(item);
  }

  // Set up the content view with a fixed-width icon column on the left and a
  // text column on the right that consumes the remaining space. To minimize the
  // number of columns and simplify column spanning, padding is applied to each
  // view within columns instead of through padding columns.
  views::GridLayout* layout = new views::GridLayout(content_view_);
  content_view_->SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     0, views::GridLayout::FIXED,
                     kIconColumnWidth + kIconToTextPadding,
                     kIconColumnWidth + kIconToTextPadding);
                     // Padding + icon + padding.
  columns->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL,
                     100, views::GridLayout::USE_PREF, 0, 0);
                     // Text + padding.

  // Create the first row and its icon view, which spans all the text views
  // to its right as well as the padding view below them.
  layout->StartRow(0, 0);
  views::ImageView* icon = new views::ImageView();
  icon->SetImageSize(gfx::Size(message_center::kNotificationIconSize,
                               message_center::kNotificationIconSize));
  icon->SetImage(notification.primary_icon());
  icon->SetHorizontalAlignment(views::ImageView::LEADING);
  icon->SetVerticalAlignment(views::ImageView::LEADING);
  icon->set_border(MakePadding(0, 0, 0, kIconToTextPadding));
  layout->AddView(icon, 1, texts.size() + 1);

  // Add the text views, creating rows for them if necessary.
  for (size_t i = 0; i < texts.size(); ++i) {
    if (i > 0) {
      layout->StartRow(0, 0);
      layout->SkipColumns(1);
    }
    layout->AddView(texts[i]);
  }

  // Add a text padding row if necessary. This adds some space between the last
  // line of text and anything below it but it also ensures views above it are
  // top-justified by expanding vertically to take up any extra space.
  if (texts.size() == 0) {
    layout->SkipColumns(1);
  } else {
    layout->StartRow(100, 0);
    layout->SkipColumns(1);
    views::View* padding = new views::ImageView();
    padding->set_border(MakePadding(kTextBottomPadding, 1, 0, 0));
    layout->AddView(padding);
  }

  // Add an image row if appropriate.
  if (!notification.image().isNull()) {
    layout->StartRow(0, 0);
    views::ImageView* image = new ProportionalImageView();
    image->SetImageSize(notification.image().size());
    image->SetImage(notification.image());
    image->SetHorizontalAlignment(views::ImageView::CENTER);
    image->SetVerticalAlignment(views::ImageView::LEADING);
    layout->AddView(image, 2, 1);
  }

  // Add action button rows.
  for (size_t i = 0; i < notification.buttons().size(); ++i) {
    views::View* separator = new views::View();
    separator->set_background(MakeBackground(kButtonSeparatorColor));
    layout->StartRow(0, 0);
    layout->AddView(separator, 2, 1,
                    views::GridLayout::FILL, views::GridLayout::FILL, 0, 1);
    NotificationButton* button = new NotificationButton(this);
    ButtonInfo button_info = notification.buttons()[i];
    button->SetTitle(button_info.title);
    button->SetIcon(button_info.icon);
    action_buttons_.push_back(button);
    layout->StartRow(0, 0);
    layout->AddView(button, 2, 1,
                    views::GridLayout::FILL, views::GridLayout::FILL, 0, 40);
  }

  return content_view_;
}

}  // namespace message_center
