// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/badging/badge_manager.h"

#include <utility>

#include "base/i18n/number_formatting.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/hosted_app_browser_controller.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/strings/grit/ui_strings.h"

#if defined(OS_MACOSX)
#include "chrome/browser/badging/badge_manager_delegate_mac.h"
#elif defined(OS_WIN)
#include "chrome/browser/badging/badge_manager_delegate_win.h"
#endif

namespace badging {

std::string GetBadgeString(base::Optional<uint64_t> badge_content) {
  if (!badge_content)
    return "•";

  if (badge_content > kMaxBadgeContent) {
    return base::UTF16ToUTF8(l10n_util::GetStringFUTF16(
        IDS_SATURATED_BADGE_CONTENT, base::FormatNumber(kMaxBadgeContent)));
  }

  return base::UTF16ToUTF8(base::FormatNumber(badge_content.value()));
}

BadgeManager::BadgeManager(Profile* profile) {
#if defined(OS_MACOSX)
  SetDelegate(std::make_unique<BadgeManagerDelegateMac>(profile));
#elif defined(OS_WIN)
  SetDelegate(std::make_unique<BadgeManagerDelegateWin>(profile));
#endif
}

BadgeManager::~BadgeManager() = default;

void BadgeManager::UpdateBadge(const extensions::ExtensionId& extension_id,
                               base::Optional<uint64_t> contents) {
  badged_apps_[extension_id] = contents;

  if (!delegate_)
    return;

  delegate_->OnBadgeSet(extension_id, contents);
}

void BadgeManager::ClearBadge(const extensions::ExtensionId& extension_id) {
  badged_apps_.erase(extension_id);
  if (!delegate_)
    return;

  delegate_->OnBadgeCleared(extension_id);
}

void BadgeManager::SetDelegate(std::unique_ptr<BadgeManagerDelegate> delegate) {
  delegate_ = std::move(delegate);
}

}  // namespace badging
