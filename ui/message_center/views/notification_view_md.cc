// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_view_md.h"

#include <stddef.h>
#include <memory>

#include "base/i18n/case_conversion.h"
#include "base/strings/string_util.h"
#include "components/url_formatter/elide_url.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/skia_util.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/notification.h"
#include "ui/message_center/notification_types.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/bounded_label.h"
#include "ui/message_center/views/constants.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/message_center/views/notification_header_view.h"
#include "ui/message_center/views/padded_button.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/radio_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/native_cursor.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace message_center {

namespace {

// Dimensions.
constexpr gfx::Insets kContentRowPadding(0, 12, 16, 12);
constexpr gfx::Insets kActionsRowPadding(8, 8, 8, 8);
constexpr int kActionsRowHorizontalSpacing = 8;
constexpr gfx::Insets kActionButtonPadding(0, 12, 0, 12);
constexpr gfx::Insets kStatusTextPadding(4, 0, 0, 0);
constexpr gfx::Size kActionButtonMinSize(0, 32);
// TODO(tetsui): Move |kIconViewSize| to public/cpp/message_center_constants.h
// and merge with contradicting |kNotificationIconSize|.
constexpr gfx::Size kIconViewSize(36, 36);
constexpr gfx::Insets kLargeImageContainerPadding(0, 12, 12, 12);
constexpr gfx::Size kLargeImageMinSize(328, 0);
constexpr gfx::Size kLargeImageMaxSize(328, 218);
constexpr gfx::Insets kLeftContentPadding(2, 4, 0, 4);
constexpr gfx::Insets kLeftContentPaddingWithIcon(2, 4, 0, 12);
constexpr gfx::Insets kNotificationInputPadding(0, 16, 0, 16);
constexpr gfx::Insets kSettingsRowPadding(8, 0, 0, 0);
constexpr gfx::Insets kSettingsRadioButtonPadding(14, 18, 14, 18);
constexpr gfx::Insets kSettingsButtonRowPadding(8);

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
// Background color of the large image.
const SkColor kLargeImageBackgroundColor = SkColorSetRGB(0xf5, 0xf5, 0xf5);

const SkColor kRegularTextColorMD = SkColorSetRGB(0x21, 0x21, 0x21);
const SkColor kDimTextColorMD = SkColorSetRGB(0x75, 0x75, 0x75);

// The text color and the background color of inline reply input field.
const SkColor kInputTextColor = SkColorSetRGB(0xFF, 0xFF, 0xFF);
const SkColor kInputPlaceholderColor = SkColorSetARGB(0x8A, 0xFF, 0xFF, 0xFF);
const SkColor kInputBackgroundColor = SkColorSetRGB(0x33, 0x67, 0xD6);

// Max number of lines for message_view_.
constexpr int kMaxLinesForMessageView = 1;
constexpr int kMaxLinesForExpandedMessageView = 4;

constexpr int kCompactTitleMessageViewSpacing = 12;

constexpr int kProgressBarHeight = 4;

constexpr int kMessageViewWidthWithIcon =
    message_center::kNotificationWidth - kIconViewSize.width() -
    kLeftContentPaddingWithIcon.left() - kLeftContentPaddingWithIcon.right() -
    kContentRowPadding.left() - kContentRowPadding.right();

constexpr int kMessageViewWidth =
    message_center::kNotificationWidth - kLeftContentPadding.left() -
    kLeftContentPadding.right() - kContentRowPadding.left() -
    kContentRowPadding.right();

// "Roboto-Regular, 13sp" is specified in the mock.
constexpr int kTextFontSize = 13;

// In progress notification, if both the title and the message are long, the
// message would be prioritized and the title would be elided.
// However, it is not perferable that we completely omit the title, so
// the ratio of the message width is limited to this value.
constexpr double kProgressNotificationMessageRatio = 0.7;

// FontList for the texts except for the header.
gfx::FontList GetTextFontList() {
  gfx::Font default_font;
  int font_size_delta = kTextFontSize - default_font.GetFontSize();
  gfx::Font font = default_font.Derive(font_size_delta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::NORMAL);
  DCHECK_EQ(kTextFontSize, font.GetFontSize());
  return gfx::FontList(font);
}

class ClickActivator : public ui::EventHandler {
 public:
  explicit ClickActivator(NotificationViewMD* owner) : owner_(owner) {}
  ~ClickActivator() override = default;

 private:
  // ui::EventHandler
  void OnEvent(ui::Event* event) override {
    if (event->type() == ui::ET_MOUSE_PRESSED ||
        event->type() == ui::ET_GESTURE_TAP) {
      owner_->Activate();
    }
  }

  NotificationViewMD* const owner_;

  DISALLOW_COPY_AND_ASSIGN(ClickActivator);
};

}  // anonymous namespace

// ItemView ////////////////////////////////////////////////////////////////////

ItemView::ItemView(const message_center::NotificationItem& item) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, gfx::Insets(), 0));

  const gfx::FontList font_list = GetTextFontList();

  views::Label* title = new views::Label(item.title);
  title->SetFontList(font_list);
  title->set_collapse_when_hidden(true);
  title->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title->SetEnabledColor(message_center::kRegularTextColorMD);
  title->SetBackgroundColor(message_center::kDimTextBackgroundColor);
  AddChildView(title);

  views::Label* message = new views::Label(l10n_util::GetStringFUTF16(
      IDS_MESSAGE_CENTER_LIST_NOTIFICATION_MESSAGE_WITH_DIVIDER, item.message));
  message->SetFontList(font_list);
  message->set_collapse_when_hidden(true);
  message->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  message->SetEnabledColor(kDimTextColorMD);
  message->SetBackgroundColor(message_center::kDimTextBackgroundColor);
  AddChildView(message);
}

ItemView::~ItemView() = default;

const char* ItemView::GetClassName() const {
  return "ItemView";
}

// CompactTitleMessageView /////////////////////////////////////////////////////

CompactTitleMessageView::~CompactTitleMessageView() = default;

const char* CompactTitleMessageView::GetClassName() const {
  return "CompactTitleMessageView";
}

CompactTitleMessageView::CompactTitleMessageView() {
  SetLayoutManager(std::make_unique<views::FillLayout>());

  const gfx::FontList& font_list = GetTextFontList();

  title_view_ = new views::Label();
  title_view_->SetFontList(font_list);
  title_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_view_->SetEnabledColor(kRegularTextColorMD);
  AddChildView(title_view_);

  message_view_ = new views::Label();
  message_view_->SetFontList(font_list);
  message_view_->SetHorizontalAlignment(gfx::ALIGN_RIGHT);
  message_view_->SetEnabledColor(kDimTextColorMD);
  AddChildView(message_view_);
}

void CompactTitleMessageView::OnPaint(gfx::Canvas* canvas) {
  base::string16 title = title_;
  base::string16 message = message_;

  const gfx::FontList& font_list = GetTextFontList();

  // Elides title and message.
  // * If the message is too long, the message occupies at most
  //   kProgressNotificationMessageRatio of the width.
  // * If the title is too long, the full content of the message is shown,
  //   kCompactTitleMessageViewSpacing is added between them, and the elided
  //   title is shown.
  // * If they are short enough, the title is left-aligned and the message is
  //   right-aligned.
  message = gfx::ElideText(
      message, font_list,
      title.empty()
          ? width()
          : static_cast<int>(kProgressNotificationMessageRatio * width()),
      gfx::ELIDE_TAIL);
  const int message_width = gfx::Canvas::GetStringWidthF(message, font_list);
  const int title_width =
      std::max(0, width() - message_width - kCompactTitleMessageViewSpacing);
  title = gfx::ElideText(title, font_list, title_width, gfx::ELIDE_TAIL);

  title_view_->SetText(title);
  message_view_->SetText(message);

  views::View::OnPaint(canvas);
}

// LargeImageView //////////////////////////////////////////////////////////////

LargeImageView::LargeImageView() {
  SetBackground(views::CreateSolidBackground(kLargeImageBackgroundColor));
}

LargeImageView::~LargeImageView() = default;

void LargeImageView::SetImage(const gfx::ImageSkia& image) {
  image_ = image;
  gfx::Size preferred_size = GetResizedImageSize();
  preferred_size.SetToMax(kLargeImageMinSize);
  preferred_size.SetToMin(kLargeImageMaxSize);
  SetPreferredSize(preferred_size);
  SchedulePaint();
  Layout();
}

void LargeImageView::OnPaint(gfx::Canvas* canvas) {
  views::View::OnPaint(canvas);

  gfx::Size resized_size = GetResizedImageSize();
  gfx::Size drawn_size = resized_size;
  drawn_size.SetToMin(kLargeImageMaxSize);
  gfx::Rect drawn_bounds = GetContentsBounds();
  drawn_bounds.ClampToCenteredSize(drawn_size);

  gfx::ImageSkia resized_image = gfx::ImageSkiaOperations::CreateResizedImage(
      image_, skia::ImageOperations::RESIZE_BEST, resized_size);

  // Cut off the overflown part.
  gfx::ImageSkia drawn_image = gfx::ImageSkiaOperations::ExtractSubset(
      resized_image, gfx::Rect(drawn_size));

  canvas->DrawImageInt(drawn_image, drawn_bounds.x(), drawn_bounds.y());
}

const char* LargeImageView::GetClassName() const {
  return "LargeImageView";
}

// Returns expected size of the image right after resizing.
// The GetResizedImageSize().width() <= kLargeImageMaxSize.width() holds, but
// GetResizedImageSize().height() may be larger than kLargeImageMaxSize.height()
// In this case, the overflown part will be just cutted off from the view.
gfx::Size LargeImageView::GetResizedImageSize() {
  gfx::Size original_size = image_.size();
  if (original_size.width() <= kLargeImageMaxSize.width())
    return image_.size();

  const double proportion =
      original_size.height() / static_cast<double>(original_size.width());
  gfx::Size resized_size;
  resized_size.SetSize(kLargeImageMaxSize.width(),
                       kLargeImageMaxSize.width() * proportion);
  return resized_size;
}

// LargeImageContainerView /////////////////////////////////////////////////////

LargeImageContainerView::LargeImageContainerView()
    : image_view_(new LargeImageView()) {
  SetLayoutManager(std::make_unique<views::FillLayout>());
  SetBorder(views::CreateEmptyBorder(kLargeImageContainerPadding));
  SetBackground(
      views::CreateSolidBackground(message_center::kImageBackgroundColor));
  AddChildView(image_view_);
}

LargeImageContainerView::~LargeImageContainerView() = default;

void LargeImageContainerView::SetImage(const gfx::ImageSkia& image) {
  image_view_->SetImage(image);
}

const char* LargeImageContainerView::GetClassName() const {
  return "LargeImageContainerView";
}

// NotificationButtonMD ////////////////////////////////////////////////////////

NotificationButtonMD::NotificationButtonMD(views::ButtonListener* listener,
                                           bool is_inline_reply,
                                           const base::string16& label,
                                           const base::string16& placeholder)
    : views::LabelButton(listener,
                         base::i18n::ToUpper(label),
                         views::style::CONTEXT_BUTTON_MD),
      is_inline_reply_(is_inline_reply),
      placeholder_(placeholder) {
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

// NotificationInputMD /////////////////////////////////////////////////////////

NotificationInputMD::NotificationInputMD(NotificationInputDelegate* delegate)
    : delegate_(delegate), index_(0) {
  set_controller(this);
  SetTextColor(kInputTextColor);
  SetBackgroundColor(kInputBackgroundColor);
  set_placeholder_text_color(kInputPlaceholderColor);
  SetBorder(views::CreateEmptyBorder(kNotificationInputPadding));
}

NotificationInputMD::~NotificationInputMD() = default;

bool NotificationInputMD::HandleKeyEvent(views::Textfield* sender,
                                         const ui::KeyEvent& event) {
  if (event.type() == ui::ET_KEY_PRESSED &&
      event.key_code() == ui::VKEY_RETURN) {
    delegate_->OnNotificationInputSubmit(index_, text());
    return true;
  }
  return event.type() == ui::ET_KEY_RELEASED;
}

void NotificationInputMD::set_placeholder(const base::string16& placeholder) {
  if (placeholder.empty()) {
    set_placeholder_text(l10n_util::GetStringUTF16(
        IDS_MESSAGE_CENTER_NOTIFICATION_INLINE_REPLY_PLACEHOLDER));
  } else {
    set_placeholder_text(placeholder);
  }
}

// InlineSettingsRadioButton ///////////////////////////////////////////////////

class InlineSettingsRadioButton : public views::RadioButton {
 public:
  InlineSettingsRadioButton(const base::string16& label_text)
      : views::RadioButton(label_text, 1 /* group */, true /* force_md */) {
    label()->SetFontList(GetTextFontList());
    label()->SetEnabledColor(kRegularTextColorMD);
  }
};

// ////////////////////////////////////////////////////////////
// NotificationViewMD
// ////////////////////////////////////////////////////////////

views::View* NotificationViewMD::TargetForRect(views::View* root,
                                               const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  // TODO(tetsui): Modify this function to support rect-based event
  // targeting. Using the center point of |rect| preserves this function's
  // expected behavior for the time being.
  gfx::Point point = rect.CenterPoint();

  // Want to return this for underlying views, otherwise GetCursor is not
  // called. But buttons are exceptions, they'll have their own event handlings.
  std::vector<views::View*> buttons;
  if (header_row_->expand_button())
    buttons.push_back(header_row_->expand_button());
  buttons.push_back(header_row_);

  if (action_buttons_row_->visible()) {
    buttons.insert(buttons.end(), action_buttons_.begin(),
                   action_buttons_.end());
  }
  if (inline_reply_->visible())
    buttons.push_back(inline_reply_);
  if (settings_row_) {
    buttons.push_back(block_all_button_);
    buttons.push_back(dont_block_button_);
    buttons.push_back(settings_done_button_);
  }

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
  CreateOrUpdateInlineSettingsViews(notification);
  UpdateViewForExpandedState(expanded_);
  // Should be called at the last because SynthesizeMouseMoveEvent() requires
  // everything is in the right location when called.
  CreateOrUpdateActionButtonViews(notification);
}

NotificationViewMD::NotificationViewMD(const Notification& notification)
    : MessageView(notification), clickable_(notification.clickable()) {
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), 0));

  control_buttons_view_ =
      std::make_unique<NotificationControlButtonsView>(this);
  control_buttons_view_->set_owned_by_client();
  control_buttons_view_->SetBackgroundColor(SK_ColorTRANSPARENT);

  // |header_row_| contains app_icon, app_name, control buttons, etc...
  header_row_ = new NotificationHeaderView(control_buttons_view_.get(), this);
  AddChildView(header_row_);

  // |content_row_| contains title, message, image, progressbar, etc...
  content_row_ = new views::View();
  auto* content_row_layout =
      content_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::kHorizontal, kContentRowPadding, 0));
  content_row_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  AddChildView(content_row_);

  // |left_content_| contains most contents like title, message, etc...
  left_content_ = new views::View();
  left_content_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, gfx::Insets(), 0));
  left_content_->SetBorder(views::CreateEmptyBorder(kLeftContentPadding));
  content_row_->AddChildView(left_content_);
  content_row_layout->SetFlexForView(left_content_, 1);

  // |right_content_| contains notification icon and small image.
  right_content_ = new views::View();
  right_content_->SetLayoutManager(std::make_unique<views::FillLayout>());
  content_row_->AddChildView(right_content_);

  // |action_row_| contains inline action buttons and inline textfield.
  actions_row_ = new views::View();
  actions_row_->SetVisible(false);
  actions_row_->SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(actions_row_);

  // |action_buttons_row_| contains inline action buttons.
  action_buttons_row_ = new views::View();
  action_buttons_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, kActionsRowPadding,
      kActionsRowHorizontalSpacing));
  action_buttons_row_->SetBackground(
      views::CreateSolidBackground(kActionsRowBackgroundColor));
  action_buttons_row_->SetVisible(false);
  actions_row_->AddChildView(action_buttons_row_);

  // |inline_reply_| is a textfield for inline reply.
  inline_reply_ = new NotificationInputMD(this);
  inline_reply_->SetVisible(false);

  actions_row_->AddChildView(inline_reply_);

  CreateOrUpdateViews(notification);
  UpdateControlButtonsVisibilityWithNotification(notification);

  SetEventTargeter(
      std::unique_ptr<views::ViewTargeter>(new views::ViewTargeter(this)));
  set_notify_enter_exit_on_child(true);

  click_activator_ = std::make_unique<ClickActivator>(this);
  // Reasons to use pretarget handler instead of OnMousePressed:
  // - NotificationViewMD::OnMousePresssed would not fire on the inline reply
  //   textfield click in native notification.
  // - To make it look similar to ArcNotificationContentView::EventForwarder.
  AddPreTargetHandler(click_activator_.get());
}

NotificationViewMD::~NotificationViewMD() {
  RemovePreTargetHandler(click_activator_.get());
}

void NotificationViewMD::Layout() {
  MessageView::Layout();

  // We need to call IsExpandable() at the end of Layout() call, since whether
  // we should show expand button or not depends on the current view layout.
  // (e.g. Show expand button when |message_view_| exceeds one line.)
  header_row_->SetExpandButtonEnabled(IsExpandable());
  header_row_->Layout();

  // The notification background is rounded in MessageView::Layout(),
  // but we also have to round the actions row background here.
  if (actions_row_->visible()) {
    constexpr SkScalar kCornerRadius = SkIntToScalar(kNotificationCornerRadius);

    // Use vertically larger clip path, so that actions row's top coners will
    // not be rounded.
    gfx::Path path;
    gfx::Rect bounds = actions_row_->GetLocalBounds();
    bounds.set_y(bounds.y() - bounds.height());
    bounds.set_height(bounds.height() * 2);
    path.addRoundRect(gfx::RectToSkRect(bounds), kCornerRadius, kCornerRadius);

    action_buttons_row_->set_clip_path(path);
    inline_reply_->set_clip_path(path);
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
  // Do not change the cursor on a notification that isn't clickable.
  if (!clickable_)
    return views::View::GetCursor(event);

  // Do not change the cursor on the actions row.
  if (expanded_) {
    DCHECK(actions_row_);
    gfx::Point point_in_child = event.location();
    ConvertPointToTarget(this, actions_row_, &point_in_child);
    if (actions_row_->HitTestPoint(point_in_child))
      return views::View::GetCursor(event);
  }

  // Do not change the cursor when inline settings is shown.
  if (settings_row_ && settings_row_->visible())
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

bool NotificationViewMD::OnMousePressed(const ui::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;

  // Ignore click of actions row outside action buttons.
  if (expanded_) {
    DCHECK(actions_row_);
    gfx::Point point_in_child = event.location();
    ConvertPointToTarget(this, actions_row_, &point_in_child);
    if (actions_row_->HitTestPoint(point_in_child))
      return true;
  }

  // Ignore clicks of outside region when inline settings is shown.
  if (settings_row_ && settings_row_->visible())
    return true;

  return MessageView::OnMousePressed(event);
}

void NotificationViewMD::UpdateWithNotification(
    const Notification& notification) {
  MessageView::UpdateWithNotification(notification);
  UpdateControlButtonsVisibilityWithNotification(notification);

  CreateOrUpdateViews(notification);
  Layout();
  SchedulePaint();
}

// TODO(yoshiki): Move this to the parent class (MessageView).
void NotificationViewMD::UpdateControlButtonsVisibilityWithNotification(
    const Notification& notification) {
  control_buttons_view_->ShowSettingsButton(
      notification.should_show_settings_button());
  control_buttons_view_->ShowCloseButton(!GetPinned());
  UpdateControlButtonsVisibility();
}

void NotificationViewMD::ButtonPressed(views::Button* sender,
                                       const ui::Event& event) {
  // Certain operations can cause |this| to be destructed, so copy the members
  // we send to other parts of the code.
  // TODO(dewittj): Remove this hack.
  std::string id(notification_id());

  // Tapping anywhere on |header_row_| can expand the notification, though only
  // |expand_button| can be focused by TAB.
  if (sender == header_row_) {
    if (IsExpandable()) {
      ToggleExpanded();
      Layout();
      SchedulePaint();
    }
    return;
  }

  // See if the button pressed was an action button.
  for (size_t i = 0; i < action_buttons_.size(); ++i) {
    if (sender != action_buttons_[i])
      continue;
    if (action_buttons_[i]->is_inline_reply()) {
      inline_reply_->set_index(i);
      inline_reply_->set_placeholder(action_buttons_[i]->placeholder());
      inline_reply_->SetVisible(true);
      action_buttons_row_->SetVisible(false);
      Layout();
      SchedulePaint();
    } else {
      MessageCenter::Get()->ClickOnNotificationButton(id, i);
    }
    return;
  }

  if (sender == settings_done_button_) {
    if (block_all_button_->checked())
      MessageCenter::Get()->DisableNotification(id);
    ToggleInlineSettings();
    return;
  }
}

void NotificationViewMD::OnNotificationInputSubmit(size_t index,
                                                   const base::string16& text) {
  MessageCenter::Get()->ClickOnNotificationButtonWithReply(notification_id(),
                                                           index, text);
}

bool NotificationViewMD::IsCloseButtonFocused() const {
  return control_buttons_view_->IsCloseButtonFocused();
}

void NotificationViewMD::RequestFocusOnCloseButton() {
  control_buttons_view_->RequestFocusOnCloseButton();
}

void NotificationViewMD::CreateOrUpdateContextTitleView(
    const Notification& notification) {
  header_row_->SetAccentColor(
      notification.accent_color() == SK_ColorTRANSPARENT
          ? message_center::kNotificationDefaultAccentColor
          : notification.accent_color());
  header_row_->SetTimestamp(notification.timestamp());

  base::string16 app_name = notification.display_source();
  if (notification.origin_url().is_valid() &&
      notification.origin_url().SchemeIsHTTPOrHTTPS()) {
    app_name = url_formatter::FormatUrlForSecurityDisplay(
        notification.origin_url(),
        url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
  } else if (app_name.empty() &&
             notification.notifier_id().type == NotifierId::SYSTEM_COMPONENT) {
    app_name = MessageCenter::Get()->GetSystemNotificationAppName();
  }
  header_row_->SetAppName(app_name);
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

  int title_character_limit =
      kNotificationWidth * kMaxTitleLines / kMinPixelsPerTitleCharacter;

  base::string16 title = gfx::TruncateString(
      notification.title(), title_character_limit, gfx::WORD_BREAK);
  if (!title_view_) {
    const gfx::FontList& font_list = GetTextFontList();

    title_view_ = new views::Label(title);
    title_view_->SetFontList(font_list);
    title_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    title_view_->SetEnabledColor(kRegularTextColorMD);
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

  const gfx::FontList& font_list = GetTextFontList();

  if (!message_view_) {
    message_view_ = new BoundedLabel(text, font_list);
    message_view_->SetLineLimit(kMaxLinesForMessageView);
    message_view_->SetColors(kDimTextColorMD, kContextTextBackgroundColor);

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

  if (0 <= notification.progress() && notification.progress() <= 100)
    header_row_->SetProgress(notification.progress());
  else
    header_row_->ClearProgress();
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
    const gfx::FontList& font_list = GetTextFontList();
    status_view_ = new views::Label();
    status_view_->SetFontList(font_list);
    status_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
    status_view_->SetEnabledColor(kDimTextColorMD);
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

  const bool use_image_for_icon = notification.icon().IsEmpty();
  gfx::ImageSkia icon;
  if (use_image_for_icon)
    icon = notification.image().AsImageSkia();
  else
    icon = notification.icon().AsImageSkia();
  icon_view_->SetImage(icon, icon.size());

  // Hide the icon on the right side when the notification is expanded.
  hide_icon_on_expanded_ = use_image_for_icon;
}

void NotificationViewMD::CreateOrUpdateSmallIconView(
    const Notification& notification) {
  if (notification.small_image().IsEmpty())
    header_row_->ClearAppIcon();
  else
    header_row_->SetAppIcon(notification.small_image().AsImageSkia());
}

void NotificationViewMD::CreateOrUpdateImageView(
    const Notification& notification) {
  if (notification.image().IsEmpty()) {
    if (image_container_view_) {
      DCHECK(Contains(image_container_view_));
      delete image_container_view_;
      image_container_view_ = nullptr;
    }
    return;
  }

  if (!image_container_view_) {
    image_container_view_ = new LargeImageContainerView();
    // Insert the created image container just after the |content_row_|.
    AddChildViewAt(image_container_view_, GetIndexOf(content_row_) + 1);
  }

  image_container_view_->SetImage(notification.image().AsImageSkia());
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
      bool is_inline_reply =
          button_info.type == message_center::ButtonType::TEXT;
      NotificationButtonMD* button = new NotificationButtonMD(
          this, is_inline_reply, button_info.title, button_info.placeholder);
      action_buttons_.push_back(button);
      action_buttons_row_->AddChildView(button);
    } else {
      action_buttons_[i]->SetText(button_info.title);
      action_buttons_[i]->SchedulePaint();
      action_buttons_[i]->Layout();
    }

    // Change action button color to the accent color.
    action_buttons_[i]->SetEnabledTextColors(notification.accent_color() ==
                                                     SK_ColorTRANSPARENT
                                                 ? kActionButtonTextColor
                                                 : notification.accent_color());
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

void NotificationViewMD::CreateOrUpdateInlineSettingsViews(
    const Notification& notification) {
  if (settings_row_) {
    DCHECK_EQ(SettingsButtonHandler::TRAY,
              notification.rich_notification_data().settings_button_handler);
    return;
  }

  if (notification.rich_notification_data().settings_button_handler !=
      SettingsButtonHandler::TRAY) {
    return;
  }

  // |settings_row_| contains inline settings.
  settings_row_ = new views::View();
  settings_row_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::kVertical, kSettingsRowPadding, 0));
  settings_row_->SetBackground(
      views::CreateSolidBackground(kActionsRowBackgroundColor));

  block_all_button_ = new InlineSettingsRadioButton(
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_BLOCK_ALL_NOTIFICATIONS));
  block_all_button_->set_listener(this);
  block_all_button_->SetBorder(
      views::CreateEmptyBorder(kSettingsRadioButtonPadding));
  settings_row_->AddChildView(block_all_button_);

  dont_block_button_ = new InlineSettingsRadioButton(
      l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_DONT_BLOCK_NOTIFICATIONS));
  dont_block_button_->set_listener(this);
  dont_block_button_->SetBorder(
      views::CreateEmptyBorder(kSettingsRadioButtonPadding));
  settings_row_->AddChildView(dont_block_button_);
  settings_row_->SetVisible(false);

  settings_done_button_ = new NotificationButtonMD(
      this, false, l10n_util::GetStringUTF16(IDS_MESSAGE_CENTER_SETTINGS_DONE),
      base::EmptyString16());
  auto* settings_button_row = new views::View;
  auto settings_button_layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::kHorizontal, kSettingsButtonRowPadding, 0);
  settings_button_layout->set_main_axis_alignment(
      views::BoxLayout::MAIN_AXIS_ALIGNMENT_END);
  settings_button_row->SetLayoutManager(std::move(settings_button_layout));
  settings_button_row->AddChildView(settings_done_button_);
  settings_row_->AddChildView(settings_button_row);

  AddChildViewAt(settings_row_, GetIndexOf(actions_row_));
}

bool NotificationViewMD::IsExpandable() {
  // Expandable if the message exceeds one line.
  if (message_view_ && message_view_->visible() &&
      message_view_->GetLinesForWidthAndLimit(message_view_->width(), -1) > 1) {
    return true;
  }
  // Expandable if there is at least one inline action.
  if (action_buttons_row_->has_children())
    return true;

  // Expandable if the notification has image.
  if (image_container_view_)
    return true;

  // Expandable if there are multiple list items.
  if (item_views_.size() > 1)
    return true;

  // TODO(fukino): Expandable if both progress bar and message exist.

  return false;
}

void NotificationViewMD::ToggleExpanded() {
  SetExpanded(!expanded_);
}

void NotificationViewMD::UpdateViewForExpandedState(bool expanded) {
  header_row_->SetExpanded(expanded);
  if (message_view_) {
    message_view_->SetLineLimit(expanded ? kMaxLinesForExpandedMessageView
                                         : kMaxLinesForMessageView);
  }
  if (image_container_view_)
    image_container_view_->SetVisible(expanded);

  actions_row_->SetVisible(expanded && (action_buttons_row_->has_children()));
  if (!expanded) {
    action_buttons_row_->SetVisible(true);
    inline_reply_->SetVisible(false);
  }

  for (size_t i = kMaxLinesForMessageView; i < item_views_.size(); ++i) {
    item_views_[i]->SetVisible(expanded);
  }
  if (status_view_)
    status_view_->SetVisible(expanded);
  header_row_->SetOverflowIndicator(
      list_items_count_ -
      (expanded ? item_views_.size() : kMaxLinesForMessageView));

  if (icon_view_)
    icon_view_->SetVisible(!hide_icon_on_expanded_ || !expanded);

  if (icon_view_ && icon_view_->visible()) {
    left_content_->SetBorder(
        views::CreateEmptyBorder(kLeftContentPaddingWithIcon));

    // TODO(tetsui): Workaround https://crbug.com/682266 by explicitly setting
    // the width.
    // Ideally, we should fix the original bug, but it seems there's no obvious
    // solution for the bug according to https://crbug.com/678337#c7, we should
    // ensure that the change won't break any of the users of BoxLayout class.
    if (message_view_)
      message_view_->SizeToFit(kMessageViewWidthWithIcon);
  } else {
    left_content_->SetBorder(views::CreateEmptyBorder(kLeftContentPadding));
    if (message_view_)
      message_view_->SizeToFit(kMessageViewWidth);
  }
}

void NotificationViewMD::ToggleInlineSettings() {
  DCHECK(settings_row_);

  bool inline_settings_visible = !settings_row_->visible();

  settings_row_->SetVisible(inline_settings_visible);
  content_row_->SetVisible(!inline_settings_visible);
  actions_row_->SetVisible(expanded_ && !inline_settings_visible);

  // When inline settings is shown, the background color of the entire
  // notification should be |kActionsRowBackgroundColor|.
  header_row_->SetBackground(views::CreateSolidBackground(
      inline_settings_visible ? kActionsRowBackgroundColor
                              : kNotificationBackgroundColor));

  // Always check "Don't block" when inline settings is shown.
  // If it's already blocked, users should not see inline settings.
  // Toggling should reset the state.
  dont_block_button_->SetChecked(true);

  PreferredSizeChanged();
}

// TODO(yoshiki): Move this to the parent class (MessageView) and share the code
// among NotificationView and ArcNotificationView.
void NotificationViewMD::UpdateControlButtonsVisibility() {
  const bool target_visibility =
      IsMouseHovered() || control_buttons_view_->IsCloseButtonFocused() ||
      control_buttons_view_->IsSettingsButtonFocused();

  control_buttons_view_->SetVisible(target_visibility);
}

NotificationControlButtonsView* NotificationViewMD::GetControlButtonsView()
    const {
  return control_buttons_view_.get();
}

bool NotificationViewMD::IsExpanded() const {
  return expanded_;
}

void NotificationViewMD::SetExpanded(bool expanded) {
  if (expanded_ == expanded)
    return;
  expanded_ = expanded;

  UpdateViewForExpandedState(expanded_);
  content_row_->InvalidateLayout();
  PreferredSizeChanged();
}

void NotificationViewMD::OnSettingsButtonPressed() {
  if (settings_row_)
    ToggleInlineSettings();
  else
    MessageView::OnSettingsButtonPressed();
}

void NotificationViewMD::Activate() {
  GetWidget()->widget_delegate()->set_can_activate(true);
  GetWidget()->Activate();
}

}  // namespace message_center
