// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_impl.h"

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/feature_list.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/policy/dlp/data_transfer_dlp_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_histogram_helper.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_reporting_manager.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/dbus/dlp/dlp_client.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/policy/core/browser/url_util.h"
#include "components/policy/core/common/policy_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

namespace {

using RuleId = DlpRulesManagerImpl::RuleId;

using UrlConditionId = DlpRulesManagerImpl::UrlConditionId;

using RulesConditionsMap = std::map<RuleId, UrlConditionId>;

DlpRulesManager::Restriction GetClassMapping(const std::string& restriction) {
  static constexpr auto kRestrictionsMap =
      base::MakeFixedFlatMap<base::StringPiece, DlpRulesManager::Restriction>(
          {{dlp::kClipboardRestriction,
            DlpRulesManager::Restriction::kClipboard},
           {dlp::kScreenshotRestriction,
            DlpRulesManager::Restriction::kScreenshot},
           {dlp::kPrintingRestriction, DlpRulesManager::Restriction::kPrinting},
           {dlp::kPrivacyScreenRestriction,
            DlpRulesManager::Restriction::kPrivacyScreen},
           {dlp::kScreenShareRestriction,
            DlpRulesManager::Restriction::kScreenShare},
           {dlp::kFilesRestriction, DlpRulesManager::Restriction::kFiles}});

  auto* it = kRestrictionsMap.find(restriction);
  return (it == kRestrictionsMap.end())
             ? DlpRulesManager::Restriction::kUnknownRestriction
             : it->second;
}

DlpRulesManager::Level GetLevelMapping(const std::string& level) {
  static constexpr auto kLevelsMap =
      base::MakeFixedFlatMap<base::StringPiece, DlpRulesManager::Level>(
          {{dlp::kAllowLevel, DlpRulesManager::Level::kAllow},
           {dlp::kBlockLevel, DlpRulesManager::Level::kBlock},
           {dlp::kWarnLevel, DlpRulesManager::Level::kWarn},
           {dlp::kReportLevel, DlpRulesManager::Level::kReport}});
  auto* it = kLevelsMap.find(level);
  return (it == kLevelsMap.end()) ? DlpRulesManager::Level::kNotSet
                                  : it->second;
}

DlpRulesManager::Component GetComponentMapping(const std::string& component) {
  static constexpr auto kComponentsMap =
      base::MakeFixedFlatMap<base::StringPiece, DlpRulesManager::Component>(
          {{dlp::kArc, DlpRulesManager::Component::kArc},
           {dlp::kCrostini, DlpRulesManager::Component::kCrostini},
           {dlp::kPluginVm, DlpRulesManager::Component::kPluginVm}});

  auto* it = kComponentsMap.find(component);
  return (it == kComponentsMap.end())
             ? DlpRulesManager::Component::kUnknownComponent
             : it->second;
}

// Creates `urls` conditions, saves patterns strings mapping in
// `patterns_mapping`, and saves conditions ids to rules ids mapping in `map`.
void AddUrlConditions(url_matcher::URLMatcher* matcher,
                      UrlConditionId& condition_id,
                      const base::Value* urls,
                      url_matcher::URLMatcherConditionSet::Vector& conditions,
                      std::map<UrlConditionId, std::string>& patterns_mapping,
                      RuleId rule_id,
                      std::map<UrlConditionId, RuleId>& map) {
  DCHECK(urls);
  std::string scheme;
  std::string host;
  uint16_t port = 0;
  std::string path;
  std::string query;
  bool match_subdomains = true;
  for (const auto& list_entry : urls->GetList()) {
    std::string url = list_entry.GetString();
    if (!url_util::FilterToComponents(url, &scheme, &host, &match_subdomains,
                                      &port, &path, &query)) {
      LOG(ERROR) << "Invalid pattern " << url;
      continue;
    }
    auto condition_set = url_util::CreateConditionSet(
        matcher, ++condition_id, scheme, host, match_subdomains, port, path,
        query, /*allow=*/true);

    conditions.push_back(std::move(condition_set));
    map[condition_id] = rule_id;
    patterns_mapping[condition_id] = url;
  }
}

// Matches `url` against `url_matcher` patterns and returns the rules IDs
// configured with the matched patterns.
RulesConditionsMap MatchUrlAndGetRulesMapping(
    const GURL& url,
    const url_matcher::URLMatcher* url_matcher,
    const std::map<UrlConditionId, RuleId>& rules_map) {
  DCHECK(url_matcher);
  const std::set<UrlConditionId> url_conditions_ids =
      url_matcher->MatchURL(url);

  RulesConditionsMap rules_conditions_map;
  for (const auto& id : url_conditions_ids) {
    rules_conditions_map[rules_map.at(id)] = id;
  }
  return rules_conditions_map;
}

// Returns the maximum level of the rules of given `restriction` joined with
// the `selected_rules`.
template <typename T>
std::pair<DlpRulesManager::Level, absl::optional<T>> GetMaxJoinRestrictionLevel(
    const DlpRulesManager::Restriction restriction,
    const std::map<RuleId, T>& selected_rules,
    const std::map<DlpRulesManager::Restriction,
                   std::map<RuleId, DlpRulesManager::Level>>& restrictions_map,
    const bool ignore_allow = false) {
  auto restriction_it = restrictions_map.find(restriction);
  if (restriction_it == restrictions_map.end())
    return std::make_pair(DlpRulesManager::Level::kAllow, absl::nullopt);

  const std::map<RuleId, DlpRulesManager::Level>& restriction_rules =
      restriction_it->second;

  std::pair<DlpRulesManager::Level, absl::optional<T>> max_level =
      std::make_pair(DlpRulesManager::Level::kNotSet, absl::nullopt);

  for (const auto& rule_pair : selected_rules) {
    const auto& restriction_rule_itr = restriction_rules.find(rule_pair.first);
    if (restriction_rule_itr == restriction_rules.end()) {
      continue;
    }
    if (ignore_allow &&
        restriction_rule_itr->second == DlpRulesManager::Level::kAllow) {
      continue;
    }
    if (restriction_rule_itr->second > max_level.first) {
      max_level.first = restriction_rule_itr->second;
      max_level.second = rule_pair.second;
    }
  }

  if (max_level.first == DlpRulesManager::Level::kNotSet)
    return std::make_pair(DlpRulesManager::Level::kAllow, absl::nullopt);

  return max_level;
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void OnSetDlpFilesPolicy(const ::dlp::SetDlpFilesPolicyResponse response) {
  if (response.has_error_message()) {
    LOG(ERROR) << "Failed to set DLP Files policy and start DLP daemon, error: "
               << response.error_message();
  }
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

::dlp::DlpRuleLevel GetLevelProtoEnum(const DlpRulesManager::Level level) {
  static constexpr auto kLevelsMap =
      base::MakeFixedFlatMap<DlpRulesManager::Level, ::dlp::DlpRuleLevel>(
          {{DlpRulesManager::Level::kNotSet, ::dlp::DlpRuleLevel::UNSPECIFIED},
           {DlpRulesManager::Level::kReport, ::dlp::DlpRuleLevel::UNSPECIFIED},
           {DlpRulesManager::Level::kWarn, ::dlp::DlpRuleLevel::UNSPECIFIED},
           {DlpRulesManager::Level::kBlock, ::dlp::DlpRuleLevel::BLOCK},
           {DlpRulesManager::Level::kAllow, ::dlp::DlpRuleLevel::ALLOW}});
  return kLevelsMap.at(level);
}

}  // namespace

DlpRulesManagerImpl::~DlpRulesManagerImpl() {
  DataTransferDlpController::DeleteInstance();
}

// static
void DlpRulesManagerImpl::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(policy_prefs::kDlpReportingEnabled, false);
  registry->RegisterListPref(policy_prefs::kDlpRulesList);
  registry->RegisterIntegerPref(policy_prefs::kDlpClipboardCheckSizeLimit, 0);
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestricted(
    const GURL& source,
    Restriction restriction) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kPrinting ||
         restriction == Restriction::kPrivacyScreen ||
         restriction == Restriction::kScreenshot ||
         restriction == Restriction::kScreenShare);

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  return GetMaxJoinRestrictionLevel(restriction, src_rules_map,
                                    restrictions_map_)
      .first;
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestrictedByAnyRule(
    const GURL& source,
    Restriction restriction) const {
  DCHECK(src_url_matcher_);

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  return GetMaxJoinRestrictionLevel(restriction, src_rules_map,
                                    restrictions_map_, /*ignore_allow=*/true)
      .first;
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestrictedDestination(
    const GURL& source,
    const GURL& destination,
    Restriction restriction,
    std::string* out_source_pattern,
    std::string* out_destination_pattern) const {
  DCHECK(src_url_matcher_);
  DCHECK(dst_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard ||
         restriction == Restriction::kFiles);

  // Allow copy/paste within the same document.
  if (url::Origin::Create(source).IsSameOriginWith(
          url::Origin::Create(destination)))
    return Level::kAllow;

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  const RulesConditionsMap dst_rules_map = MatchUrlAndGetRulesMapping(
      destination, dst_url_matcher_.get(), dst_url_rules_mapping_);

  std::map<DlpRulesManagerImpl::RuleId,
           std::pair<DlpRulesManagerImpl::UrlConditionId,
                     DlpRulesManagerImpl::UrlConditionId>>
      intersection_rules;
  auto src_map_itr = src_rules_map.begin();
  auto dst_map_itr = dst_rules_map.begin();
  while (src_map_itr != src_rules_map.end() &&
         dst_map_itr != dst_rules_map.end()) {
    if (src_map_itr->first < dst_map_itr->first) {
      ++src_map_itr;
    } else if (dst_map_itr->first < src_map_itr->first) {
      ++dst_map_itr;
    } else {
      intersection_rules.insert(std::make_pair(
          src_map_itr->first,
          std::make_pair(src_map_itr->second, dst_map_itr->second)));
      ++src_map_itr;
      ++dst_map_itr;
    }
  }

  std::pair<Level, absl::optional<std::pair<UrlConditionId, UrlConditionId>>>
      level_urls_pair = GetMaxJoinRestrictionLevel(
          restriction, intersection_rules, restrictions_map_);
  if (level_urls_pair.second.has_value() && out_source_pattern &&
      out_destination_pattern) {
    UrlConditionId src_condition_id = level_urls_pair.second.value().first;
    UrlConditionId dst_condition_id = level_urls_pair.second.value().second;
    if (out_source_pattern)
      *out_source_pattern = src_pattterns_mapping_.at(src_condition_id);
    if (out_destination_pattern)
      *out_destination_pattern = dst_pattterns_mapping_.at(dst_condition_id);
  }
  return level_urls_pair.first;
}

DlpRulesManager::Level DlpRulesManagerImpl::IsRestrictedComponent(
    const GURL& source,
    const Component& destination,
    Restriction restriction,
    std::string* out_source_pattern) const {
  DCHECK(src_url_matcher_);
  DCHECK(restriction == Restriction::kClipboard);

  const RulesConditionsMap src_rules_map = MatchUrlAndGetRulesMapping(
      source, src_url_matcher_.get(), src_url_rules_mapping_);

  auto it = components_rules_.find(destination);
  if (it == components_rules_.end())
    return Level::kAllow;

  const std::set<RuleId>& component_rules_ids = it->second;

  RulesConditionsMap intersection_rules;
  auto src_map_itr = src_rules_map.begin();
  auto component_rules_itr = component_rules_ids.begin();
  while (src_map_itr != src_rules_map.end() &&
         component_rules_itr != component_rules_ids.end()) {
    if (src_map_itr->first < *component_rules_itr) {
      ++src_map_itr;
    } else if (*component_rules_itr < src_map_itr->first) {
      ++component_rules_itr;
    } else {
      intersection_rules.insert(*src_map_itr);
      ++src_map_itr;
      ++component_rules_itr;
    }
  }

  std::pair<Level, absl::optional<UrlConditionId>> level_url_pair =
      GetMaxJoinRestrictionLevel(restriction, intersection_rules,
                                 restrictions_map_);
  if (level_url_pair.second.has_value() && out_source_pattern) {
    UrlConditionId src_condition_id = level_url_pair.second.value();
    *out_source_pattern = src_pattterns_mapping_.at(src_condition_id);
  }
  return level_url_pair.first;
}

DlpRulesManagerImpl::DlpRulesManagerImpl(PrefService* local_state) {
  pref_change_registrar_.Init(local_state);
  pref_change_registrar_.Add(
      policy_prefs::kDlpRulesList,
      base::BindRepeating(&DlpRulesManagerImpl::OnPolicyUpdate,
                          base::Unretained(this)));
  OnPolicyUpdate();

  if (!IsReportingEnabled())
    return;
  reporting_manager_ = std::make_unique<DlpReportingManager>();
}

bool DlpRulesManagerImpl::IsReportingEnabled() const {
  return g_browser_process->local_state()->GetBoolean(
      policy_prefs::kDlpReportingEnabled);
}

DlpReportingManager* DlpRulesManagerImpl::GetReportingManager() const {
  return reporting_manager_.get();
}

std::string DlpRulesManagerImpl::GetSourceUrlPattern(const GURL& source_url,
                                                     Restriction restriction,
                                                     Level level) const {
  const std::set<UrlConditionId> url_conditions_ids =
      src_url_matcher_->MatchURL(source_url);

  std::map<RuleId, UrlConditionId> rules_conditions_map;
  for (const auto& condition_id : url_conditions_ids) {
    rules_conditions_map.insert(
        std::make_pair(src_url_rules_mapping_.at(condition_id), condition_id));
  }
  auto restriction_itr = restrictions_map_.find(restriction);
  if (restriction_itr == restrictions_map_.end())
    return std::string();

  const auto rules_levels_map = restriction_itr->second;
  for (const auto& rule_level_entry : rules_levels_map) {
    auto rule_id = rule_level_entry.first;
    auto lvl = rule_level_entry.second;
    auto rule_condition_itr = rules_conditions_map.find(rule_id);
    if (lvl == level && rule_condition_itr != rules_conditions_map.end()) {
      auto condition_id = rule_condition_itr->second;
      auto condition_pattern_itr = src_pattterns_mapping_.find(condition_id);
      if (condition_pattern_itr != src_pattterns_mapping_.end())
        return condition_pattern_itr->second;
    }
  }
  return std::string();
}

size_t DlpRulesManagerImpl::GetClipboardCheckSizeLimitInBytes() const {
  return pref_change_registrar_.prefs()->GetInteger(
      policy_prefs::kDlpClipboardCheckSizeLimit);
}

std::vector<uint64_t> DlpRulesManagerImpl::GetDisallowedFileTransfers(
    const std::vector<FileMetadata>& transferred_files,
    const GURL& destination) const {
  // TODO(crbug.com/1273793): Change to handle VMs, external drive, ...etc.
  std::vector<uint64_t> restricted_files;
  for (const auto& file : transferred_files) {
    Level level = IsRestrictedDestination(
        file.source, destination, Restriction::kFiles, nullptr, nullptr);
    if (level == Level::kBlock)
      restricted_files.push_back(file.inode);
  }
  return restricted_files;
}

void DlpRulesManagerImpl::OnPolicyUpdate() {
  components_rules_.clear();
  restrictions_map_.clear();
  src_url_rules_mapping_.clear();
  dst_url_rules_mapping_.clear();
  src_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  dst_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  src_pattterns_mapping_.clear();
  dst_pattterns_mapping_.clear();
  src_conditions_.clear();
  dst_conditions_.clear();

  if (!base::FeatureList::IsEnabled(features::kDataLeakPreventionPolicy)) {
    return;
  }

  const base::Value* rules_list =
      g_browser_process->local_state()->GetList(policy_prefs::kDlpRulesList);

  DlpBooleanHistogram(dlp::kDlpPolicyPresentUMA,
                      rules_list && !rules_list->GetList().empty());
  if (!rules_list || rules_list->GetList().empty()) {
    DataTransferDlpController::DeleteInstance();
    return;
  }

  RuleId rules_counter = 0;
  UrlConditionId src_url_condition_id = 0;
  UrlConditionId dst_url_condition_id = 0;

  // Constructing request to send the policy to DLP Files daemon.
  ::dlp::SetDlpFilesPolicyRequest request_to_daemon;

  for (const base::Value& rule : rules_list->GetList()) {
    DCHECK(rule.is_dict());
    const auto* sources = rule.FindDictKey("sources");
    DCHECK(sources);
    const auto* sources_urls = sources->FindListKey("urls");
    DCHECK(sources_urls);  // This DCHECK should be removed when other types are
                           // supported as sources.

    AddUrlConditions(src_url_matcher_.get(), src_url_condition_id, sources_urls,
                     src_conditions_, src_pattterns_mapping_, rules_counter,
                     src_url_rules_mapping_);

    const auto* destinations = rule.FindDictKey("destinations");
    const auto* destinations_urls =
        destinations ? destinations->FindListKey("urls") : nullptr;
    if (destinations_urls) {
      AddUrlConditions(dst_url_matcher_.get(), dst_url_condition_id,
                       destinations_urls, dst_conditions_,
                       dst_pattterns_mapping_, rules_counter,
                       dst_url_rules_mapping_);
    }
    const auto* destinations_components =
        destinations ? destinations->FindListKey("components") : nullptr;
    if (destinations_components) {
      for (const auto& component : destinations_components->GetList()) {
        DCHECK(component.is_string());
        components_rules_[GetComponentMapping(component.GetString())].insert(
            rules_counter);
      }
    }

    const auto* restrictions = rule.FindListKey("restrictions");
    DCHECK(restrictions);
    for (const auto& restriction : restrictions->GetList()) {
      const auto* rule_class_str = restriction.FindStringKey("class");
      DCHECK(rule_class_str);
      const auto* rule_level_str = restriction.FindStringKey("level");
      DCHECK(rule_level_str);

      const Restriction rule_restriction = GetClassMapping(*rule_class_str);
      if (rule_restriction == Restriction::kUnknownRestriction)
        continue;

      Level rule_level = GetLevelMapping(*rule_level_str);
      if (rule_level == Level::kNotSet)
        continue;

      // TODO(crbug.com/1172959): Implement Warn level for Files.
      if (rule_restriction == Restriction::kFiles && destinations_urls &&
          !destinations_urls->GetList().empty() && rule_level != Level::kWarn) {
        ::dlp::DlpFilesRule files_rule;
        for (const auto& url : sources_urls->GetList()) {
          DCHECK(url.is_string());
          files_rule.add_source_urls(url.GetString());
        }
        for (const auto& url : destinations_urls->GetList()) {
          DCHECK(url.is_string());
          files_rule.add_destination_urls(url.GetString());
        }
        files_rule.set_level(GetLevelProtoEnum(rule_level));
        request_to_daemon.mutable_rules()->Add(std::move(files_rule));
      }

      DlpRestrictionConfiguredHistogram(rule_restriction);
      restrictions_map_[rule_restriction].emplace(rules_counter, rule_level);
    }
    ++rules_counter;
  }

  src_url_matcher_->AddConditionSets(src_conditions_);
  dst_url_matcher_->AddConditionSets(dst_conditions_);

  if (base::Contains(restrictions_map_, Restriction::kClipboard)) {
    DataTransferDlpController::Init(*this);
  } else {
    DataTransferDlpController::DeleteInstance();
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(crbug.com/1174501) Shutdown the daemon when restrictions are empty.
  if (request_to_daemon.rules_size() > 0 &&
      base::FeatureList::IsEnabled(
          features::kDataLeakPreventionFilesRestriction)) {
    DlpBooleanHistogram(dlp::kFilesDaemonStartedUMA, true);
    chromeos::DlpClient::Get()->SetDlpFilesPolicy(
        request_to_daemon, base::BindOnce(&OnSetDlpFilesPolicy));
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

}  // namespace policy
