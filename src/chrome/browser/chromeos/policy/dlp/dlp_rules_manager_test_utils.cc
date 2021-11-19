// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_test_utils.h"

#include <utility>

#include "base/check.h"

namespace policy {

namespace dlp_test_util {

namespace {

const char kName[] = "name";
const char kDescription[] = "description";
const char kSources[] = "sources";
const char kUrls[] = "urls";
const char kDestinations[] = "destinations";
const char kComponents[] = "components";
const char kRestrictions[] = "restrictions";
const char kClass[] = "class";
const char kLevel[] = "level";

}  // namespace

base::Value CreateSources(base::Value urls) {
  base::Value srcs(base::Value::Type::DICTIONARY);
  srcs.SetKey(kUrls, std::move(urls));
  return srcs;
}

base::Value CreateDestinations(base::Value urls, base::Value components) {
  base::Value dsts(base::Value::Type::DICTIONARY);
  dsts.SetKey(kUrls, std::move(urls));
  dsts.SetKey(kComponents, std::move(components));
  return dsts;
}

base::Value CreateRestrictionWithLevel(const std::string& restriction,
                                       const std::string& level) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey(kClass, restriction);
  dict.SetStringKey(kLevel, level);
  return dict;
}

base::Value CreateRule(const std::string& name,
                       const std::string& desc,
                       base::Value src_urls,
                       base::Value dst_urls,
                       base::Value dst_components,
                       base::Value restrictions) {
  base::Value rule(base::Value::Type::DICTIONARY);
  rule.SetStringKey(kName, name);
  rule.SetStringKey(kDescription, desc);
  DCHECK(src_urls.is_list());
  rule.SetKey(kSources, CreateSources(std::move(src_urls)));
  DCHECK(dst_urls.is_list());
  DCHECK(dst_components.is_list());
  rule.SetKey(kDestinations, CreateDestinations(std::move(dst_urls),
                                                std::move(dst_components)));
  DCHECK(restrictions.is_list());
  rule.SetKey(kRestrictions, std::move(restrictions));
  return rule;
}

}  // namespace dlp_test_util

}  // namespace policy
