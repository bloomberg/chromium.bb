// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_manager_infobar_delegate_android.h"

#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/password_manager/core/browser/password_manager_constants.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

PasswordManagerInfoBarDelegate::~PasswordManagerInfoBarDelegate() = default;

std::u16string PasswordManagerInfoBarDelegate::GetDetailsMessageText() const {
  return details_message_;
}

infobars::InfoBarDelegate::InfoBarAutomationType
PasswordManagerInfoBarDelegate::GetInfoBarAutomationType() const {
  return PASSWORD_INFOBAR;
}

int PasswordManagerInfoBarDelegate::GetIconId() const {
  return IDR_ANDROID_INFOBAR_SAVE_PASSWORD;
}

GURL PasswordManagerInfoBarDelegate::GetLinkURL() const {
  return GURL(password_manager::kPasswordManagerHelpCenterSmartLock);
}

bool PasswordManagerInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return !details.is_redirect && ConfirmInfoBarDelegate::ShouldExpire(details);
}

bool PasswordManagerInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  ConfirmInfoBarDelegate::LinkClicked(disposition);
  return true;
}

std::u16string PasswordManagerInfoBarDelegate::GetMessageText() const {
  return message_;
}

PasswordManagerInfoBarDelegate::PasswordManagerInfoBarDelegate() = default;

void PasswordManagerInfoBarDelegate::SetMessage(const std::u16string& message) {
  message_ = message;
}

void PasswordManagerInfoBarDelegate::SetDetailsMessage(
    const std::u16string& details_message) {
  details_message_ = details_message;
}
