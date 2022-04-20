// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/app_service/public/cpp/intent_test_util.h"

#include <utility>
#include <vector>

#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"

namespace {

apps::IntentFilterPtr MakeIntentFilter(const std::string& action,
                                       const std::string& mime_type,
                                       const std::string& file_extension,
                                       const std::string& url_pattern,
                                       const std::string& activity_label) {
  DCHECK(!mime_type.empty() || !file_extension.empty() || !url_pattern.empty());
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kAction, action,
                                         apps::PatternMatchType::kNone);

  apps::ConditionValues condition_values;
  if (!mime_type.empty()) {
    condition_values.push_back(std::make_unique<apps::ConditionValue>(
        mime_type, apps::PatternMatchType::kMimeType));
  }
  if (!file_extension.empty()) {
    condition_values.push_back(std::make_unique<apps::ConditionValue>(
        file_extension, apps::PatternMatchType::kFileExtension));
  }
  if (!url_pattern.empty()) {
    condition_values.push_back(std::make_unique<apps::ConditionValue>(
        url_pattern, apps::PatternMatchType::kGlob));
  }
  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kFile, std::move(condition_values)));

  intent_filter->activity_label = activity_label;
  return intent_filter;
}

apps::mojom::IntentFilterPtr CreateIntentFilterForFiles(
    const std::string& action,
    const std::string& mime_type,
    const std::string& file_extension,
    const std::string& url_pattern,
    const std::string& activity_label) {
  DCHECK(!mime_type.empty() || !file_extension.empty() || !url_pattern.empty());
  auto intent_filter = apps::mojom::IntentFilter::New();

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kAction, action,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  if (!mime_type.empty()) {
    condition_values.push_back(apps_util::MakeConditionValue(
        mime_type, apps::mojom::PatternMatchType::kMimeType));
  }
  if (!file_extension.empty()) {
    condition_values.push_back(apps_util::MakeConditionValue(
        file_extension, apps::mojom::PatternMatchType::kFileExtension));
  }
  if (!url_pattern.empty()) {
    condition_values.push_back(apps_util::MakeConditionValue(
        url_pattern, apps::mojom::PatternMatchType::kGlob));
  }
  intent_filter->conditions.push_back(apps_util::MakeCondition(
      apps::mojom::ConditionType::kFile, std::move(condition_values)));

  intent_filter->activity_label = activity_label;
  return intent_filter;
}

}  // namespace

namespace apps_util {

apps::IntentFilterPtr MakeIntentFilterForMimeType(
    const std::string& mime_type) {
  auto intent_filter = std::make_unique<apps::IntentFilter>();

  intent_filter->AddSingleValueCondition(apps::ConditionType::kAction,
                                         kIntentActionSend,
                                         apps::PatternMatchType::kNone);

  std::vector<apps::ConditionValuePtr> condition_values;
  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      mime_type, apps::PatternMatchType::kMimeType));
  intent_filter->conditions.push_back(std::make_unique<apps::Condition>(
      apps::ConditionType::kMimeType, std::move(condition_values)));

  return intent_filter;
}

apps::IntentFilterPtr MakeIntentFilterForSend(
    const std::string& mime_type,
    const std::string& activity_label) {
  return MakeIntentFilter(kIntentActionSend, mime_type, "", "", activity_label);
}

apps::IntentFilterPtr MakeIntentFilterForSendMultiple(
    const std::string& mime_type,
    const std::string& activity_label) {
  return MakeIntentFilter(kIntentActionSendMultiple, mime_type, "", "",
                          activity_label);
}

apps::IntentFilterPtr MakeFileFilterForView(const std::string& mime_type,
                                            const std::string& file_extension,
                                            const std::string& activity_label) {
  return MakeIntentFilter(kIntentActionView, mime_type, file_extension, "",
                          activity_label);
}

apps::IntentFilterPtr MakeURLFilterForView(const std::string& url_pattern,
                                           const std::string& activity_label) {
  return MakeIntentFilter(kIntentActionView, "", "", url_pattern,
                          activity_label);
}

apps::IntentFilterPtr MakeSchemeOnlyFilter(const std::string& scheme) {
  apps::ConditionValues condition_values;
  condition_values.push_back(std::make_unique<apps::ConditionValue>(
      scheme, apps::PatternMatchType::kNone));
  auto condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(condition_values));

  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->conditions.push_back(std::move(condition));

  return intent_filter;
}

apps::IntentFilterPtr MakeSchemeAndHostOnlyFilter(const std::string& scheme,
                                                  const std::string& host) {
  apps::ConditionValues scheme_condition_values;
  scheme_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      scheme, apps::PatternMatchType::kNone));
  auto scheme_condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kScheme, std::move(scheme_condition_values));

  apps::ConditionValues host_condition_values;
  host_condition_values.push_back(std::make_unique<apps::ConditionValue>(
      host, apps::PatternMatchType::kNone));
  auto host_condition = std::make_unique<apps::Condition>(
      apps::ConditionType::kHost, std::move(host_condition_values));

  auto intent_filter = std::make_unique<apps::IntentFilter>();
  intent_filter->conditions.push_back(std::move(scheme_condition));
  intent_filter->conditions.push_back(std::move(host_condition));

  return intent_filter;
}

void AddConditionValue(apps::ConditionType condition_type,
                       const std::string& value,
                       apps::PatternMatchType pattern_match_type,
                       apps::IntentFilterPtr& intent_filter) {
  for (auto& condition : intent_filter->conditions) {
    if (condition->condition_type == condition_type) {
      condition->condition_values.push_back(
          std::make_unique<apps::ConditionValue>(value, pattern_match_type));
      return;
    }
  }
  intent_filter->AddSingleValueCondition(condition_type, value,
                                         pattern_match_type);
}

apps::mojom::IntentFilterPtr CreateSchemeOnlyFilter(const std::string& scheme) {
  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  condition_values.push_back(apps_util::MakeConditionValue(
      scheme, apps::mojom::PatternMatchType::kNone));
  auto condition = apps_util::MakeCondition(apps::mojom::ConditionType::kScheme,
                                            std::move(condition_values));

  auto intent_filter = apps::mojom::IntentFilter::New();
  intent_filter->conditions.push_back(std::move(condition));

  return intent_filter;
}

apps::mojom::IntentFilterPtr CreateSchemeAndHostOnlyFilter(
    const std::string& scheme,
    const std::string& host) {
  std::vector<apps::mojom::ConditionValuePtr> scheme_condition_values;
  scheme_condition_values.push_back(apps_util::MakeConditionValue(
      scheme, apps::mojom::PatternMatchType::kNone));
  auto scheme_condition = apps_util::MakeCondition(
      apps::mojom::ConditionType::kScheme, std::move(scheme_condition_values));

  std::vector<apps::mojom::ConditionValuePtr> host_condition_values;
  host_condition_values.push_back(apps_util::MakeConditionValue(
      host, apps::mojom::PatternMatchType::kNone));
  auto host_condition = apps_util::MakeCondition(
      apps::mojom::ConditionType::kHost, std::move(host_condition_values));

  auto intent_filter = apps::mojom::IntentFilter::New();
  intent_filter->conditions.push_back(std::move(scheme_condition));
  intent_filter->conditions.push_back(std::move(host_condition));

  return intent_filter;
}

apps::mojom::IntentFilterPtr CreateIntentFilterForSend(
    const std::string& mime_type,
    const std::string& activity_label) {
  return CreateIntentFilterForFiles(kIntentActionSend, mime_type, "", "",
                                    activity_label);
}

apps::mojom::IntentFilterPtr CreateIntentFilterForMimeType(
    const std::string& mime_type) {
  auto intent_filter = apps::mojom::IntentFilter::New();

  apps_util::AddSingleValueCondition(
      apps::mojom::ConditionType::kAction, kIntentActionSend,
      apps::mojom::PatternMatchType::kNone, intent_filter);

  std::vector<apps::mojom::ConditionValuePtr> condition_values;
  condition_values.push_back(apps_util::MakeConditionValue(
      mime_type, apps::mojom::PatternMatchType::kMimeType));
  intent_filter->conditions.push_back(apps_util::MakeCondition(
      apps::mojom::ConditionType::kMimeType, std::move(condition_values)));

  return intent_filter;
}

apps::mojom::IntentFilterPtr CreateIntentFilterForSendMultiple(
    const std::string& mime_type,
    const std::string& activity_label) {
  return CreateIntentFilterForFiles(kIntentActionSendMultiple, mime_type, "",
                                    "", activity_label);
}

apps::mojom::IntentFilterPtr CreateFileFilterForView(
    const std::string& mime_type,
    const std::string& file_extension,
    const std::string& activity_label) {
  return CreateIntentFilterForFiles(kIntentActionView, mime_type,
                                    file_extension, "", activity_label);
}

apps::mojom::IntentFilterPtr CreateURLFilterForView(
    const std::string& url_pattern,
    const std::string& activity_label) {
  return CreateIntentFilterForFiles(kIntentActionView, "", "", url_pattern,
                                    activity_label);
}

void AddConditionValue(apps::mojom::ConditionType condition_type,
                       const std::string& value,
                       apps::mojom::PatternMatchType pattern_match_type,
                       apps::mojom::IntentFilterPtr& intent_filter) {
  for (auto& condition : intent_filter->conditions) {
    if (condition->condition_type == condition_type) {
      condition->condition_values.push_back(
          MakeConditionValue(value, pattern_match_type));
      return;
    }
  }
  AddSingleValueCondition(condition_type, value, pattern_match_type,
                          intent_filter);
}

}  // namespace apps_util
