// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/cookie_settings.h"
#include "base/bind.h"

namespace network {

CookieSettings::CookieSettings() {}
CookieSettings::~CookieSettings() {}

SessionCleanupCookieStore::DeleteCookiePredicate
CookieSettings::CreateDeleteCookieOnExitPredicate() const {
  if (!HasSessionOnlyOrigins())
    return SessionCleanupCookieStore::DeleteCookiePredicate();
  return base::BindRepeating(&CookieSettings::ShouldDeleteCookieOnExit,
                             base::Unretained(this),
                             base::ConstRef(content_settings_));
}

void CookieSettings::GetCookieSetting(const GURL& url,
                                      const GURL& first_party_url,
                                      content_settings::SettingSource* source,
                                      ContentSetting* cookie_setting) const {
  // TODO(cduvall): Move third party cookie blocking logic here.
  DCHECK_EQ(url, first_party_url);
  for (const auto& entry : content_settings_) {
    if (entry.primary_pattern.Matches(url) &&
        entry.secondary_pattern.Matches(first_party_url)) {
      *cookie_setting = entry.GetContentSetting();
      return;
    }
  }
  *cookie_setting = CONTENT_SETTING_ALLOW;
}

bool CookieSettings::HasSessionOnlyOrigins() const {
  for (const auto& entry : content_settings_) {
    if (entry.GetContentSetting() == CONTENT_SETTING_SESSION_ONLY)
      return true;
  }
  return false;
}

}  // namespace network
