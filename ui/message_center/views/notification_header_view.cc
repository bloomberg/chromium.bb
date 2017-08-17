// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/notification_header_view.h"

#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/message_center/message_center_style.h"
#include "ui/message_center/vector_icons.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/animation/ink_drop_impl.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/painter.h"

namespace message_center {

namespace {

constexpr int kHeaderHeight = 28;
constexpr int kAppIconSize = 12;
constexpr int kExpandIconSize = 12;
constexpr gfx::Insets kHeaderPadding(0, 12, 0, 2);
constexpr int kHeaderHorizontalSpacing = 2;
constexpr int kAppInfoConatainerTopPadding = 12;
// Bullet character. The divider symbol between different parts of the header.
constexpr wchar_t kNotificationHeaderDivider[] = L" \u2022 ";

// Base ink drop color of action buttons.
const SkColor kInkDropBaseColor = SkColorSetRGB(0x0, 0x0, 0x0);
// Ripple ink drop opacity of action buttons.
constexpr float kInkDropRippleVisibleOpacity = 0.08f;
// Highlight (hover) ink drop opacity of action buttons.
constexpr float kInkDropHighlightVisibleOpacity = 0.08f;

// base::TimeBase has similar constants, but some of them are missing.
constexpr int64_t kMinuteInMillis = 60LL * 1000LL;
constexpr int64_t kHourInMillis = 60LL * kMinuteInMillis;
constexpr int64_t kDayInMillis = 24LL * kHourInMillis;
// In Android, DateUtils.YEAR_IN_MILLIS is 364 days.
constexpr int64_t kYearInMillis = 364LL * kDayInMillis;

// ExpandButtton forwards all mouse and key events to NotificationHeaderView,
// but takes tab focus for accessibility purpose.
class ExpandButton : public views::ImageView {
 public:
  ExpandButton();
  ~ExpandButton() override;

  // Overridden from views::ImageView:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

 private:
  std::unique_ptr<views::Painter> focus_painter_;
};

ExpandButton::ExpandButton() {
  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      kFocusBorderColor, gfx::Insets(1, 2, 2, 2));
  SetFocusBehavior(FocusBehavior::ALWAYS);
}

ExpandButton::~ExpandButton() = default;

void ExpandButton::OnPaint(gfx::Canvas* canvas) {
  views::ImageView::OnPaint(canvas);
  views::Painter::PaintFocusPainter(this, canvas, focus_painter_.get());
}

void ExpandButton::OnFocus() {
  views::ImageView::OnFocus();
  SchedulePaint();
}

void ExpandButton::OnBlur() {
  views::ImageView::OnBlur();
  SchedulePaint();
}

void ExpandButton::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_BUTTON;
  node_data->SetName(views::ImageView::GetTooltipText());
}

// Do relative time string formatting that is similar to
// com.java.android.widget.DateTimeView.updateRelativeTime.
// Chromium has its own base::TimeFormat::Simple(), but none of the formats
// supported by the function is similar to Android's one.
base::string16 FormatToRelativeTime(base::Time past) {
  base::Time now = base::Time::Now();
  int64_t duration = (now - past).InMilliseconds();
  if (duration < kMinuteInMillis) {
    return l10n_util::GetStringUTF16(
        IDS_MESSAGE_NOTIFICATION_NOW_STRING_SHORTEST);
  } else if (duration < kHourInMillis) {
    int count = static_cast<int>(duration / kMinuteInMillis);
    return l10n_util::GetPluralStringFUTF16(
        IDS_MESSAGE_NOTIFICATION_DURATION_MINUTES_SHORTEST, count);
  } else if (duration < kDayInMillis) {
    int count = static_cast<int>(duration / kHourInMillis);
    return l10n_util::GetPluralStringFUTF16(
        IDS_MESSAGE_NOTIFICATION_DURATION_HOURS_SHORTEST, count);
  } else if (duration < kYearInMillis) {
    int count = static_cast<int>(duration / kDayInMillis);
    return l10n_util::GetPluralStringFUTF16(
        IDS_MESSAGE_NOTIFICATION_DURATION_DAYS_SHORTEST, count);
  } else {
    int count = static_cast<int>(duration / kYearInMillis);
    return l10n_util::GetPluralStringFUTF16(
        IDS_MESSAGE_NOTIFICATION_DURATION_YEARS_SHORTEST, count);
  }
}

}  // namespace

NotificationHeaderView::NotificationHeaderView(
    NotificationControlButtonsView* control_buttons_view,
    views::ButtonListener* listener)
    : views::Button(listener) {
  SetInkDropMode(InkDropMode::ON);
  set_has_ink_drop_action_on_click(true);
  set_animate_on_state_change(true);
  set_notify_enter_exit_on_child(true);
  set_ink_drop_base_color(kInkDropBaseColor);
  set_ink_drop_visible_opacity(kInkDropRippleVisibleOpacity);

  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, kHeaderPadding, kHeaderHorizontalSpacing);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(layout);

  ink_drop_container_ = new views::InkDropContainerView();
  ink_drop_container_->SetPaintToLayer();
  ink_drop_container_->layer()->SetFillsBoundsOpaquely(false);
  ink_drop_container_->SetVisible(false);
  AddChildView(ink_drop_container_);

  views::View* app_info_container = new views::View();
  views::BoxLayout* app_info_layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal,
                           gfx::Insets(kAppInfoConatainerTopPadding, 0, 0, 0),
                           kHeaderHorizontalSpacing);
  app_info_layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_CENTER);
  app_info_container->SetLayoutManager(app_info_layout);
  AddChildView(app_info_container);

  // App icon view
  app_icon_view_ = new views::ImageView();
  app_icon_view_->SetImageSize(gfx::Size(kAppIconSize, kAppIconSize));
  app_info_container->AddChildView(app_icon_view_);

  // App name view
  const gfx::FontList& font_list = views::Label().font_list().Derive(
      -2, gfx::Font::NORMAL, gfx::Font::Weight::NORMAL);
  app_name_view_ = new views::Label(base::string16());
  app_name_view_->SetFontList(font_list);
  app_name_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  app_name_view_->SetEnabledColor(accent_color_);
  app_info_container->AddChildView(app_name_view_);

  // Summary text divider
  summary_text_divider_ =
      new views::Label(base::WideToUTF16(kNotificationHeaderDivider));
  summary_text_divider_->SetFontList(font_list);
  summary_text_divider_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  summary_text_divider_->SetVisible(false);
  app_info_container->AddChildView(summary_text_divider_);

  // Summary text view
  summary_text_view_ = new views::Label(base::string16());
  summary_text_view_->SetFontList(font_list);
  summary_text_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  summary_text_view_->SetVisible(false);
  app_info_container->AddChildView(summary_text_view_);

  // Timestamp divider
  timestamp_divider_ =
      new views::Label(base::WideToUTF16(kNotificationHeaderDivider));
  timestamp_divider_->SetFontList(font_list);
  timestamp_divider_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  timestamp_divider_->SetVisible(false);
  app_info_container->AddChildView(timestamp_divider_);

  // Timestamp view
  timestamp_view_ = new views::Label(base::string16());
  timestamp_view_->SetFontList(font_list);
  timestamp_view_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  timestamp_view_->SetVisible(false);
  app_info_container->AddChildView(timestamp_view_);

  // Expand button view
  expand_button_ = new ExpandButton();
  SetExpanded(is_expanded_);
  app_info_container->AddChildView(expand_button_);

  // Spacer between left-aligned views and right-aligned views
  views::View* spacer = new views::View;
  spacer->SetPreferredSize(gfx::Size(1, kHeaderHeight));
  AddChildView(spacer);
  layout->SetFlexForView(spacer, 1);

  // Settings and close buttons view
  AddChildView(control_buttons_view);
}

void NotificationHeaderView::SetAppIcon(const gfx::ImageSkia& img) {
  app_icon_view_->SetImage(img);
}

void NotificationHeaderView::ClearAppIcon() {
  app_icon_view_->SetImage(gfx::CreateVectorIcon(kProductIcon, accent_color_));
}

void NotificationHeaderView::SetAppName(const base::string16& name) {
  app_name_view_->SetText(name);
}

void NotificationHeaderView::SetProgress(int progress) {
  summary_text_view_->SetText(l10n_util::GetStringFUTF16Int(
      IDS_MESSAGE_CENTER_NOTIFICATION_PROGRESS_PERCENTAGE, progress));
  has_progress_ = true;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::ClearProgress() {
  has_progress_ = false;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::SetOverflowIndicator(int count) {
  if (count > 0) {
    summary_text_view_->SetText(l10n_util::GetStringFUTF16Int(
        IDS_MESSAGE_CENTER_LIST_NOTIFICATION_HEADER_OVERFLOW_INDICATOR, count));
    has_overflow_indicator_ = true;
  } else {
    has_overflow_indicator_ = false;
  }
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::ClearOverflowIndicator() {
  has_overflow_indicator_ = false;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::SetTimestamp(base::Time past) {
  timestamp_view_->SetText(FormatToRelativeTime(past));
  has_timestamp_ = true;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::ClearTimestamp() {
  has_timestamp_ = false;
  UpdateSummaryTextVisibility();
}

void NotificationHeaderView::SetExpandButtonEnabled(bool enabled) {
  // SetInkDropMode iff. the visibility changed.
  // Otherwise, the ink drop animation cannot finish.
  if (expand_button_->visible() != enabled)
    SetInkDropMode(enabled ? InkDropMode::ON : InkDropMode::OFF);

  expand_button_->SetVisible(enabled);
}

void NotificationHeaderView::SetExpanded(bool expanded) {
  is_expanded_ = expanded;
  expand_button_->SetImage(gfx::CreateVectorIcon(
      expanded ? kNotificationExpandLessIcon : kNotificationExpandMoreIcon,
      kExpandIconSize, accent_color_));
  // TODO(tetsui): Replace by more helpful accessibility strings.
  // https://crbug.com/755855
  expand_button_->SetTooltipText(l10n_util::GetStringUTF16(
      expanded ? IDS_APP_ACCNAME_MINIMIZE : IDS_APP_ACCNAME_MAXIMIZE));
}

void NotificationHeaderView::SetAccentColor(SkColor color) {
  accent_color_ = color;
  app_name_view_->SetEnabledColor(accent_color_);
  SetExpanded(is_expanded_);
}

bool NotificationHeaderView::IsExpandButtonEnabled() {
  return expand_button_->visible();
}

std::unique_ptr<views::InkDrop> NotificationHeaderView::CreateInkDrop() {
  auto ink_drop = base::MakeUnique<views::InkDropImpl>(this, size());
  ink_drop->SetAutoHighlightMode(
      views::InkDropImpl::AutoHighlightMode::SHOW_ON_RIPPLE);
  ink_drop->SetShowHighlightOnHover(false);
  return ink_drop;
}

std::unique_ptr<views::InkDropRipple>
NotificationHeaderView::CreateInkDropRipple() const {
  return base::MakeUnique<views::FloodFillInkDropRipple>(
      size(), GetInkDropCenterBasedOnLastEvent(), GetInkDropBaseColor(),
      ink_drop_visible_opacity());
}

std::unique_ptr<views::InkDropHighlight>
NotificationHeaderView::CreateInkDropHighlight() const {
  auto highlight = base::MakeUnique<views::InkDropHighlight>(
      size(), kInkDropSmallCornerRadius,
      gfx::RectF(GetLocalBounds()).CenterPoint(), GetInkDropBaseColor());
  highlight->set_visible_opacity(kInkDropHighlightVisibleOpacity);
  return highlight;
}

void NotificationHeaderView::AddInkDropLayer(ui::Layer* ink_drop_layer) {
  ink_drop_container_->AddInkDropLayer(ink_drop_layer);
  InstallInkDropMask(ink_drop_layer);
}

void NotificationHeaderView::RemoveInkDropLayer(ui::Layer* ink_drop_layer) {
  ResetInkDropMask();
  ink_drop_container_->RemoveInkDropLayer(ink_drop_layer);
}

void NotificationHeaderView::UpdateSummaryTextVisibility() {
  const bool visible = has_progress_ || has_overflow_indicator_;
  summary_text_divider_->SetVisible(visible);
  summary_text_view_->SetVisible(visible);
  timestamp_divider_->SetVisible(!has_progress_ && has_timestamp_);
  timestamp_view_->SetVisible(!has_progress_ && has_timestamp_);
  Layout();
}

}  // namespace message_center
