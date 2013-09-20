// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view.h"

#include "base/command_line.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/message_center_switches.h"
#include "ui/message_center/message_center_util.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/views/bounded_label.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/base/cursor/cursor.h"
#endif

namespace {

// Dimensions.
const int kIconSize = message_center::kNotificationIconSize;
const int kLegacyIconSize = 40;
const int kTextLeftPadding = kIconSize + message_center::kIconToTextPadding;
const int kTextBottomPadding = 12;
const int kTextRightPadding = 23;
const int kItemTitleToMessagePadding = 3;
const int kProgressBarWidth = message_center::kNotificationWidth -
    kTextLeftPadding - kTextRightPadding;
const int kProgressBarBottomPadding = 0;
const int kButtonVecticalPadding = 0;
const int kButtonTitleTopPadding = 0;

// Character limits: Displayed text will be subject to the line limits above,
// but we also remove trailing characters from text to reduce processing cost.
// Character limit = pixels per line * line limit / min. pixels per character.
const size_t kTitleCharacterLimit =
    message_center::kNotificationWidth * message_center::kTitleLineLimit / 4;
const size_t kMessageCharacterLimit =
    message_center::kNotificationWidth *
        message_center::kMessageExpandedLineLimit / 3;
const size_t kContextMessageCharacterLimit =
    message_center::kNotificationWidth *
    message_center::kContextMessageLineLimit / 3;

// Notification colors. The text background colors below are used only to keep
// view::Label from modifying the text color and will not actually be drawn.
// See view::Label's RecalculateColors() for details.
const SkColor kRegularTextBackgroundColor = SK_ColorWHITE;
const SkColor kDimTextBackgroundColor = SK_ColorWHITE;
const SkColor kContextTextBackgroundColor = SK_ColorWHITE;

// static
views::Background* MakeBackground(
    SkColor color = message_center::kNotificationBackgroundColor) {
  return views::Background::CreateSolidBackground(color);
}

// static
views::Border* MakeEmptyBorder(int top, int left, int bottom, int right) {
  return views::Border::CreateEmptyBorder(top, left, bottom, right);
}

// static
views::Border* MakeTextBorder(int padding, int top, int bottom) {
  // Split the padding between the top and the bottom, then add the extra space.
  return MakeEmptyBorder(padding / 2 + top, kTextLeftPadding,
                         (padding + 1) / 2 + bottom, kTextRightPadding);
}

// static
views::Border* MakeProgressBarBorder(int top, int bottom) {
  return MakeEmptyBorder(top, kTextLeftPadding, bottom, kTextRightPadding);
}

// static
views::Border* MakeSeparatorBorder(int top, int left, SkColor color) {
  return views::Border::CreateSolidSidedBorder(top, left, 0, 0, color);
}

// static
// Return true if and only if the image is null or has alpha.
bool HasAlpha(gfx::ImageSkia& image, views::Widget* widget) {
  // Determine which bitmap to use.
  ui::ScaleFactor factor = ui::SCALE_FACTOR_100P;
  if (widget) {
    factor = ui::GetScaleFactorForNativeView(widget->GetNativeView());
    if (factor == ui::SCALE_FACTOR_NONE)
      factor = ui::SCALE_FACTOR_100P;
  }

  // Extract that bitmap's alpha and look for a non-opaque pixel there.
  SkBitmap bitmap =
      image.GetRepresentation(ui::GetImageScale(factor)).sk_bitmap();
  if (!bitmap.isNull()) {
    SkBitmap alpha;
    alpha.setConfig(SkBitmap::kA1_Config, bitmap.width(), bitmap.height(), 0);
    bitmap.extractAlpha(&alpha);
    for (int y = 0; y < bitmap.height(); ++y) {
      for (int x = 0; x < bitmap.width(); ++x) {
        if (alpha.getColor(x, y) != SK_ColorBLACK) {
          return true;
        }
      }
    }
  }

  // If no opaque pixel was found, return false unless the bitmap is empty.
  return bitmap.isNull();
}

// ItemView ////////////////////////////////////////////////////////////////////

// ItemViews are responsible for drawing each list notification item's title and
// message next to each other within a single column.
class ItemView : public views::View {
 public:
  ItemView(const message_center::NotificationItem& item);
  virtual ~ItemView();

  // Overridden from views::View:
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
  title->SetEnabledColor(message_center::kRegularTextColor);
  title->SetBackgroundColor(kRegularTextBackgroundColor);
  AddChildView(title);

  views::Label* message = new views::Label(item.message);
  message->set_collapse_when_hidden(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetEnabledColor(message_center::kDimTextColor);
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
    gfx::Rect draw_bounds = GetContentsBounds();
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
  return message_center::GetImageSizeForWidth(width, size);
}

// NotificationProgressBar /////////////////////////////////////////////////////

class NotificationProgressBar : public views::ProgressBar {
 public:
  NotificationProgressBar();
  virtual ~NotificationProgressBar();

 private:
  // Overriden from View
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(NotificationProgressBar);
};

NotificationProgressBar::NotificationProgressBar() {
}

NotificationProgressBar::~NotificationProgressBar() {
}

gfx::Size NotificationProgressBar::GetPreferredSize() {
  gfx::Size pref_size(kProgressBarWidth, message_center::kProgressBarThickness);
  gfx::Insets insets = GetInsets();
  pref_size.Enlarge(insets.width(), insets.height());
  return pref_size;
}

void NotificationProgressBar::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect content_bounds = GetContentsBounds();

  // Draw background.
  SkPath background_path;
  background_path.addRoundRect(gfx::RectToSkRect(content_bounds),
                               message_center::kProgressBarCornerRadius,
                               message_center::kProgressBarCornerRadius);
  SkPaint background_paint;
  background_paint.setStyle(SkPaint::kFill_Style);
  background_paint.setFlags(SkPaint::kAntiAlias_Flag);
  background_paint.setColor(message_center::kProgressBarBackgroundColor);
  canvas->DrawPath(background_path, background_paint);

  // Draw slice.
  const int slice_width =
      static_cast<int>(content_bounds.width() * GetNormalizedValue() + 0.5);
  if (slice_width < 1)
    return;

  gfx::Rect slice_bounds = content_bounds;
  slice_bounds.set_width(slice_width);
  SkPath slice_path;
  slice_path.addRoundRect(gfx::RectToSkRect(slice_bounds),
                          message_center::kProgressBarCornerRadius,
                          message_center::kProgressBarCornerRadius);
  SkPaint slice_paint;
  slice_paint.setStyle(SkPaint::kFill_Style);
  slice_paint.setFlags(SkPaint::kAntiAlias_Flag);
  slice_paint.setColor(message_center::kProgressBarSliceColor);
  canvas->DrawPath(slice_path, slice_paint);
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
  virtual void OnFocus() OVERRIDE;
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::CustomButton:
  virtual void StateChanged() OVERRIDE;

 private:
  views::ImageView* icon_;
  views::Label* title_;
};

NotificationButton::NotificationButton(views::ButtonListener* listener)
    : views::CustomButton(listener),
      icon_(NULL),
      title_(NULL) {
  set_focusable(true);
  set_request_focus_on_press(false);
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           message_center::kButtonHorizontalPadding,
                           kButtonVecticalPadding,
                           message_center::kButtonIconToTitlePadding));
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
    icon_->set_border(
        MakeEmptyBorder(message_center::kButtonIconTopPadding, 0, 0, 0));
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
    title_->SetEnabledColor(message_center::kRegularTextColor);
    title_->SetBackgroundColor(kRegularTextBackgroundColor);
    title_->set_border(MakeEmptyBorder(kButtonTitleTopPadding, 0, 0, 0));
    AddChildView(title_);
  }
  SetAccessibleName(title);
}

gfx::Size NotificationButton::GetPreferredSize() {
  return gfx::Size(message_center::kNotificationWidth,
                   message_center::kButtonHeight);
}

int NotificationButton::GetHeightForWidth(int width) {
  return message_center::kButtonHeight;
}

void NotificationButton::OnFocus() {
  views::CustomButton::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

void NotificationButton::OnPaintFocusBorder(gfx::Canvas* canvas) {
  if (HasFocus() && (focusable() || IsAccessibilityFocusable())) {
    canvas->DrawRect(gfx::Rect(2, 1, width() - 4, height() - 3),
                     message_center::kFocusBorderColor);
  }
}

void NotificationButton::StateChanged() {
  if (state() == STATE_HOVERED || state() == STATE_PRESSED) {
    set_background(
        MakeBackground(message_center::kHoveredButtonBackgroundColor));
  } else {
    set_background(NULL);
  }
}

}  // namespace

namespace message_center {

// NotificationView ////////////////////////////////////////////////////////////

// static
MessageView* NotificationView::Create(const Notification& notification,
                                      MessageCenter* message_center,
                                      MessageCenterTray* tray,
                                      bool expanded,
                                      bool top_level) {
  switch (notification.type()) {
    case NOTIFICATION_TYPE_BASE_FORMAT:
    case NOTIFICATION_TYPE_IMAGE:
    case NOTIFICATION_TYPE_MULTIPLE:
    case NOTIFICATION_TYPE_SIMPLE:
    case NOTIFICATION_TYPE_PROGRESS:
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
  MessageView* notification_view =
      new NotificationView(notification, message_center, tray, expanded);

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Don't create shadows for notification toasts on linux wih aura.
  if (top_level)
    return notification_view;
#endif

  notification_view->CreateShadowBorder();
  return notification_view;
}

NotificationView::NotificationView(const Notification& notification,
                                   MessageCenter* message_center,
                                   MessageCenterTray* tray,
                                   bool expanded)
    : MessageView(notification, message_center, tray, expanded) {
  std::vector<string16> accessible_lines;

  // Create the opaque background that's above the view's shadow.
  background_view_ = new views::View();
  background_view_->set_background(MakeBackground());

  // Create the top_view_, which collects into a vertical box all content
  // at the top of the notification (to the right of the icon) except for the
  // close button.
  top_view_ = new views::View();
  top_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  top_view_->set_border(MakeEmptyBorder(
      kTextTopPadding - 8, 0, kTextBottomPadding - 5, 0));

  const gfx::FontList default_label_font_list = views::Label().font_list();

  // Create the title view if appropriate.
  title_view_ = NULL;
  if (!notification.title().empty()) {
    const gfx::FontList& font_list =
        default_label_font_list.DeriveFontListWithSizeDelta(2);
    int padding = kTitleLineHeight - font_list.GetHeight();
    title_view_ = new BoundedLabel(
        gfx::TruncateString(notification.title(), kTitleCharacterLimit),
        font_list);
    title_view_->SetLineHeight(kTitleLineHeight);
    title_view_->SetLineLimit(message_center::kTitleLineLimit);
    title_view_->SetColors(message_center::kRegularTextColor,
                           kRegularTextBackgroundColor);
    title_view_->set_border(MakeTextBorder(padding, 3, 0));
    top_view_->AddChildView(title_view_);
    accessible_lines.push_back(notification.title());
  }

  // Create the message view if appropriate.
  message_view_ = NULL;
  if (!notification.message().empty()) {
    int padding = kMessageLineHeight - default_label_font_list.GetHeight();
    message_view_ = new BoundedLabel(
        gfx::TruncateString(notification.message(), kMessageCharacterLimit));
    message_view_->SetLineHeight(kMessageLineHeight);
    message_view_->SetVisible(!is_expanded() || !notification.items().size());
    message_view_->SetColors(message_center::kRegularTextColor,
                             kDimTextBackgroundColor);
    message_view_->set_border(MakeTextBorder(padding, 4, 0));
    top_view_->AddChildView(message_view_);
    accessible_lines.push_back(notification.message());
  }

  // Create the context message view if appropriate.
  context_message_view_ = NULL;
  if (!notification.context_message().empty()) {
    int padding = kMessageLineHeight - default_label_font_list.GetHeight();
    context_message_view_ =
        new BoundedLabel(gfx::TruncateString(notification.context_message(),
                                            kContextMessageCharacterLimit),
                         default_label_font_list);
    context_message_view_->SetLineLimit(
        message_center::kContextMessageLineLimit);
    context_message_view_->SetLineHeight(kMessageLineHeight);
    context_message_view_->SetColors(message_center::kDimTextColor,
                                     kContextTextBackgroundColor);
    context_message_view_->set_border(MakeTextBorder(padding, 4, 0));
    top_view_->AddChildView(context_message_view_);
    accessible_lines.push_back(notification.context_message());
  }

  // Create the progress bar view.
  progress_bar_view_ = NULL;
  if (notification.type() == NOTIFICATION_TYPE_PROGRESS) {
    progress_bar_view_ = new NotificationProgressBar();
    progress_bar_view_->set_border(MakeProgressBarBorder(
        message_center::kProgressBarTopPadding, kProgressBarBottomPadding));
    progress_bar_view_->SetValue(notification.progress() / 100.0);
    top_view_->AddChildView(progress_bar_view_);
  }

  // Create the list item views (up to a maximum).
  int padding = kMessageLineHeight - default_label_font_list.GetHeight();
  std::vector<NotificationItem> items = notification.items();
  for (size_t i = 0; i < items.size() && i < kNotificationMaximumItems; ++i) {
    ItemView* item_view = new ItemView(items[i]);
    item_view->SetVisible(is_expanded());
    item_view->set_border(MakeTextBorder(padding, i ? 0 : 4, 0));
    item_views_.push_back(item_view);
    top_view_->AddChildView(item_view);
    accessible_lines.push_back(
        items[i].title + base::ASCIIToUTF16(" ") + items[i].message);
  }

  // Create the notification icon view.
  gfx::ImageSkia icon = notification.icon().AsImageSkia();
  if (notification.type() == NOTIFICATION_TYPE_SIMPLE &&
      (icon.width() != kIconSize ||
       icon.height() != kIconSize ||
       HasAlpha(icon, GetWidget()))) {
    views::ImageView* icon_view = new views::ImageView();
    icon_view->SetImage(icon);
    icon_view->SetImageSize(gfx::Size(kLegacyIconSize, kLegacyIconSize));
    icon_view->SetHorizontalAlignment(views::ImageView::CENTER);
    icon_view->SetVerticalAlignment(views::ImageView::CENTER);
    icon_view->set_background(MakeBackground(kLegacyIconBackgroundColor));
    icon_view_ = icon_view;
  } else {
    icon_view_ = new ProportionalImageView(icon);
  }

  // Create the bottom_view_, which collects into a vertical box all content
  // below the notification icon except for the expand button.
  bottom_view_ = new views::View();
  bottom_view_->SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));

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
    separator->set_border(MakeSeparatorBorder(1, 0, kButtonSeparatorColor));
    bottom_view_->AddChildView(separator);
    NotificationButton* button = new NotificationButton(this);
    ButtonInfo button_info = buttons[i];
    button->SetTitle(button_info.title);
    button->SetIcon(button_info.icon.AsImageSkia());
    action_buttons_.push_back(button);
    bottom_view_->AddChildView(button);
  }

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
  set_accessible_name(JoinString(accessible_lines, '\n'));
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
  // Get the height assuming no line limit changes.
  int content_width = width - GetInsets().width();
  int top_height = top_view_->GetHeightForWidth(content_width);
  int bottom_height = bottom_view_->GetHeightForWidth(content_width);
  int content_height = std::max(top_height, kIconSize) + bottom_height;

  // <http://crbug.com/230448> Fix: Adjust the height when the message_view's
  // line limit would be different for the specified width than it currently is.
  // TODO(dharcourt): Avoid BoxLayout and directly compute the correct height.
  if (message_view_) {
    int used_limit = message_view_->GetLineLimit();
    int correct_limit = GetMessageLineLimit(width);
    if (used_limit != correct_limit) {
      content_height -= GetMessageHeight(content_width, used_limit);
      content_height += GetMessageHeight(content_width, correct_limit);
    }
  }

  // Adjust the height to make sure there is at least 16px of space below the
  // icon if there is any space there (<http://crbug.com/232966>).
  if (content_height > kIconSize)
    content_height = std::max(content_height,
                              kIconSize + message_center::kIconBottomPadding);

  return content_height + GetInsets().height();
}

void NotificationView::Layout() {
  gfx::Insets insets = GetInsets();
  int content_width = width() - insets.width();
  int content_right = width() - insets.right();

  // Before any resizing, set or adjust the number of message lines.
  if (message_view_)
    message_view_->SetLineLimit(GetMessageLineLimit(width()));

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

  // Expand button.
  gfx::Size expand_size(expand_button()->GetPreferredSize());
  int expand_y = bottom_y - expand_size.height();
  expand_button()->SetVisible(IsExpansionNeeded(width()));
  expand_button()->SetBounds(content_right - expand_size.width(), expand_y,
                             expand_size.width(), expand_size.height());
}

void NotificationView::OnFocus() {
  MessageView::OnFocus();
  ScrollRectToVisible(GetLocalBounds());
}

void NotificationView::ScrollRectToVisible(const gfx::Rect& rect) {
  // Notification want to show the whole notification when a part of it (like
  // a button) gets focused.
  views::View::ScrollRectToVisible(GetLocalBounds());
}

views::View* NotificationView::GetEventHandlerForPoint(
    const gfx::Point& point) {
  // Want to return this for underlying views, otherwise GetCursor is not
  // called. But buttons are exceptions, they'll have their own event handlings.
  std::vector<views::View*> buttons(action_buttons_);
  buttons.push_back(close_button());
  buttons.push_back(expand_button());

  for (size_t i = 0; i < buttons.size(); ++i) {
    gfx::Point point_in_child = point;
    ConvertPointToTarget(this, buttons[i], &point_in_child);
    if (buttons[i]->HitTestPoint(point_in_child))
      return buttons[i]->GetEventHandlerForPoint(point_in_child);
  }

  return this;
}

gfx::NativeCursor NotificationView::GetCursor(const ui::MouseEvent& event) {
  if (!message_center()->HasClickedListener(notification_id()))
    return views::View::GetCursor(event);

#if defined(USE_AURA)
  return ui::kCursorHand;
#elif defined(OS_WIN)
  static HCURSOR g_hand_cursor = LoadCursor(NULL, IDC_HAND);
  return g_hand_cursor;
#endif
}

void NotificationView::ButtonPressed(views::Button* sender,
                                     const ui::Event& event) {
  // See if the button pressed was an action button.
  for (size_t i = 0; i < action_buttons_.size(); ++i) {
    if (sender == action_buttons_[i]) {
      message_center()->ClickOnNotificationButton(notification_id(), i);
      return;
    }
  }

  // Adjust notification subviews for expansion.
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

    return;
  }

  // Let the superclass handled anything other than action buttons.
  // Warning: This may cause the NotificationView itself to be deleted,
  // so don't do anything afterwards.
  MessageView::ButtonPressed(sender, event);
}

bool NotificationView::IsExpansionNeeded(int width) {
  return (!is_expanded() &&
              (image_view_ ||
                  item_views_.size() ||
                  IsMessageExpansionNeeded(width)));
}

bool NotificationView::IsMessageExpansionNeeded(int width) {
  int current = GetMessageLines(width, GetMessageLineLimit(width));
  int expanded = GetMessageLines(width,
                                 message_center::kMessageExpandedLineLimit);
  return current < expanded;
}

int NotificationView::GetMessageLineLimit(int width) {
  // Expanded notifications get a larger limit, except for image notifications,
  // whose images must be kept flush against their icons.
  if (is_expanded() && !image_view_)
    return message_center::kMessageExpandedLineLimit;

  // If there's a title ensure title + message lines <= collapsed line limit.
  if (title_view_) {
    int title_lines = title_view_->GetLinesForWidthAndLimit(width, -1);
    return std::max(message_center::kMessageCollapsedLineLimit - title_lines,
                    0);
  }

  return message_center::kMessageCollapsedLineLimit;
}

int NotificationView::GetMessageLines(int width, int limit) {
  return message_view_ ?
         message_view_->GetLinesForWidthAndLimit(width, limit) : 0;
}

int NotificationView::GetMessageHeight(int width, int limit) {
  return message_view_ ?
         message_view_->GetSizeForWidthAndLines(width, limit).height() : 0;
}

}  // namespace message_center
