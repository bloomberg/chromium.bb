// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view_md.h"

#include <stddef.h>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
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
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
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
constexpr gfx::Insets kActionButtonPadding(0, 12, 0, 12);
constexpr gfx::Insets kStatusTextPadding(4, 0, 0, 0);
constexpr gfx::Size kActionButtonMinSize(88, 32);
constexpr gfx::Size kIconViewSize(30, 30);

// Foreground of small icon image.
constexpr SkColor kSmallImageBackgroundColor = SK_ColorWHITE;
// Background of small icon image.
const SkColor kSmallImageColor = SkColorSetRGB(0x43, 0x43, 0x43);
// Background of inline actions area.
const SkColor kActionsRowBackgroundColor = SkColorSetRGB(0xee, 0xee, 0xee);
// Base ink drop color of action buttons.
const SkColor kActionButtonInkDropBaseColor = SkColorSetRGB(0x0, 0x0, 0x0);
// Ripple ink drop opacity of action buttons.
const float kActionButtonInkDropRippleVisibleOpacity = 0.08f;
// Highlight (hover) ink drop opacity of action buttons.
const float kActionButtonInkDropHighlightVisibleOpacity = 0.08f;
// Text color of action button.
const SkColor kActionButtonTextColor = SkColorSetRGB(0x33, 0x67, 0xD6);

// Max number of lines for message_view_.
constexpr int kMaxLinesForMessageView = 1;
constexpr int kMaxLinesForExpandedMessageView = 4;

constexpr int kCompactTitleMessageViewSpacing = 12;

constexpr int kProgressBarHeight = 4;

constexpr int kMessageViewWidth =
    message_center::kNotificationWidth - kIconViewSize.width() -
    kContentRowPadding.left() - kContentRowPadding.right();

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

// ItemView ////////////////////////////////////////////////////////////////////

// ItemViews are responsible for drawing each list notification item's title and
// message next to each other within a single column.
class ItemView : public views::View {
 public:
  explicit ItemView(const message_center::NotificationItem& item);
  ~ItemView() override;

  const char* GetClassName() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ItemView);
};

ItemView::ItemView(const message_center::NotificationItem& item) {
  SetLayoutManager(
      new views::BoxLayout(views::BoxLayout::kHorizontal, gfx::Insets(), 0));

  views::Label* title = new views::Label(item.title);
  title->set_collapse_when_hidden(true);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetEnabledColor(message_center::kRegularTextColor);
  title->SetBackgroundColor(message_center::kDimTextBackgroundColor);
  AddChildView(title);

  views::Label* message = new views::Label(l10n_util::GetStringFUTF16(
      IDS_MESSAGE_CENTER_LIST_NOTIFICATION_MESSAGE_WITH_DIVIDER, item.message));
  message->set_collapse_when_hidden(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetEnabledColor(message_center::kDimTextColor);
  message->SetBackgroundColor(message_center::kDimTextBackgroundColor);
  AddChildView(message);
}

ItemView::~ItemView() = default;

const char* ItemView::GetClassName() const {
  return "ItemView";
}

// CompactTitleMessageView /////////////////////////////////////////////////////

// CompactTitleMessageView shows notification title and message in a single
// line. This view is used for NOTIFICATION_TYPE_PROGRESS.
class CompactTitleMessageView : public views::View {
 public:
  explicit CompactTitleMessageView();
  ~CompactTitleMessageView() override;

  const char* GetClassName() const override;

  void OnPaint(gfx::Canvas* canvas) override;

  void set_title(const base::string16& title) { title_ = title; }
  void set_message(const base::string16& message) { message_ = message; }

 private:
  DISALLOW_COPY_AND_ASSIGN(CompactTitleMessageView);

  base::string16 title_;
  base::string16 message_;

  views::Label* title_view_ = nullptr;
  views::Label* message_view_ = nullptr;
};

CompactTitleMessageView::~CompactTitleMessageView() = default;

const char* CompactTitleMessageView::GetClassName() const {
  return "CompactTitleMessageView";
}

CompactTitleMessageView::CompactTitleMessageView() {
  SetLayoutManager(new views::FillLayout());

  const gfx::FontList& font_list = views::Label().font_list().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);

  title_view_ = new views::Label();
  title_view_->SetFontList(font_list);
  title_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_view_->SetEnabledColor(message_center::kRegularTextColor);
  AddChildView(title_view_);

  message_view_ = new views::Label();
  message_view_->SetFontList(font_list);
  message_view_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  message_view_->SetEnabledColor(message_center::kDimTextColor);
  AddChildView(message_view_);
}

void CompactTitleMessageView::OnPaint(gfx::Canvas* canvas) {
  base::string16 title = title_;
  base::string16 message = message_;

  const gfx::FontList& font_list = views::Label().font_list().Derive(
      1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);

  // Elides title and message. The behavior is based on Android's one.
  // * If the title is too long, only the title is shown.
  // * If the message is too long, the full content of the title is shown,
  //   kCompactTitleMessageViewSpacing is added between them, and the elided
  //   message is shown.
  // * If they are short enough, the title is left-aligned and the message is
  //   right-aligned.
  const int original_title_width =
      gfx::Canvas::GetStringWidthF(title, font_list);
  if (original_title_width >= width())
    message.clear();
  title = gfx::ElideText(title, font_list, width(), gfx::ELIDE_TAIL);
  const int title_width = gfx::Canvas::GetStringWidthF(title, font_list);
  const int message_width =
      std::max(0, width() - title_width - kCompactTitleMessageViewSpacing);
  message = gfx::ElideText(message, font_list, message_width, gfx::ELIDE_TAIL);

  title_view_->SetText(title);
  message_view_->SetText(message);

  views::View::OnPaint(canvas);
}

// NotificationButtonMD ////////////////////////////////////////////////////////

// This class is needed in addition to LabelButton mainly becuase we want to set
// visible_opacity of InkDropHighlight.
// This button capitalizes the given label string.
class NotificationButtonMD : public views::LabelButton {
 public:
  NotificationButtonMD(views::ButtonListener* listener,
                       const base::string16& text);
  ~NotificationButtonMD() override;

  void SetText(const base::string16& text) override;
  const char* GetClassName() const override;

  std::unique_ptr<views::InkDropHighlight> CreateInkDropHighlight()
      const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NotificationButtonMD);
};

NotificationButtonMD::NotificationButtonMD(views::ButtonListener* listener,
                                           const base::string16& text)
    : views::LabelButton(listener,
                         base::i18n::ToUpper(text),
                         views::style::CONTEXT_BUTTON_MD) {
  SetHorizontalAlignment(gfx::ALIGN_CENTER);
  SetInkDropMode(views::LabelButton::InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_ink_drop_base_color(kActionButtonInkDropBaseColor);
  set_ink_drop_visible_opacity(kActionButtonInkDropRippleVisibleOpacity);
  SetEnabledTextColors(kActionButtonTextColor);
  SetBorder(views::CreateEmptyBorder(kActionButtonPadding));
  SetMinSize(kActionButtonMinSize);
  SetFocusForPlatform();
}

NotificationButtonMD::~NotificationButtonMD() = default;

void NotificationButtonMD::SetText(const base::string16& text) {
  views::LabelButton::SetText(base::i18n::ToUpper(text));
}

const char* NotificationButtonMD::GetClassName() const {
  return "NotificationButtonMD";
}

std::unique_ptr<views::InkDropHighlight>
NotificationButtonMD::CreateInkDropHighlight() const {
  std::unique_ptr<views::InkDropHighlight> highlight =
      views::LabelButton::CreateInkDropHighlight();
  highlight->set_visible_opacity(kActionButtonInkDropHighlightVisibleOpacity);
  return highlight;
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
  CreateOrUpdateCompactTitleMessageView(notification);
  CreateOrUpdateProgressBarView(notification);
  CreateOrUpdateProgressStatusView(notification);
  CreateOrUpdateListItemViews(notification);
  CreateOrUpdateIconView(notification);
  CreateOrUpdateSmallIconView(notification);
  CreateOrUpdateImageView(notification);
  CreateOrUpdateCloseButtonView(notification);
  CreateOrUpdateSettingsButtonView(notification);
  UpdateViewForExpandedState(expanded_);
  // Should be called at the last because SynthesizeMouseMoveEvent() requires
  // everything is in the right location when called.
  CreateOrUpdateActionButtonViews(notification);
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
  content_row_->AddChildView(left_content_);
  content_row_layout->SetFlexForView(left_content_, 1);

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
  if (IsExpandable() && sender == header_row_) {
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
  header_row_->SetTimestamp(notification.timestamp());
}

void NotificationViewMD::CreateOrUpdateTitleView(
    const Notification& notification) {
  if (notification.title().empty() ||
      notification.type() == NOTIFICATION_TYPE_PROGRESS) {
    DCHECK(!title_view_ || left_content_->Contains(title_view_));
    delete title_view_;
    title_view_ = nullptr;
    return;
  }
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
  if (notification.type() == NOTIFICATION_TYPE_PROGRESS ||
      notification.message().empty()) {
    // Deletion will also remove |message_view_| from its parent.
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

    // TODO(tetsui): Workaround https://crbug.com/682266 by explicitly setting
    // the width.
    // Ideally, we should fix the original bug, but it seems there's no obvious
    // solution for the bug according to https://crbug.com/678337#c7, we should
    // ensure that the change won't break any of the users of BoxLayout class.
    DCHECK(right_content_);
    message_view_->SizeToFit(kMessageViewWidth);

    left_content_->AddChildView(message_view_);
  } else {
    message_view_->SetText(text);
  }

  message_view_->SetVisible(notification.items().empty());
}

void NotificationViewMD::CreateOrUpdateCompactTitleMessageView(
    const Notification& notification) {
  if (notification.type() != NOTIFICATION_TYPE_PROGRESS) {
    DCHECK(!compact_title_message_view_ ||
           left_content_->Contains(compact_title_message_view_));
    delete compact_title_message_view_;
    compact_title_message_view_ = nullptr;
    return;
  }
  if (!compact_title_message_view_) {
    compact_title_message_view_ = new CompactTitleMessageView();
    left_content_->AddChildView(compact_title_message_view_);
  }

  compact_title_message_view_->set_title(notification.title());
  compact_title_message_view_->set_message(notification.message());
  left_content_->InvalidateLayout();
}

void NotificationViewMD::CreateOrUpdateProgressBarView(
    const Notification& notification) {
  if (notification.type() != NOTIFICATION_TYPE_PROGRESS) {
    DCHECK(!progress_bar_view_ || left_content_->Contains(progress_bar_view_));
    delete progress_bar_view_;
    progress_bar_view_ = nullptr;
    header_row_->ClearProgress();
    return;
  }

  DCHECK(left_content_);

  if (!progress_bar_view_) {
    progress_bar_view_ = new views::ProgressBar(kProgressBarHeight,
                                                /* allow_round_corner */ false);
    progress_bar_view_->SetBorder(views::CreateEmptyBorder(
        message_center::kProgressBarTopPadding, 0, 0, 0));
    left_content_->AddChildView(progress_bar_view_);
  }

  progress_bar_view_->SetValue(notification.progress() / 100.0);
  progress_bar_view_->SetVisible(notification.items().empty());

  header_row_->SetProgress(notification.progress());
}

void NotificationViewMD::CreateOrUpdateProgressStatusView(
    const Notification& notification) {
  if (notification.type() != NOTIFICATION_TYPE_PROGRESS ||
      notification.progress_status().empty()) {
    if (!status_view_)
      return;
    DCHECK(left_content_->Contains(status_view_));
    delete status_view_;
    status_view_ = nullptr;
    return;
  }

  if (!status_view_) {
    const gfx::FontList& font_list = views::Label().font_list().Derive(
        1, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
    status_view_ = new views::Label();
    status_view_->SetFontList(font_list);
    status_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    status_view_->SetEnabledColor(message_center::kDimTextColor);
    status_view_->SetBorder(views::CreateEmptyBorder(kStatusTextPadding));
    left_content_->AddChildView(status_view_);
  }

  status_view_->SetText(notification.progress_status());
}

void NotificationViewMD::CreateOrUpdateListItemViews(
    const Notification& notification) {
  for (auto* item_view : item_views_)
    delete item_view;
  item_views_.clear();

  const std::vector<NotificationItem>& items = notification.items();

  for (size_t i = 0; i < items.size() && i < kMaxLinesForExpandedMessageView;
       ++i) {
    ItemView* item_view = new ItemView(items[i]);
    item_views_.push_back(item_view);
    left_content_->AddChildView(item_view);
  }

  list_items_count_ = items.size();

  // Needed when CreateOrUpdateViews is called for update.
  if (!item_views_.empty())
    left_content_->InvalidateLayout();
}

void NotificationViewMD::CreateOrUpdateIconView(
    const Notification& notification) {
  if (notification.type() == NOTIFICATION_TYPE_PROGRESS ||
      notification.type() == NOTIFICATION_TYPE_MULTIPLE) {
    DCHECK(!icon_view_ || right_content_->Contains(icon_view_));
    delete icon_view_;
    icon_view_ = nullptr;
    return;
  }

  if (!icon_view_) {
    icon_view_ = new ProportionalImageView(kIconViewSize);
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
    if (image_container_) {
      DCHECK(image_view_);
      DCHECK(Contains(image_container_));
      delete image_container_;
      image_container_ = NULL;
      image_view_ = NULL;
    } else {
      DCHECK(!image_view_);
    }
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

    DCHECK(!image_view_);
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
      NotificationButtonMD* button =
          new NotificationButtonMD(this, button_info.title);
      action_buttons_.push_back(button);
      actions_row_->AddChildView(button);
    } else {
      action_buttons_[i]->SetText(button_info.title);
      action_buttons_[i]->SchedulePaint();
      action_buttons_[i]->Layout();
    }
  }

  // Inherit mouse hover state when action button views reset.
  // If the view is not expanded, there should be no hover state.
  if (new_buttons && expanded_) {
    views::Widget* widget = GetWidget();
    if (widget) {
      // This Layout() is needed because button should be in the right location
      // in the view hierarchy when SynthesizeMouseMoveEvent() is called.
      Layout();
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

  // Expandable if there are multiple list items.
  if (item_views_.size() > 1)
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
  for (size_t i = kMaxLinesForMessageView; i < item_views_.size(); ++i) {
    item_views_[i]->SetVisible(expanded);
  }
  if (status_view_)
    status_view_->SetVisible(expanded);
  header_row_->SetOverflowIndicator(
      list_items_count_ -
      (expanded ? item_views_.size() : kMaxLinesForMessageView));
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

NotificationControlButtonsView* NotificationViewMD::GetControlButtonsView()
    const {
  // TODO(yoshiki): have this view use NotificationControlButtonsView.
  return nullptr;
}

}  // namespace message_center
