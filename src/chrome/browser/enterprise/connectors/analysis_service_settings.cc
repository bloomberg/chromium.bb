// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis_service_settings.h"

#include "components/policy/core/browser/url_util.h"

namespace enterprise_connectors {

AnalysisServiceSettings::AnalysisServiceSettings(
    const base::Value& settings_value) {
  if (!settings_value.is_dict())
    return;

  // The service provider identifier should always be there.
  const std::string* service_provider =
      settings_value.FindStringKey(kKeyServiceProvider);
  if (service_provider)
    service_provider_ = *service_provider;
  else
    return;

  // Add the patterns to the settings, which configures settings.matcher and
  // settings.*_pattern_settings. No enable patterns implies the settings are
  // invalid.
  matcher_ = std::make_unique<url_matcher::URLMatcher>();
  url_matcher::URLMatcherConditionSet::ID id(0);
  const base::Value* enable = settings_value.FindListKey(kKeyEnable);
  if (enable && enable->is_list()) {
    for (const base::Value& value : enable->GetList())
      AddUrlPatternSettings(value, true, &id);
  } else {
    return;
  }

  const base::Value* disable = settings_value.FindListKey(kKeyDisable);
  if (disable && disable->is_list()) {
    for (const base::Value& value : disable->GetList())
      AddUrlPatternSettings(value, false, &id);
  }

  // The block settings are optional, so a default is used if they can't be
  // found.
  block_until_verdict_ =
      settings_value.FindIntKey(kKeyBlockUntilVerdict).value_or(0)
          ? BlockUntilVerdict::BLOCK
          : BlockUntilVerdict::NO_BLOCK;
  block_password_protected_files_ =
      settings_value.FindBoolKey(kKeyBlockPasswordProtected).value_or(false);
  block_large_files_ =
      settings_value.FindBoolKey(kKeyBlockLargeFiles).value_or(false);
  block_unsupported_file_types_ =
      settings_value.FindBoolKey(kKeyBlockUnsupportedFileTypes).value_or(false);
}

// static
base::Optional<AnalysisServiceSettings::URLPatternSettings>
AnalysisServiceSettings::GetPatternSettings(
    const PatternSettings& patterns,
    url_matcher::URLMatcherConditionSet::ID match) {
  // If the pattern exists directly in the map, return its settings.
  if (patterns.count(match) == 1)
    return patterns.at(match);

  // If the pattern doesn't exist in the map, it might mean that it wasn't the
  // only pattern to correspond to its settings and that the ID added to
  // the map was the one of the last pattern corresponding to those settings.
  // This means the next match ID greater than |match| has the correct settings
  // if it exists.
  auto next = patterns.upper_bound(match);
  if (next != patterns.end())
    return next->second;

  return base::nullopt;
}

base::Optional<AnalysisSettings> AnalysisServiceSettings::GetAnalysisSettings(
    const GURL& url) const {
  if (!IsValid())
    return base::nullopt;

  DCHECK(matcher_);
  auto matches = matcher_->MatchURL(url);
  if (matches.empty())
    return base::nullopt;

  auto tags = GetTags(matches);
  if (tags.empty())
    return base::nullopt;

  AnalysisSettings settings;

  settings.tags = std::move(tags);
  settings.block_until_verdict = block_until_verdict_;
  settings.block_password_protected_files = block_password_protected_files_;
  settings.block_large_files = block_large_files_;
  settings.block_unsupported_file_types = block_unsupported_file_types_;

  return settings;
}

void AnalysisServiceSettings::AddUrlPatternSettings(
    const base::Value& url_settings_value,
    bool enabled,
    url_matcher::URLMatcherConditionSet::ID* id) {
  DCHECK(id);
  if (enabled)
    DCHECK(disabled_patterns_settings_.empty());
  else
    DCHECK(!enabled_patterns_settings_.empty());

  URLPatternSettings setting;

  const base::Value* tags = url_settings_value.FindListKey(kKeyTags);
  if (tags && tags->is_list()) {
    for (const base::Value& tag : tags->GetList()) {
      if (tag.is_string())
        setting.tags.insert(tag.GetString());
    }
  } else {
    return;
  }

  // Add the URL patterns to the matcher and store the condition set IDs.
  const base::Value* url_list = url_settings_value.FindListKey(kKeyUrlList);
  if (url_list && url_list->is_list()) {
    const base::ListValue* url_list_value = nullptr;
    url_list->GetAsList(&url_list_value);
    DCHECK(url_list_value);
    policy::url_util::AddFilters(matcher_.get(), enabled, id, url_list_value);
  } else {
    return;
  }

  if (enabled)
    enabled_patterns_settings_[*id] = std::move(setting);
  else
    disabled_patterns_settings_[*id] = std::move(setting);
}

std::set<std::string> AnalysisServiceSettings::GetTags(
    const std::set<url_matcher::URLMatcherConditionSet::ID>& matches) const {
  std::set<std::string> enable_tags;
  std::set<std::string> disable_tags;
  for (const url_matcher::URLMatcherConditionSet::ID match : matches) {
    // Enabled patterns need to be checked first, otherwise they always match
    // the first disabled pattern.
    bool enable = true;
    auto maybe_pattern_setting =
        GetPatternSettings(enabled_patterns_settings_, match);
    if (!maybe_pattern_setting.has_value()) {
      maybe_pattern_setting =
          GetPatternSettings(disabled_patterns_settings_, match);
      enable = false;
    }

    DCHECK(maybe_pattern_setting.has_value());
    auto tags = std::move(maybe_pattern_setting.value().tags);
    if (enable)
      enable_tags.insert(tags.begin(), tags.end());
    else
      disable_tags.insert(tags.begin(), tags.end());
  }

  for (const std::string& tag_to_disable : disable_tags)
    enable_tags.erase(tag_to_disable);

  return enable_tags;
}

bool AnalysisServiceSettings::IsValid() const {
  // The settings are invalid if no provider was given.
  if (service_provider_.empty())
    return false;

  // The settings are invalid if no enabled pattern(s) exist since that would
  // imply no URL can ever have an analysis.
  if (enabled_patterns_settings_.empty())
    return false;

  return true;
}

AnalysisServiceSettings::AnalysisServiceSettings(AnalysisServiceSettings&&) =
    default;
AnalysisServiceSettings::~AnalysisServiceSettings() = default;

AnalysisServiceSettings::URLPatternSettings::URLPatternSettings() = default;
AnalysisServiceSettings::URLPatternSettings::URLPatternSettings(
    const AnalysisServiceSettings::URLPatternSettings&) = default;
AnalysisServiceSettings::URLPatternSettings::~URLPatternSettings() = default;

}  // namespace enterprise_connectors
