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

namespace {

// Dimensions.
const int kIconColumnWidth = message_center::kNotificationIconSize;
const int kLegacyIconSize = 40;
const int kIconToTextPadding = 16;
const int kTextTopPadding = 6;
const int kTextLeftPadding = kIconColumnWidth + kIconToTextPadding;
const int kTextBottomPadding = 6;
const int kTextRightPadding = 23;
const int kItemTitleToMessagePadding = 3;
const int kButtonHeight = 38;
const int kButtonHorizontalPadding = 16;
const int kButtonVecticalPadding = 0;
const int kButtonIconTopPadding = 11;
const int kButtonIconToTitlePadding = 16;
const int kButtonTitleTopPadding = 0;

const size_t kTitleCharacterLimit = 100;
const size_t kMessageCharacterLimit = 200;

// Notification colors. The text background colors below are used only to keep
// view::Label from modifying the text color and will not actually be drawn.
// See view::Label's SetEnabledColor() and SetBackgroundColor() for details.
const SkColor kBackgroundColor = SkColorSetRGB(255, 255, 255);
const SkColor kLegacyIconBackgroundColor = SkColorSetRGB(230, 230, 230);
const SkColor kRegularTextColor = SkColorSetRGB(68, 68, 68);
const SkColor kRegularTextBackgroundColor = SK_ColorWHITE;
const SkColor kDimTextColor = SkColorSetRGB(136, 136, 136);
const SkColor kDimTextBackgroundColor = SK_ColorBLACK;
const SkColor kButtonSeparatorColor = SkColorSetRGB(234, 234, 234);

// Static.
views::Background* MakeBackground(SkColor color = kBackgroundColor) {
  return views::Background::CreateSolidBackground(color);
}

// Static.
views::Border* MakeBorder(int top,
                          int bottom,
                          int left = kTextLeftPadding,
                          int right = kTextRightPadding,
                          SkColor color = 0x00000000) {
  return (color == 0x00000000) ?
         views::Border::CreateEmptyBorder(top, left, bottom, right) :
         views::Border::CreateSolidSidedBorder(top, left, bottom, right, color);
}

// ContainerView ///////////////////////////////////////////////////////////////

// ContainerViews are vertical BoxLayout views that propagates their childrens'
// ChildPreferredSizeChanged() and ChildVisibilityChanged() calls.
class ContainerView : public views::View {
 public:
  ContainerView();
  virtual ~ContainerView();

 protected:
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void ChildVisibilityChanged(View* child) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ContainerView);
};

ContainerView::ContainerView() {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
}

ContainerView::~ContainerView() {
}

void ContainerView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

void ContainerView::ChildVisibilityChanged(View* child) {
  PreferredSizeChanged();
}

// ItemView ////////////////////////////////////////////////////////////////////

// ItemViews are responsible for drawing each list notification item's title and
// message next to each other within a single column.
class ItemView : public views::View {
 public:
  ItemView(const message_center::NotificationItem& item);
  virtual ~ItemView();

  virtual void SetVisible(bool visible) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ItemView);
};

ItemView::ItemView(const message_center::NotificationItem& item) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        0, 0, kItemTitleToMessagePadding));

  views::Label* title = new views::Label(item.title);
  title->set_collapse_when_hidden(true);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetElideBehavior(views::Label::ELIDE_AT_END);
  title->SetEnabledColor(kRegularTextColor);
  title->SetBackgroundColor(kRegularTextBackgroundColor);
  AddChildView(title);

  views::Label* message = new views::Label(item.message);
  message->set_collapse_when_hidden(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetElideBehavior(views::Label::ELIDE_AT_END);
  message->SetEnabledColor(kDimTextColor);
  message->SetBackgroundColor(kDimTextBackgroundColor);
  AddChildView(message);

  PreferredSizeChanged();
  SchedulePaint();
}

ItemView::~ItemView() {
}

void ItemView::SetVisible(bool visible) {
  views::View::SetVisible(visible);
  for (int i = 0; i < child_count(); ++i)
    child_at(i)->SetVisible(visible);
}

// ProportionalImageView ///////////////////////////////////////////////////////

// ProportionalImageViews center their images to preserve their proportion.
class ProportionalImageView : public views::View {
 public:
  ProportionalImageView(const gfx::ImageSkia& image);
  virtual ~ProportionalImageView();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  gfx::Size GetImageSizeForWidth(int width);

  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(ProportionalImageView);
};

ProportionalImageView::ProportionalImageView(const gfx::ImageSkia& image)
    : image_(image) {
}

ProportionalImageView::~ProportionalImageView() {
}

gfx::Size ProportionalImageView::GetPreferredSize() {
  gfx::Size size = GetImageSizeForWidth(image_.width());
  return gfx::Size(size.width() + GetInsets().width(),
                   size.height() + GetInsets().height());
}

int ProportionalImageView::GetHeightForWidth(int width) {
  return GetImageSizeForWidth(width).height();
}

void ProportionalImageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Size draw_size(GetImageSizeForWidth(width()));
  if (!draw_size.IsEmpty()) {
    gfx::Rect draw_bounds = GetLocalBounds();
    draw_bounds.Inset(GetInsets());
    draw_bounds.ClampToCenteredSize(draw_size);

    gfx::Size image_size(image_.size());
    if (image_size == draw_size) {
      canvas->DrawImageInt(image_, draw_bounds.x(), draw_bounds.y());
    } else {
      // Resize case
      SkPaint paint;
      paint.setFilterBitmap(true);
      canvas->DrawImageInt(image_, 0, 0,
                           image_size.width(), image_size.height(),
                           draw_bounds.x(), draw_bounds.y(),
                           draw_size.width(), draw_size.height(),
                           true, paint);
    }
  }
}

gfx::Size ProportionalImageView::GetImageSizeForWidth(int width) {
  gfx::Size size = visible() ? image_.size() : gfx::Size();
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

// NotificationButton //////////////////////////////////////////////////////////

// NotificationButtons render the action buttons of notifications.
class NotificationButton : public views::CustomButton {
 public:
  NotificationButton(views::ButtonListener* listener);
  virtual ~NotificationButton();

  void SetIcon(const gfx::ImageSkia& icon);
  void SetTitle(const string16& title);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int width) OVERRIDE;

 private:
  views::ImageView* icon_;
  views::Label* title_;
};

NotificationButton::NotificationButton(views::ButtonListener* listener)
    : views::CustomButton(listener),
      icon_(NULL),
      title_(NULL) {
  set_focusable(true);
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
                                        kButtonHorizontalPadding,
                                        kButtonVecticalPadding,
                                        kButtonIconToTitlePadding));
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
    icon_->set_border(MakeBorder(kButtonIconTopPadding, 0, 0, 0));
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
    title_->SetEnabledColor(kRegularTextColor);
    title_->SetBackgroundColor(kRegularTextBackgroundColor);
    title_->set_border(MakeBorder(kButtonTitleTopPadding, 0, 0, 0));
    AddChildView(title_);
  }
}

gfx::Size NotificationButton::GetPreferredSize() {
  return gfx::Size(std::numeric_limits<int>::max(), kButtonHeight);
}

int NotificationButton::GetHeightForWidth(int width) {
  return kButtonHeight;
}

}  // namespace

namespace message_center {

// NotificationView ////////////////////////////////////////////////////////////

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
    : MessageView(notification, observer, expanded) {
  // Create the opaque background that's above the view's shadow.
  background_view_ = new views::View();
  background_view_->set_background(MakeBackground());

  // Create the top_view_, which collects into a vertical box all content
  // at the top of the notification (to the right of the icon) except for the
  // close button.
  top_view_ = new ContainerView();

  // Create the title view if appropriate.
  title_view_ = NULL;
  if (!notification.title().empty()) {
    title_view_ = new views::Label(
        MaybeTruncateText( notification.title(), kTitleCharacterLimit));
    title_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (is_expanded())
      title_view_->SetMultiLine(true);
    else
      title_view_->SetElideBehavior(views::Label::ELIDE_AT_END);
    title_view_->SetFont(title_view_->font().DeriveFont(2));
    title_view_->SetEnabledColor(kRegularTextColor);
    title_view_->SetBackgroundColor(kRegularTextBackgroundColor);
    title_view_->set_border(MakeBorder(kTextTopPadding, 3));
    top_view_->AddChildView(title_view_);
  }

  // Create the message view if appropriate.
  message_view_ = NULL;
  if (!notification.message().empty()) {
    message_view_ = new views::Label(
        MaybeTruncateText(notification.message(), kMessageCharacterLimit));
    message_view_->SetVisible(!is_expanded() || !notification.items().size());
    message_view_->set_collapse_when_hidden(true);
    message_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    if (is_expanded())
      message_view_->SetMultiLine(true);
    else
      message_view_->SetElideBehavior(views::Label::ELIDE_AT_END);
    message_view_->SetEnabledColor(kRegularTextColor);
    message_view_->SetBackgroundColor(kRegularTextBackgroundColor);
    message_view_->set_border(MakeBorder(0, 3));
    top_view_->AddChildView(message_view_);
  }

  // Create the list item views (up to a maximum).
  std::vector<NotificationItem> items = notification.items();
  for (size_t i = 0; i < items.size() && i < kNotificationMaximumItems; ++i) {
    ItemView* item_view = new ItemView(items[i]);
    item_view->SetVisible(is_expanded());
    item_view->set_border(MakeBorder(0, 4));
    item_views_.push_back(item_view);
    top_view_->AddChildView(item_view);
  }

  // Create the notification icon view.
  if (notification.type() == NOTIFICATION_TYPE_SIMPLE) {
    views::ImageView* icon_view = new views::ImageView();
    icon_view->SetImage(notification.icon().AsImageSkia());
    icon_view->SetImageSize(gfx::Size(kLegacyIconSize, kLegacyIconSize));
    icon_view->SetHorizontalAlignment(views::ImageView::CENTER);
    icon_view->SetVerticalAlignment(views::ImageView::CENTER);
    icon_view->set_background(MakeBackground(kLegacyIconBackgroundColor));
    icon_view_ = icon_view;
  } else {
    icon_view_ = new ProportionalImageView(notification.icon().AsImageSkia());
  }

  // Create the bottom_view_, which collects into a vertical box all content
  // below the notification icon except for the expand button.
  bottom_view_ = new ContainerView();

  // Create the image view if appropriate.
  image_view_ = NULL;
  if (!notification.image().IsEmpty()) {
    image_view_ = new ProportionalImageView(notification.image().AsImageSkia());
    image_view_->SetVisible(is_expanded());
    bottom_view_->AddChildView(image_view_);
  }

  // Create action buttons if appropriate.
  std::vector<ButtonInfo> buttons = notification.buttons();
  for (size_t i = 0; i < buttons.size(); ++i) {
    views::View* separator = new views::ImageView();
    separator->set_border(MakeBorder(1, 0, 0, 0, kButtonSeparatorColor));
    bottom_view_->AddChildView(separator);
    NotificationButton* button = new NotificationButton(this);
    ButtonInfo button_info = buttons[i];
    button->SetTitle(button_info.title);
    button->SetIcon(button_info.icon.AsImageSkia());
    action_buttons_.push_back(button);
    bottom_view_->AddChildView(button);
  }

  // Hide the expand button if appropriate.
  bool expandable = item_views_.size() || image_view_;
  expand_button()->SetVisible(expandable && !is_expanded());

  // Put together the different content and control views. Layering those allows
  // for proper layout logic and it also allows the close and expand buttons to
  // overlap the content as needed to provide large enough click and touch areas
  // (<http://crbug.com/168822> and <http://crbug.com/168856>).
  AddChildView(background_view_);
  AddChildView(top_view_);
  AddChildView(icon_view_);
  AddChildView(bottom_view_);
  AddChildView(close_button());
  AddChildView(expand_button());
}

NotificationView::~NotificationView() {
}

gfx::Size NotificationView::GetPreferredSize() {
  int top_width = top_view_->GetPreferredSize().width();
  int bottom_width = bottom_view_->GetPreferredSize().width();
  int preferred_width = std::max(top_width, bottom_width) + GetInsets().width();
  return gfx::Size(preferred_width, GetHeightForWidth(preferred_width));
}

int NotificationView::GetHeightForWidth(int width) {
  gfx::Insets insets = GetInsets();
  int top_height = top_view_->GetHeightForWidth(width - insets.width());
  int bottom_height = bottom_view_->GetHeightForWidth(width - insets.width());
  int icon_size = message_center::kNotificationIconSize;
  return std::max(top_height, icon_size) + bottom_height + insets.height();
}

void NotificationView::Layout() {
  gfx::Insets insets = GetInsets();
  int content_width = width() - insets.width();
  int content_right = width() - insets.right();

  background_view_->SetBounds(insets.left(), insets.top(),
                              content_width, height() - insets.height());

  int top_height = top_view_->GetHeightForWidth(content_width);
  top_view_->SetBounds(insets.left(), insets.top(), content_width, top_height);

  int icon_size = message_center::kNotificationIconSize;
  icon_view_->SetBounds(insets.left(), insets.top(), icon_size, icon_size);

  int bottom_y = insets.top() + std::max(top_height, icon_size);
  int bottom_height = bottom_view_->GetHeightForWidth(content_width);
  bottom_view_->SetBounds(insets.left(), bottom_y,
                          content_width, bottom_height);

  gfx::Size close_size(close_button()->GetPreferredSize());
  close_button()->SetBounds(content_right - close_size.width(), insets.top(),
                            close_size.width(), close_size.height());

  gfx::Size expand_size(expand_button()->GetPreferredSize());
  int expand_y = bottom_y - expand_size.height();
  expand_button()->SetBounds(content_right - expand_size.width(), expand_y,
                             expand_size.width(), expand_size.height());
}

void NotificationView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  // See if the button pressed was an action button.
  for (size_t i = 0; i < action_buttons_.size(); ++i) {
    if (sender == action_buttons_[i]) {
      observer()->OnButtonClicked(notification_id(), i);
      return;
    }
  }

  // Let the superclass handled anything other than action buttons.
  MessageView::ButtonPressed(sender, event);

  // Show and hide subviews appropriately on expansion.
  if (sender == expand_button()) {
    if (message_view_ && item_views_.size())
      message_view_->SetVisible(false);
    for (size_t i = 0; i < item_views_.size(); ++i)
      item_views_[i]->SetVisible(true);
    if (image_view_)
      image_view_->SetVisible(true);
    expand_button()->SetVisible(false);
    PreferredSizeChanged();
    SchedulePaint();
  }
}

string16 NotificationView::MaybeTruncateText(const string16& text,
                                             size_t limit) {
  // Currently just truncate the text by the total number of characters.
  // TODO(mukai): add better assumption like number of lines.
  if (!is_expanded())
    return text;

  return ui::TruncateString(text, limit);
}

}  // namespace message_center
