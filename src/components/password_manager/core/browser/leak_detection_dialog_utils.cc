// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "net/base/url_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace password_manager {

using metrics_util::LeakDialogType;

constexpr char kPasswordCheckupURL[] =
    "https://passwords.google.com/checkup/start?hideExplanation=true";

CredentialLeakType CreateLeakType(IsSaved is_saved,
                                  IsReused is_reused,
                                  IsSyncing is_syncing) {
  CredentialLeakType leak_type = 0;
  if (is_saved)
    leak_type |= kPasswordSaved;
  if (is_reused)
    leak_type |= kPasswordUsedOnOtherSites;
  if (is_syncing)
    leak_type |= kSyncingPasswordsNormally;
  return leak_type;
}

bool IsPasswordSaved(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kPasswordSaved;
}

bool IsPasswordUsedOnOtherSites(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kPasswordUsedOnOtherSites;
}

bool IsSyncingPasswordsNormally(CredentialLeakType leak_type) {
  return leak_type & CredentialLeakFlags::kSyncingPasswordsNormally;
}

// Formats the |origin| to a human-friendly url string.
base::string16 GetFormattedUrl(const GURL& origin) {
  return url_formatter::FormatUrlForSecurityDisplay(
      origin, url_formatter::SchemeDisplay::OMIT_HTTP_AND_HTTPS);
}

base::string16 GetAcceptButtonLabel(CredentialLeakType leak_type) {
  return l10n_util::GetStringUTF16(
      ShouldCheckPasswords(leak_type) ? IDS_LEAK_CHECK_CREDENTIALS : IDS_OK);
}

base::string16 GetCancelButtonLabel() {
  return l10n_util::GetStringUTF16(IDS_CLOSE);
}

base::string16 GetDescription(CredentialLeakType leak_type,
                              const GURL& origin) {
  const base::string16 formatted = GetFormattedUrl(origin);
  if (!ShouldCheckPasswords(leak_type)) {
    return l10n_util::GetStringFUTF16(
        IDS_CREDENTIAL_LEAK_CHANGE_PASSWORD_MESSAGE, formatted);
  } else if (password_manager::IsPasswordSaved(leak_type)) {
    return l10n_util::GetStringUTF16(
        IDS_CREDENTIAL_LEAK_CHECK_PASSWORDS_MESSAGE);
  } else {
    return l10n_util::GetStringFUTF16(
        IDS_CREDENTIAL_LEAK_CHANGE_AND_CHECK_PASSWORDS_MESSAGE, formatted);
  }
}

base::string16 GetTitle(CredentialLeakType leak_type) {
  return l10n_util::GetStringUTF16(ShouldCheckPasswords(leak_type)
                                       ? IDS_CREDENTIAL_LEAK_TITLE_CHECK
                                       : IDS_CREDENTIAL_LEAK_TITLE_CHANGE);
}

base::string16 GetLeakDetectionTooltip() {
  return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_LEAK_HELP_MESSAGE);
}

bool ShouldCheckPasswords(CredentialLeakType leak_type) {
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordCheck)) {
    return password_manager::IsPasswordUsedOnOtherSites(leak_type);
  }
  return password_manager::IsPasswordUsedOnOtherSites(leak_type) &&
         password_manager::IsSyncingPasswordsNormally(leak_type);
}

bool ShouldShowCancelButton(CredentialLeakType leak_type) {
  return ShouldCheckPasswords(leak_type);
}

LeakDialogType GetLeakDialogType(CredentialLeakType leak_type) {
  if (!ShouldCheckPasswords(leak_type))
    return LeakDialogType::kChange;

  return password_manager::IsPasswordSaved(leak_type)
             ? LeakDialogType::kCheckup
             : LeakDialogType::kCheckupAndChange;
}

GURL GetPasswordCheckupURL(PasswordCheckupReferrer referrer) {
  GURL url(kPasswordCheckupURL);
  url = net::AppendQueryParameter(url, "utm_source", "chrome");

#if defined(OS_ANDROID)
  const char* const medium = "android";
#elif defined(OS_IOS)
  const char* const medium = "ios";
#else
  const char* const medium = "desktop";
#endif
  url = net::AppendQueryParameter(url, "utm_medium", medium);

  const char* const campaign =
      referrer == PasswordCheckupReferrer::kLeakDetectionDialog
          ? "leak_dialog"
          : "password_settings";

  return net::AppendQueryParameter(url, "utm_campaign", campaign);
}

}  // namespace password_manager
