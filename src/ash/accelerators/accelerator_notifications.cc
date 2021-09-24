// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_notifications.h"

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/new_window_delegate.h"
#include "ash/public/cpp/notification_utils.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/enterprise_domain_model.h"
#include "ash/system/model/system_tray_model.h"
#include "base/strings/string_split.h"
#include "chromeos/ui/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/message_center.h"

namespace ash {

using gfx::VectorIcon;
using message_center::ButtonInfo;
using message_center::HandleNotificationClickDelegate;
using message_center::MessageCenter;
using message_center::Notification;
using message_center::NotificationDelegate;
using message_center::NotifierId;
using message_center::NotifierType;
using message_center::RichNotificationData;
using message_center::SystemNotificationWarningLevel;

namespace {

constexpr char kNotifierAccelerator[] = "ash.accelerator-controller";
constexpr char kSpokenFeedbackToggleAccelNotificationId[] =
    "chrome://settings/accessibility/spokenfeedback";
constexpr char kStartupNewShortcutNotificationId[] =
    "accelerator_controller.new_shortcuts_in_release";

// Ensures that there are no word breaks at the "+"s in the shortcut texts such
// as "Ctrl+Shift+Space".
void EnsureNoWordBreaks(std::u16string* shortcut_text) {
  std::vector<std::u16string> keys = base::SplitString(
      *shortcut_text, u"+", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  if (keys.size() < 2U)
    return;

  // The plus sign surrounded by the word joiner to guarantee an non-breaking
  // shortcut.
  const std::u16string non_breaking_plus = u"\u2060+\u2060";
  shortcut_text->clear();
  for (size_t i = 0; i < keys.size() - 1; ++i) {
    *shortcut_text += keys[i];
    *shortcut_text += non_breaking_plus;
  }

  *shortcut_text += keys.back();
}

// Gets the notification message after it formats it in such a way that there
// are no line breaks in the middle of the shortcut texts.
std::u16string GetNotificationText(int message_id,
                                   int old_shortcut_id,
                                   int new_shortcut_id) {
  std::u16string old_shortcut = l10n_util::GetStringUTF16(old_shortcut_id);
  std::u16string new_shortcut = l10n_util::GetStringUTF16(new_shortcut_id);
  EnsureNoWordBreaks(&old_shortcut);
  EnsureNoWordBreaks(&new_shortcut);

  return l10n_util::GetStringFUTF16(message_id, new_shortcut, old_shortcut);
}

std::unique_ptr<Notification> CreateNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    const VectorIcon& icon,
    scoped_refptr<NotificationDelegate> click_handler = nullptr,
    const RichNotificationData& rich_data = RichNotificationData()) {
  return CreateSystemNotification(
      message_center::NOTIFICATION_TYPE_SIMPLE, notification_id, title, message,
      std::u16string() /* display source */, GURL(),
      NotifierId(NotifierType::SYSTEM_COMPONENT, kNotifierAccelerator),
      rich_data, click_handler, icon, SystemNotificationWarningLevel::NORMAL);
}

void CreateAndShowStickyNotification(const std::string& notification_id,
                                     const std::u16string& title,
                                     const std::u16string& message,
                                     const VectorIcon& icon) {
  std::unique_ptr<Notification> notification =
      CreateNotification(notification_id, title, message, icon);

  notification->set_priority(message_center::SYSTEM_PRIORITY);
  MessageCenter::Get()->AddNotification(std::move(notification));
}

void CreateAndShowNotification(
    const std::string& notification_id,
    const std::u16string& title,
    const std::u16string& message,
    const VectorIcon& icon,
    scoped_refptr<NotificationDelegate> click_handler = nullptr,
    const RichNotificationData& rich_data = RichNotificationData()) {
  std::unique_ptr<Notification> notification = CreateNotification(
      notification_id, title, message, icon, click_handler, rich_data);
  MessageCenter::Get()->AddNotification(std::move(notification));
}

void NotifyAccessibilityFeatureDisabledByAdmin(
    int feature_name_id,
    bool feature_state,
    const std::string& notification_id) {
  const std::u16string title = l10n_util::GetStringUTF16(
      IDS_ASH_ACCESSIBILITY_FEATURE_SHORTCUT_DISABLED_TITLE);
  const std::u16string organization_manager =
      base::UTF8ToUTF16(Shell::Get()
                            ->system_tray_model()
                            ->enterprise_domain()
                            ->enterprise_domain_manager());
  const std::u16string activation_string = l10n_util::GetStringUTF16(
      feature_state ? IDS_ASH_ACCESSIBILITY_FEATURE_ACTIVATED
                    : IDS_ASH_ACCESSIBILITY_FEATURE_DEACTIVATED);

  const std::u16string message = l10n_util::GetStringFUTF16(
      IDS_ASH_ACCESSIBILITY_FEATURE_SHORTCUT_DISABLED_MSG, organization_manager,
      activation_string, l10n_util::GetStringUTF16(feature_name_id));

  CreateAndShowStickyNotification(notification_id, title, message,
                                  chromeos::kEnterpriseIcon);
}

void ShowAccessibilityNotification(int title_id,
                                   int message_id,
                                   const std::string& notification_id) {
  CreateAndShowStickyNotification(
      notification_id, l10n_util::GetStringUTF16(title_id),
      l10n_util::GetStringUTF16(message_id), kNotificationAccessibilityIcon);
}

void RemoveNotification(const std::string& notification_id) {
  MessageCenter::Get()->RemoveNotification(notification_id,
                                           /*by_user=*/false);
}

}  // namespace

// Shortcut help URL.
const char kKeyboardShortcutHelpPageUrl[] =
    "https://support.google.com/chromebook/answer/183101";

// Accessibility notification ids.
const char kDockedMagnifierToggleAccelNotificationId[] =
    "chrome://settings/accessibility/dockedmagnifier";
const char kFullscreenMagnifierToggleAccelNotificationId[] =
    "chrome://settings/accessibility/fullscreenmagnifier";
const char kHighContrastToggleAccelNotificationId[] =
    "chrome://settings/accessibility/highcontrast";

void ShowDeprecatedAcceleratorNotification(const char* notification_id,
                                           int message_id,
                                           int old_shortcut_id,
                                           int new_shortcut_id) {
  const std::u16string title =
      l10n_util::GetStringUTF16(IDS_DEPRECATED_SHORTCUT_TITLE);
  const std::u16string message =
      GetNotificationText(message_id, old_shortcut_id, new_shortcut_id);
  auto on_click_handler = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::BindRepeating([]() {
        if (!Shell::Get()->session_controller()->IsUserSessionBlocked())
          Shell::Get()->shell_delegate()->OpenKeyboardShortcutHelpPage();
      }));

  CreateAndShowNotification(notification_id, title, message,
                            kNotificationKeyboardIcon, on_click_handler);
}

void ShowShortcutsChangedNotification() {
  const std::u16string title = l10n_util::GetStringUTF16(
      IDS_SHORTCUT_CHANGES_IN_RELEASE_NOTIFICATION_TITLE);
  const std::u16string message = l10n_util::GetStringUTF16(
      IDS_SHORTCUT_CHANGES_IN_RELEASE_NOTIFICATION_BODY);

  // The notification only has one button, "Learn more".
  RichNotificationData rich_data;
  rich_data.buttons.push_back(ButtonInfo(l10n_util::GetStringUTF16(
      IDS_SHORTCUT_CHANGES_IN_RELEASE_NOTIFICATION_LEARN_MORE_BUTTON_TEXT)));

  // When the learn more button is clicked, open the keyboard shortcuts help
  // page. Otherwise if the body is clicked, open the shortcut viewer app.
  auto on_click_handler = base::MakeRefCounted<HandleNotificationClickDelegate>(
      base::BindRepeating([](absl::optional<int> button_index) {
        if (Shell::Get()->session_controller()->IsUserSessionBlocked())
          return;

        if (button_index.has_value()) {
          DCHECK_EQ(0, button_index.value());
          NewWindowDelegate::GetInstance()->OpenUrl(
              GURL(kKeyboardShortcutHelpPageUrl),
              /*from_user_interaction=*/true);
        } else {
          NewWindowDelegate::GetInstance()->ShowKeyboardShortcutViewer();
        }
      }));

  CreateAndShowNotification(kStartupNewShortcutNotificationId, title, message,
                            kNotificationKeyboardIcon, on_click_handler,
                            rich_data);
}

Notification* FindShortcutsChangedNotificationForTest() {
  return MessageCenter::Get()->FindVisibleNotificationById(
      kStartupNewShortcutNotificationId);
}

void ShowDockedMagnifierNotification() {
  ShowAccessibilityNotification(IDS_DOCKED_MAGNIFIER_ACCEL_TITLE,
                                IDS_DOCKED_MAGNIFIER_ACCEL_MSG,
                                kDockedMagnifierToggleAccelNotificationId);
}

void ShowDockedMagnifierDisabledByAdminNotification(bool feature_state) {
  NotifyAccessibilityFeatureDisabledByAdmin(
      IDS_ASH_DOCKED_MAGNIFIER_SHORTCUT_DISABLED, feature_state,
      kDockedMagnifierToggleAccelNotificationId);
}

void RemoveDockedMagnifierNotification() {
  RemoveNotification(kDockedMagnifierToggleAccelNotificationId);
}

void ShowFullscreenMagnifierNotification() {
  ShowAccessibilityNotification(IDS_FULLSCREEN_MAGNIFIER_ACCEL_TITLE,
                                IDS_FULLSCREEN_MAGNIFIER_ACCEL_MSG,
                                kFullscreenMagnifierToggleAccelNotificationId);
}

void ShowFullscreenMagnifierDisabledByAdminNotification(bool feature_state) {
  NotifyAccessibilityFeatureDisabledByAdmin(
      IDS_ASH_FULLSCREEN_MAGNIFIER_SHORTCUT_DISABLED, feature_state,
      kFullscreenMagnifierToggleAccelNotificationId);
}

void RemoveFullscreenMagnifierNotification() {
  RemoveNotification(kFullscreenMagnifierToggleAccelNotificationId);
}

void ShowHighContrastNotification() {
  ShowAccessibilityNotification(IDS_HIGH_CONTRAST_ACCEL_TITLE,
                                IDS_HIGH_CONTRAST_ACCEL_MSG,
                                kHighContrastToggleAccelNotificationId);
}

void ShowHighContrastDisabledByAdminNotification(bool feature_state) {
  NotifyAccessibilityFeatureDisabledByAdmin(
      IDS_ASH_HIGH_CONTRAST_SHORTCUT_DISABLED, feature_state,
      kHighContrastToggleAccelNotificationId);
}

void RemoveHighContrastNotification() {
  RemoveNotification(kHighContrastToggleAccelNotificationId);
}

void ShowSpokenFeedbackDisabledByAdminNotification(bool feature_state) {
  NotifyAccessibilityFeatureDisabledByAdmin(
      IDS_ASH_SPOKEN_FEEDBACK_SHORTCUT_DISABLED, feature_state,
      kSpokenFeedbackToggleAccelNotificationId);
}

void RemoveSpokenFeedbackNotification() {
  RemoveNotification(kSpokenFeedbackToggleAccelNotificationId);
}

}  // namespace ash
