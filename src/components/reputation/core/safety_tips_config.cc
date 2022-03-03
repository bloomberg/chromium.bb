// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reputation/core/safety_tips_config.h"

#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"
#include "third_party/re2/src/re2/re2.h"
#include "url/gurl.h"

using safe_browsing::V4ProtocolManagerUtil;

namespace reputation {

namespace {

class SafetyTipsConfigSingleton {
 public:
  void SetProto(std::unique_ptr<SafetyTipsConfig> proto) {
    proto_ = std::move(proto);
  }

  SafetyTipsConfig* GetProto() const { return proto_.get(); }

  static SafetyTipsConfigSingleton& GetInstance() {
    static base::NoDestructor<SafetyTipsConfigSingleton> instance;
    return *instance;
  }

 private:
  std::unique_ptr<SafetyTipsConfig> proto_;
};

// Given a URL, generates all possible variant URLs to check the blocklist for.
// This is conceptually almost identical to safe_browsing::UrlToFullHashes, but
// without the hashing step.
//
// Note: Blocking "a.b/c/" does NOT block http://a.b/c without the trailing /.
void UrlToSafetyTipPatterns(const GURL& url,
                            std::vector<std::string>* patterns) {
  std::string canon_host;
  std::string canon_path;
  std::string canon_query;
  V4ProtocolManagerUtil::CanonicalizeUrl(url, &canon_host, &canon_path,
                                         &canon_query);

  std::vector<std::string> hosts;
  if (url.HostIsIPAddress()) {
    hosts.push_back(url.host());
  } else {
    V4ProtocolManagerUtil::GenerateHostVariantsToCheck(canon_host, &hosts);
  }

  std::vector<std::string> paths;
  V4ProtocolManagerUtil::GeneratePathVariantsToCheck(canon_path, canon_query,
                                                     &paths);

  for (const std::string& host : hosts) {
    for (const std::string& path : paths) {
      DCHECK(path.length() == 0 || path[0] == '/');
      patterns->push_back(host + path);
    }
  }
}

security_state::SafetyTipStatus FlagTypeToSafetyTipStatus(
    FlaggedPage::FlagType type) {
  switch (type) {
    case FlaggedPage::FlagType::FlaggedPage_FlagType_UNKNOWN:
    case FlaggedPage::FlagType::FlaggedPage_FlagType_YOUNG_DOMAIN:
      // Reached if component includes these flags, which might happen to
      // support newer Chrome releases.
      return security_state::SafetyTipStatus::kNone;
    case FlaggedPage::FlagType::FlaggedPage_FlagType_BAD_REP:
      return security_state::SafetyTipStatus::kBadReputation;
  }
  NOTREACHED();
  return security_state::SafetyTipStatus::kNone;
}

}  // namespace

// static
void SetSafetyTipsRemoteConfigProto(std::unique_ptr<SafetyTipsConfig> proto) {
  SafetyTipsConfigSingleton::GetInstance().SetProto(std::move(proto));
}

// static
const SafetyTipsConfig* GetSafetyTipsRemoteConfigProto() {
  return SafetyTipsConfigSingleton::GetInstance().GetProto();
}

bool IsUrlAllowlistedBySafetyTipsComponent(const SafetyTipsConfig* proto,
                                           const GURL& url) {
  DCHECK(proto);
  DCHECK(url.is_valid());
  std::vector<std::string> patterns;
  UrlToSafetyTipPatterns(url, &patterns);
  auto allowed_pages = proto->allowed_pattern();
  for (const auto& pattern : patterns) {
    UrlPattern search_target;
    search_target.set_pattern(pattern);

    auto lower = std::lower_bound(
        allowed_pages.begin(), allowed_pages.end(), search_target,
        [](const UrlPattern& a, const UrlPattern& b) -> bool {
          return a.pattern() < b.pattern();
        });

    if (lower != allowed_pages.end() && pattern == lower->pattern()) {
      return true;
    }
  }
  return false;
}

bool IsTargetHostAllowlistedBySafetyTipsComponent(const SafetyTipsConfig* proto,
                                                  const std::string& hostname) {
  DCHECK(!hostname.empty());
  if (proto == nullptr) {
    return false;
  }
  for (const auto& host_pattern : proto->allowed_target_pattern()) {
    if (!host_pattern.has_regex()) {
      continue;
    }
    DCHECK(!host_pattern.regex().empty());
    const re2::RE2 regex(host_pattern.regex());
    DCHECK(regex.ok());
    if (re2::RE2::FullMatch(hostname, regex)) {
      return true;
    }
  }
  return false;
}

security_state::SafetyTipStatus GetSafetyTipUrlBlockType(const GURL& url) {
  auto* proto = GetSafetyTipsRemoteConfigProto();
  if (!proto) {
    return security_state::SafetyTipStatus::kNone;
  }

  std::vector<std::string> patterns;
  UrlToSafetyTipPatterns(url, &patterns);
  auto flagged_pages = proto->flagged_page();
  for (const auto& pattern : patterns) {
    FlaggedPage search_target;
    search_target.set_pattern(pattern);

    auto lower = std::lower_bound(
        flagged_pages.begin(), flagged_pages.end(), search_target,
        [](const FlaggedPage& a, const FlaggedPage& b) -> bool {
          return a.pattern() < b.pattern();
        });

    while (lower != flagged_pages.end() && pattern == lower->pattern()) {
      // Skip over sites with unexpected flag types and keep looking for other
      // matches. This allows components to include flag types not handled by
      // this release.
      auto type = FlagTypeToSafetyTipStatus(lower->type());
      if (type != security_state::SafetyTipStatus::kNone) {
        return type;
      }
      ++lower;
    }
  }

  return security_state::SafetyTipStatus::kNone;
}

bool IsCommonWordInConfigProto(const SafetyTipsConfig* proto,
                               const std::string& word) {
  // proto is nullptr when running in non-Lookalike tests.
  if (proto == nullptr) {
    return false;
  }

  auto common_words = proto->common_word();
  DCHECK(base::ranges::is_sorted(common_words.begin(), common_words.end()));
  auto lower = std::lower_bound(
      common_words.begin(), common_words.end(), word,
      [](const std::string& a, const std::string& b) -> bool { return a < b; });

  return lower != common_words.end() && word == *lower;
}

}  // namespace reputation
