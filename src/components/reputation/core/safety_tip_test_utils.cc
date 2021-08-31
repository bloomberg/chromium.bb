// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reputation/core/safety_tip_test_utils.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "components/reputation/core/safety_tips_config.h"

namespace reputation {

namespace {

// Retrieve existing config proto if set, or create a new one otherwise.
std::unique_ptr<SafetyTipsConfig> GetConfig() {
  auto* old = GetSafetyTipsRemoteConfigProto();
  if (old) {
    return std::make_unique<SafetyTipsConfig>(*old);
  }

  auto conf = std::make_unique<SafetyTipsConfig>();
  // Any version ID will do.
  conf->set_version_id(4);
  return conf;
}

}  // namespace

void InitializeSafetyTipConfig() {
  SetSafetyTipsRemoteConfigProto(GetConfig());
}

void SetSafetyTipPatternsWithFlagType(std::vector<std::string> patterns,
                                      FlaggedPage::FlagType type) {
  auto config_proto = GetConfig();
  config_proto->clear_flagged_page();

  std::sort(patterns.begin(), patterns.end());
  for (const auto& pattern : patterns) {
    FlaggedPage* page = config_proto->add_flagged_page();
    page->set_pattern(pattern);
    page->set_type(type);
  }

  SetSafetyTipsRemoteConfigProto(std::move(config_proto));
}

void SetSafetyTipBadRepPatterns(std::vector<std::string> patterns) {
  SetSafetyTipPatternsWithFlagType(patterns, FlaggedPage::BAD_REP);
}

void SetSafetyTipAllowlistPatterns(std::vector<std::string> patterns,
                                   std::vector<std::string> target_patterns,
                                   std::vector<std::string> common_words) {
  auto config_proto = GetConfig();
  config_proto->clear_allowed_pattern();
  config_proto->clear_allowed_target_pattern();
  config_proto->clear_common_word();

  std::sort(patterns.begin(), patterns.end());
  std::sort(target_patterns.begin(), target_patterns.end());
  std::sort(common_words.begin(), common_words.end());

  for (const auto& pattern : patterns) {
    UrlPattern* page = config_proto->add_allowed_pattern();
    page->set_pattern(pattern);
  }
  for (const auto& pattern : target_patterns) {
    HostPattern* page = config_proto->add_allowed_target_pattern();
    page->set_regex(pattern);
  }
  for (const auto& word : common_words) {
    config_proto->add_common_word(word);
  }
  SetSafetyTipsRemoteConfigProto(std::move(config_proto));
}

void InitializeBlankLookalikeAllowlistForTesting() {
  SetSafetyTipAllowlistPatterns({}, {}, {});
}

}  // namespace reputation
