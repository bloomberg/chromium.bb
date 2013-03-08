// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_change_observer.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/message_simple_view.h"
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

// ProportionalImageViews center their images to preserve their proportion.
// Note that for this subclass of views::ImageView GetImageBounds() will return
// potentially incorrect values (this can't be fixed because GetImageBounds()
// is not virtual) and horizontal and vertical alignments will be ignored.
class ProportionalImageView : public views::ImageView {
 public:
  ProportionalImageView();
  virtual ~ProportionalImageView();

  // Overridden from views::View:
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  gfx::Size GetImageSizeForWidth(int width);

  DISALLOW_COPY_AND_ASSIGN(ProportionalImageView);
};

ProportionalImageView::ProportionalImageView() {
}

ProportionalImageView::~ProportionalImageView() {
}

int ProportionalImageView::GetHeightForWidth(int width) {
  return GetImageSizeForWidth(width).height();
}

void ProportionalImageView::OnPaint(gfx::Canvas* canvas) {
  View::OnPaint(canvas);

  gfx::Size draw_size(GetImageSizeForWidth(width()));
  if (!draw_size.IsEmpty()) {
    int x = (width() - draw_size.width()) / 2;
    int y = (height() - draw_size.height()) / 2;

    gfx::Size image_size(GetImage().size());
    if (image_size == draw_size) {
      canvas->DrawImageInt(GetImage(), x, y);
    } else {
      // Resize case
      SkPaint paint;
      paint.setFilterBitmap(true);
      canvas->DrawImageInt(GetImage(), 0, 0,
                           image_size.width(), image_size.height(), x, y,
                           draw_size.width(), draw_size.height(), true, paint);
    }
  }
}

gfx::Size ProportionalImageView::GetImageSizeForWidth(int width) {
  gfx::Size size(GetImage().size());
  if (width > 0 && !size.IsEmpty()) {
    double proportion = size.height() / (double) size.width();
    size.SetSize(width, std::max(0.5 + width * proportion, 1.0));
    if (size.height() > message_center::kNotificationMaximumImageHeight) {
      int height = message_center::kNotificationMaximumImageHeight;
      size.SetSize(std::max(0.5 + height / proportion, 1.0), height);
    }
  }
  return size;
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
MessageView* NotificationView::Create(const Notification& notification,
                                      NotificationChangeObserver* observer,
                                      bool expanded) {
  // For the time being, use MessageSimpleView for simple notifications unless
  // one of the use-the-new-style flags are set. This preserves the appearance
  // of notifications created by existing code that uses webkitNotifications.
  if (notification.type() == NOTIFICATION_TYPE_SIMPLE &&
      !IsRichNotificationEnabled() &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          message_center::switches::kEnableNewSimpleNotifications)) {
    return new MessageSimpleView(notification, observer);
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
  return new NotificationView(notification, observer, expanded);
}

NotificationView::NotificationView(const Notification& notification,
                                   NotificationChangeObserver* observer,
                                   bool expanded)
    : MessageView(notification, observer, expanded),
      content_view_(NULL),
      icon_view_(NULL) {
  AddChildViews(notification);
}

NotificationView::~NotificationView() {
}

void NotificationView::Layout() {
  gfx::Rect content_bounds(GetLocalBounds());
  content_bounds.Inset(GetInsets());
  content_view_->SetBoundsRect(content_bounds);

  gfx::Size close_size(close_button()->GetPreferredSize());
  close_button()->SetBounds(content_bounds.right() - close_size.width(),
                            content_bounds.y(),
                            close_size.width(),
                            close_size.height());

  gfx::Rect icon_bounds(content_bounds.origin(), icon_view_->size());
  gfx::Size expand_size(expand_button()->GetPreferredSize());
  expand_button()->SetBounds(content_bounds.right() - expand_size.width(),
                             icon_bounds.bottom() - expand_size.height(),
                             expand_size.width(),
                             expand_size.height());
}

gfx::Size NotificationView::GetPreferredSize() {
  gfx::Size size;
  if (content_view_) {
    size = content_view_->GetPreferredSize();
    if (border()) {
      gfx::Insets insets = border()->GetInsets();
      size.Enlarge(insets.width(), insets.height());
    }
  }
  return size;
}

void NotificationView::Update(const Notification& notification) {
  MessageView::Update(notification);
  content_view_ = NULL;
  icon_view_ = NULL;
  action_buttons_.clear();
  RemoveAllChildViews(true);
  AddChildViews(notification);
  PreferredSizeChanged();
  SchedulePaint();
}

void NotificationView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  for (size_t i = 0; i < action_buttons_.size(); ++i) {
    if (sender == action_buttons_[i]) {
      observer()->OnButtonClicked(notification_id(), i);
      return;
    }
  }
  MessageView::ButtonPressed(sender, event);
}

void NotificationView::AddChildViews(const Notification& notification) {
  // Child views are in two layers: The first layer has the notification content
  // (text, images, action buttons, ...). This is overlaid by a second layer
  // that has the notification close and expand buttons. This allows the close
  // and expand buttons to overlap the content as needed to provide large enough
  // click and touch areas (<http://crbug.com/168822> and
  // <http://crbug.com/168856>).
  AddContentView(notification);
  AddChildView(close_button());
  if (!is_expanded()) {
    AddChildView(expand_button());
  }
}

void NotificationView::AddContentView(const Notification& notification) {
  content_view_ = new views::View();
  content_view_->set_background(
      views::Background::CreateSolidBackground(kBackgroundColor));

  // The top part of the content view is composed of an icon view on the left
  // and a certain number of text views on the right (title and message or list
  // items), followed by a padding view. Laying out the icon view will require
  // information about the text views, so these are created first and collected
  // in this vector.
  std::vector<views::View*> text_views;

  // Title if it exists.
  if (!notification.title().empty()) {
    views::Label* title = new views::Label(notification.title());
    title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (is_expanded())
      title->SetMultiLine(true);
    else
      title->SetElideBehavior(views::Label::ELIDE_AT_END);
    title->SetFont(title->font().DeriveFont(4));
    title->SetEnabledColor(kTitleColor);
    title->SetBackgroundColor(kTitleBackgroundColor);
    title->set_border(MakePadding(kTextTopPadding, 0, 3, kTextRightPadding));
    text_views.push_back(title);
  }

  // List notification items up to a maximum if appropriate.
  size_t maximum = is_expanded() ? kNotificationMaximumItems : 0ul;
  size_t items = std::min(notification.items().size(), maximum);
  for (size_t i = 0; i < items; ++i) {
    ItemView* item = new ItemView(notification.items()[i]);
    item->set_border(MakePadding(0, 0, 4, kTextRightPadding));
    text_views.push_back(item);
  }

  // Message if appropriate.
  if (items == 0ul && !notification.message().empty()) {
    views::Label* message = new views::Label(notification.message());
    message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (is_expanded())
      message->SetMultiLine(true);
    else
      message->SetElideBehavior(views::Label::ELIDE_AT_END);
    message->SetEnabledColor(kMessageColor);
    message->SetBackgroundColor(kMessageBackgroundColor);
    message->set_border(MakePadding(0, 0, 3, kTextRightPadding));
    text_views.push_back(message);
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
  icon_view_ = new views::ImageView();
  icon_view_->SetImageSize(gfx::Size(message_center::kNotificationIconSize,
                                     message_center::kNotificationIconSize));
  icon_view_->SetImage(notification.icon().AsImageSkia());
  icon_view_->SetHorizontalAlignment(views::ImageView::LEADING);
  icon_view_->SetVerticalAlignment(views::ImageView::LEADING);
  icon_view_->set_border(MakePadding(0, 0, 0, kIconToTextPadding));
  layout->AddView(icon_view_, 1, text_views.size() + 1);

  // Add the text views, creating rows for them if necessary.
  for (size_t i = 0; i < text_views.size(); ++i) {
    if (i > 0) {
      layout->StartRow(0, 0);
      layout->SkipColumns(1);
    }
    layout->AddView(text_views[i]);
  }

  // Add a text padding row if necessary. This adds some space between the last
  // line of text and anything below it but it also ensures views above it are
  // top-justified by expanding vertically to take up any extra space.
  if (text_views.size() == 0) {
    layout->SkipColumns(1);
  } else {
    layout->StartRow(100, 0);
    layout->SkipColumns(1);
    views::View* padding = new views::ImageView();
    padding->set_border(MakePadding(kTextBottomPadding, 1, 0, 0));
    layout->AddView(padding);
  }

  // Add an image row if appropriate.
  if (is_expanded() && !notification.image().IsEmpty()) {
    layout->StartRow(0, 0);
    ProportionalImageView* image_view = new ProportionalImageView();
    image_view->SetImage(notification.image().ToImageSkia());
    layout->AddView(image_view, 2, 1);
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
    button->SetIcon(button_info.icon.AsImageSkia());
    action_buttons_.push_back(button);
    layout->StartRow(0, 0);
    layout->AddView(button, 2, 1,
                    views::GridLayout::FILL, views::GridLayout::FILL, 0, 40);
  }

  AddChildView(content_view_);
}

}  // namespace message_center
