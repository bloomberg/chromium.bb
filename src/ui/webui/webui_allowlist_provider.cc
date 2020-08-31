// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/webui/webui_allowlist_provider.h"

#include "components/content_settings/core/common/content_settings_pattern.h"
#include "ui/webui/webui_allowlist.h"

WebUIAllowlistProvider::WebUIAllowlistProvider(WebUIAllowlist* allowlist)
    : allowlist_(allowlist) {
  DCHECK(allowlist_);
  allowlist_->SetWebUIAllowlistProvider(this);
}

WebUIAllowlistProvider::~WebUIAllowlistProvider() = default;

std::unique_ptr<content_settings::RuleIterator>
WebUIAllowlistProvider::GetRuleIterator(
    ContentSettingsType content_type,
    const content_settings::ResourceIdentifier& /*resource_identifier*/,
    bool incognito) const {
  if (!allowlist_)
    return nullptr;

  return allowlist_->GetRuleIterator(content_type);
}

void WebUIAllowlistProvider::NotifyContentSettingChange(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type) {
  NotifyObservers(primary_pattern, secondary_pattern, content_type,
                  /*resource_identifier*/ std::string());
}

bool WebUIAllowlistProvider::SetWebsiteSetting(
    const ContentSettingsPattern& primary_pattern,
    const ContentSettingsPattern& secondary_pattern,
    ContentSettingsType content_type,
    const content_settings::ResourceIdentifier& /*resource_identifier*/,
    std::unique_ptr<base::Value>&& value,
    const content_settings::ContentSettingConstraints& constraints) {
  // WebUIAllowlistProvider doesn't support settings Website settings.
  return false;
}

void WebUIAllowlistProvider::ClearAllContentSettingsRules(
    ContentSettingsType content_type) {
  // WebUIAllowlistProvider doesn't support changing content settings directly.
}

void WebUIAllowlistProvider::ShutdownOnUIThread() {
  RemoveAllObservers();
  allowlist_->ResetWebUIAllowlistProvider();
  allowlist_ = nullptr;
}
