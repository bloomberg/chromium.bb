// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/tailored_security/consented_message_android.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/safe_browsing/android/safe_browsing_settings_launcher_android.h"
#include "chrome/browser/safe_browsing/tailored_security/tailored_security_outcome.h"
#include "chrome/grit/generated_resources.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace safe_browsing {

namespace {
void LogOutcome(TailoredSecurityOutcome outcome, bool enable) {
  std::string histogram =
      enable ? "SafeBrowsing.TailoredSecurityConsentedEnabledMessageOutcome"
             : "SafeBrowsing.TailoredSecurityConsentedDisabledMessageOutcome";
  base::UmaHistogramEnumeration(histogram, outcome);
}

}  // namespace

TailoredSecurityConsentedModalAndroid::TailoredSecurityConsentedModalAndroid() =
    default;

TailoredSecurityConsentedModalAndroid::
    ~TailoredSecurityConsentedModalAndroid() {
  DismissMessageInternal(messages::DismissReason::UNKNOWN);
}

void TailoredSecurityConsentedModalAndroid::DisplayMessage(
    content::WebContents* web_contents,
    bool enable,
    base::OnceClosure dismiss_callback) {
  if (message_) {
    return;
  }
  web_contents_ = web_contents;
  is_enable_message_ = enable;
  dismiss_callback_ = std::move(dismiss_callback);
  message_ = std::make_unique<messages::MessageWrapper>(
      is_enable_message_
          ? messages::MessageIdentifier::TAILORED_SECURITY_ENABLED
          : messages::MessageIdentifier::TAILORED_SECURITY_DISABLED,
      base::BindOnce(
          &TailoredSecurityConsentedModalAndroid::HandleMessageAccepted,
          base::Unretained(this)),
      base::BindOnce(
          &TailoredSecurityConsentedModalAndroid::HandleMessageDismissed,
          base::Unretained(this)));
  std::u16string title, description;
  int icon_resource_id;
  if (is_enable_message_) {
    title = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_ENABLE_MESSAGE_TITLE);
    description = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_ENABLE_MESSAGE_DESCRIPTION);
    icon_resource_id =
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SAFETY_CHECK);
  } else {
    title = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_MESSAGE_TITLE);
    description = l10n_util::GetStringUTF16(
        IDS_TAILORED_SECURITY_CONSENTED_DISABLE_MESSAGE_DESCRIPTION);
    icon_resource_id =
        ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_GPP_MAYBE_GREY);
  }
  message_->SetTitle(title);
  message_->SetDescription(description);
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(
      IDS_TAILORED_SECURITY_CONSENTED_MESSAGE_OK_BUTTON));
  message_->SetIconResourceId(icon_resource_id);
  message_->DisableIconTint();
  message_->SetSecondaryIconResourceId(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_MESSAGE_SETTINGS));
  message_->SetSecondaryActionCallback(base::BindOnce(
      &TailoredSecurityConsentedModalAndroid::HandleSettingsClicked,
      base::Unretained(this)));

  messages::MessageDispatcherBridge::Get()->EnqueueWindowScopedMessage(
      message_.get(), web_contents_->GetTopLevelNativeWindow(),
      messages::MessagePriority::kNormal);
  LogOutcome(TailoredSecurityOutcome::kShown, is_enable_message_);
}

void TailoredSecurityConsentedModalAndroid::DismissMessageInternal(
    messages::DismissReason dismiss_reason) {
  if (!message_)
    return;
  messages::MessageDispatcherBridge::Get()->DismissMessage(message_.get(),
                                                           dismiss_reason);
  std::move(dismiss_callback_).Run();
}

void TailoredSecurityConsentedModalAndroid::HandleSettingsClicked() {
  ShowSafeBrowsingSettings(web_contents_,
                           SettingsAccessPoint::kTailoredSecurity);
  LogOutcome(TailoredSecurityOutcome::kSettings, is_enable_message_);
  DismissMessageInternal(messages::DismissReason::SECONDARY_ACTION);
}

void TailoredSecurityConsentedModalAndroid::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  LogOutcome(TailoredSecurityOutcome::kDismissed, is_enable_message_);
  message_.reset();
  std::move(dismiss_callback_).Run();
}

void TailoredSecurityConsentedModalAndroid::HandleMessageAccepted() {
  LogOutcome(TailoredSecurityOutcome::kAccepted, is_enable_message_);
  std::move(dismiss_callback_).Run();
}

}  // namespace safe_browsing
