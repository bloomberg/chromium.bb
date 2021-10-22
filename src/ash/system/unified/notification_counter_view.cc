// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/notification_counter_view.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/vm_camera_mic_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/message_center/ash_message_center_lock_screen_controller.h"
#include "ash/system/message_center/message_center_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_utils.h"
#include "ash/system/unified/notification_icons_controller.h"
#include "base/i18n/number_formatting.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/image/canvas_image_source.h"
#include "ui/gfx/vector_icon_utils.h"
#include "ui/message_center/message_center.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"

namespace ash {

namespace {

constexpr double kTrayNotificationCircleIconRadius = 8;

// The size of the number font inside the icon. Should be updated when
// kUnifiedTrayIconSize is changed.
constexpr int kNumberIconFontSize = 11;

constexpr gfx::Insets kSeparatorPadding(6, 4);

gfx::FontList GetNumberIconFontList() {
  // |kNumberIconFontSize| is hard-coded as 11, which whould be updated when
  // the tray icon size is changed.
  DCHECK_EQ(18, kUnifiedTrayIconSize);

  gfx::Font default_font;
  int font_size_delta = kNumberIconFontSize - default_font.GetFontSize();
  gfx::Font font = default_font.Derive(font_size_delta, gfx::Font::NORMAL,
                                       gfx::Font::Weight::BOLD);
  DCHECK_EQ(kNumberIconFontSize, font.GetFontSize());
  return gfx::FontList(font);
}

SkColor SeparatorIconColor(session_manager::SessionState state) {
  if (state == session_manager::SessionState::OOBE)
    return kIconColorInOobe;
  return AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor);
}

class NumberIconImageSource : public gfx::CanvasImageSource {
 public:
  explicit NumberIconImageSource(size_t count)
      : CanvasImageSource(
            gfx::Size(kUnifiedTrayIconSize, kUnifiedTrayIconSize)),
        count_(count) {
    DCHECK_LE(count_, kTrayNotificationMaxCount + 1);
  }

  NumberIconImageSource(const NumberIconImageSource&) = delete;
  NumberIconImageSource& operator=(const NumberIconImageSource&) = delete;

  void Draw(gfx::Canvas* canvas) override {
    SkColor tray_icon_color =
        TrayIconColor(Shell::Get()->session_controller()->GetSessionState());
    // Paint the contents inside the circle background. The color doesn't matter
    // as it will be hollowed out by the XOR operation.
    if (count_ > kTrayNotificationMaxCount) {
      canvas->DrawImageInt(
          gfx::CreateVectorIcon(kSystemTrayNotificationCounterPlusIcon,
                                size().width(), tray_icon_color),
          0, 0);
    } else {
      canvas->DrawStringRectWithFlags(
          base::FormatNumber(count_), GetNumberIconFontList(), tray_icon_color,
          gfx::Rect(size()),
          gfx::Canvas::TEXT_ALIGN_CENTER | gfx::Canvas::NO_SUBPIXEL_RENDERING);
    }
    cc::PaintFlags flags;
    flags.setBlendMode(SkBlendMode::kXor);
    flags.setAntiAlias(true);
    flags.setColor(tray_icon_color);
    canvas->DrawCircle(gfx::RectF(gfx::SizeF(size())).CenterPoint(),
                       kTrayNotificationCircleIconRadius, flags);
  }

 private:
  size_t count_;
};

}  // namespace

NotificationCounterView::NotificationCounterView(
    Shelf* shelf,
    NotificationIconsController* controller)
    : TrayItemView(shelf), controller_(controller) {
  CreateImageView();
  SetVisible(false);
}

NotificationCounterView::~NotificationCounterView() = default;

void NotificationCounterView::Update() {
  if (message_center_utils::GetNotificationCount() == 0 ||
      !controller_->ShouldShowNotificationItemsInTray()) {
    SetVisible(false);
    return;
  }

  // If the tray is showing notification icons, display the count of
  // notifications not showing. Otherwise, show the count of total
  // notifications.
  size_t notification_count;
  if (features::IsScalableStatusAreaEnabled() &&
      controller_->icons_view_visible() &&
      controller_->TrayItemHasNotification()) {
    notification_count = message_center_utils::GetNotificationCount() -
                         controller_->TrayNotificationIconsCount();
    if (notification_count == 0) {
      SetVisible(false);
      return;
    }
    image_view()->SetTooltipText(l10n_util::GetPluralStringFUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_HIDDEN_COUNT_TOOLTIP,
        notification_count));
  } else {
    notification_count = message_center_utils::GetNotificationCount();
    image_view()->SetTooltipText(l10n_util::GetPluralStringFUTF16(
        IDS_ASH_STATUS_TRAY_NOTIFICATIONS_COUNT_TOOLTIP, notification_count));
  }

  int icon_id = std::min(notification_count, kTrayNotificationMaxCount + 1);
  if (icon_id != count_for_display_) {
    image_view()->SetImage(
        gfx::CanvasImageSource::MakeImageSkia<NumberIconImageSource>(icon_id));
    count_for_display_ = icon_id;
  }
  SetVisible(true);
}

std::u16string NotificationCounterView::GetAccessibleNameString() const {
  return GetVisible() ? image_view()->GetTooltipText() : base::EmptyString16();
}

void NotificationCounterView::HandleLocaleChange() {
  Update();
}

void NotificationCounterView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  image_view()->SetImage(
      gfx::CanvasImageSource::MakeImageSkia<NumberIconImageSource>(
          count_for_display_));
}

const char* NotificationCounterView::GetClassName() const {
  return "NotificationCounterView";
}

QuietModeView::QuietModeView(Shelf* shelf) : TrayItemView(shelf) {
  CreateImageView();
  image_view()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_QUIET_MODE_TOOLTIP));
  SetVisible(false);
}

QuietModeView::~QuietModeView() = default;

void QuietModeView::Update() {
  // TODO(yamaguchi): Add this check when new style of the system tray is
  // implemented, so that icon resizing will not happen here.
  // DCHECK_EQ(kTrayIconSize,
  //     gfx::GetDefaultSizeOfVectorIcon(kSystemTrayDoNotDisturbIcon));
  if (message_center::MessageCenter::Get()->IsQuietMode()) {
    image_view()->SetImage(gfx::CreateVectorIcon(
        kSystemTrayDoNotDisturbIcon,
        TrayIconColor(Shell::Get()->session_controller()->GetSessionState())));
    SetVisible(true);
  } else {
    SetVisible(false);
  }
}

void QuietModeView::HandleLocaleChange() {
  image_view()->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_QUIET_MODE_TOOLTIP));
}

void QuietModeView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  Update();
}

const char* QuietModeView::GetClassName() const {
  return "QuietModeView";
}

SeparatorTrayItemView::SeparatorTrayItemView(Shelf* shelf)
    : TrayItemView(shelf) {
  views::Separator* separator = new views::Separator();
  separator->SetColor(SeparatorIconColor(
      Shell::Get()->session_controller()->GetSessionState()));
  separator->SetBorder(views::CreateEmptyBorder(kSeparatorPadding));
  separator_ = AddChildView(separator);

  set_use_scale_in_animation(false);
}

SeparatorTrayItemView::~SeparatorTrayItemView() = default;

void SeparatorTrayItemView::HandleLocaleChange() {}

const char* SeparatorTrayItemView::GetClassName() const {
  return "SeparatorTrayItemView";
}

void SeparatorTrayItemView::UpdateColor(session_manager::SessionState state) {
  separator_->SetColor(SeparatorIconColor(state));
}

}  // namespace ash
