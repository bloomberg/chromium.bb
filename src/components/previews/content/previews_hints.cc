// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_hints.h"

#include <memory>
#include <string>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "components/optimization_guide/optimization_guide_service_observer.h"
#include "components/optimization_guide/url_pattern_with_wildcards.h"
#include "components/previews/core/previews_features.h"
#include "url/gurl.h"

namespace previews {

namespace {

// Name of sentinel file to guard potential crash loops while processing
// the config into hints. It holds the version of the config that is/was
// being processed into hints.
const base::FilePath::CharType kSentinelFileName[] =
    FILE_PATH_LITERAL("previews_config_sentinel.txt");

// Creates the sentinel file (at |sentinel_path|) to persistently mark the
// beginning of processing the configuration data for Previews hints. It
// records the configuration version in the file. Returns true when the
// sentinel file is successfully created and processing should continue.
// Returns false if the processing should not continue because the
// file exists with the same version (indicating that processing that version
// failed previously (possibly crash or shutdown). Should be run in the
// background (e.g., same task as Hints.CreateFromConfig).
bool CreateSentinelFile(const base::FilePath& sentinel_path,
                        const base::Version& version) {
  DCHECK(version.IsValid());

  if (base::PathExists(sentinel_path)) {
    // Processing apparently did not complete previously, check its version.
    std::string content;
    if (!base::ReadFileToString(sentinel_path, &content)) {
      DLOG(WARNING) << "Error reading previews config sentinel file";
      // Attempt to delete sentinel for fresh start next time.
      base::DeleteFile(sentinel_path, false /* recursive */);
      return false;
    }
    base::Version previous_attempted_version(content);
    if (!previous_attempted_version.IsValid()) {
      DLOG(ERROR) << "Bad contents in previews config sentinel file";
      // Attempt to delete sentinel for fresh start next time.
      base::DeleteFile(sentinel_path, false /* recursive */);
      return false;
    }
    if (previous_attempted_version.CompareTo(version) == 0) {
      // Previously attempted same version without completion.
      return false;
    }
  }

  // Write config version in the sentinel file.
  std::string new_sentinel_value = version.GetString();
  if (base::WriteFile(sentinel_path, new_sentinel_value.data(),
                      new_sentinel_value.length()) <= 0) {
    DLOG(ERROR) << "Failed to create sentinel file " << sentinel_path;
    return false;
  }
  return true;
}

// Deletes the sentinel file. This should be done once processing the
// configuration is complete and should be done in the background (e.g.,
// same task as Hints.CreateFromConfig).
void DeleteSentinelFile(const base::FilePath& sentinel_path) {
  if (!base::DeleteFile(sentinel_path, false /* recursive */))
    DLOG(ERROR) << "Error deleting sentinel file";
}

// Enumerates the possible outcomes of processing previews hints. Used in UMA
// histograms, so the order of enumerators should not be changed.
//
// Keep in sync with PreviewsProcessHintsResult in
// tools/metrics/histograms/enums.xml.
enum class PreviewsProcessHintsResult {
  PROCESSED_NO_PREVIEWS_HINTS = 0,
  PROCESSED_PREVIEWS_HINTS = 1,
  FAILED_FINISH_PROCESSING = 2,

  // Insert new values before this line.
  MAX,
};

// Returns base::nullopt if |optimization_type| can't be converted.
base::Optional<PreviewsType>
ConvertProtoOptimizationTypeToPreviewsOptimizationType(
    optimization_guide::proto::OptimizationType optimization_type) {
  switch (optimization_type) {
    case optimization_guide::proto::TYPE_UNSPECIFIED:
      return base::nullopt;
    case optimization_guide::proto::NOSCRIPT:
      return PreviewsType::NOSCRIPT;
    case optimization_guide::proto::RESOURCE_LOADING:
      return PreviewsType::RESOURCE_LOADING_HINTS;
  }
}

// Returns whether any features using page hints are enabled.
bool ShouldProcessPageHints() {
  return previews::params::IsResourceLoadingHintsEnabled();
}

bool IsDisabledExperimentalOptimization(
    const optimization_guide::proto::Optimization& optimization) {
  // If this optimization has been marked with an experiment name, consider it
  // disabled unless an experiment with that name is running. Experiment names
  // are configured with the experiment_name parameter to the
  // kOptimizationHintsExperiments feature.
  //
  // If kOptimizationHintsExperiments is disabled, getting the param value
  // returns an empty string. Since experiment names are not allowed to be
  // empty strings, all experiments will be disabled if the feature is
  // disabled.
  if (optimization.has_experiment_name() &&
      !optimization.experiment_name().empty() &&
      optimization.experiment_name() !=
          base::GetFieldTrialParamValueByFeature(
              features::kOptimizationHintsExperiments,
              features::kOptimizationHintsExperimentNameParam)) {
    return true;
  }
  return false;
}

void RecordProcessHintsResult(PreviewsProcessHintsResult result) {
  UMA_HISTOGRAM_ENUMERATION("Previews.ProcessHintsResult",
                            static_cast<int>(result),
                            static_cast<int>(PreviewsProcessHintsResult::MAX));
}

}  // namespace

PreviewsHints::PreviewsHints() {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

PreviewsHints::~PreviewsHints() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

// static
std::unique_ptr<PreviewsHints> PreviewsHints::CreateFromConfig(
    const optimization_guide::proto::Configuration& config,
    const optimization_guide::ComponentInfo& info) {
  base::FilePath sentinel_path(
      info.hints_path.DirName().Append(kSentinelFileName));
  if (!CreateSentinelFile(sentinel_path, info.hints_version)) {
    std::unique_ptr<PreviewsHints> no_hints;
    RecordProcessHintsResult(
        PreviewsProcessHintsResult::FAILED_FINISH_PROCESSING);
    return no_hints;
  }

  std::unique_ptr<PreviewsHints> hints(new PreviewsHints());

  // The condition set ID is a simple increasing counter that matches the
  // order of hints in the config (where earlier hints in the config take
  // precendence over later hints in the config if there are multiple matches).
  url_matcher::URLMatcherConditionSet::ID id = 0;
  url_matcher::URLMatcherConditionFactory* condition_factory =
      hints->url_matcher_.condition_factory();
  url_matcher::URLMatcherConditionSet::Vector all_conditions;
  std::set<std::string> seen_host_suffixes;

  size_t total_resource_loading_hints_received = 0;
  size_t total_page_patterns_with_resource_loading_hints_received = 0;
  // Process hint configuration.
  for (const auto hint : config.hints()) {
    // We only support host suffixes at the moment. Skip anything else.
    // One |hint| applies to one host URL suffix.
    if (hint.key_representation() != optimization_guide::proto::HOST_SUFFIX)
      continue;

    // Validate configuration keys.
    DCHECK(!hint.key().empty());
    if (hint.key().empty())
      continue;

    auto seen_host_suffixes_iter = seen_host_suffixes.find(hint.key());
    DCHECK(seen_host_suffixes_iter == seen_host_suffixes.end());
    if (seen_host_suffixes_iter != seen_host_suffixes.end()) {
      DLOG(WARNING) << "Received config with duplicate key";
      continue;
    }
    seen_host_suffixes.insert(hint.key());

    // Create whitelist condition set out of the optimizations that are
    // whitelisted for the host suffix at the top level (i.e., not within
    // PageHints).
    std::set<std::pair<PreviewsType, int>> whitelisted_optimizations;
    for (const auto optimization : hint.whitelisted_optimizations()) {
      if (IsDisabledExperimentalOptimization(optimization)) {
        continue;
      }
      base::Optional<PreviewsType> previews_type =
          ConvertProtoOptimizationTypeToPreviewsOptimizationType(
              optimization.optimization_type());
      if (previews_type.has_value()) {
        whitelisted_optimizations.insert(std::make_pair(
            previews_type.value(), optimization.inflation_percent()));
      }
    }
    url_matcher::URLMatcherCondition condition =
        condition_factory->CreateHostSuffixCondition(hint.key());
    all_conditions.push_back(new url_matcher::URLMatcherConditionSet(
        id, std::set<url_matcher::URLMatcherCondition>{condition}));
    hints->whitelist_[id] = whitelisted_optimizations;
    id++;

    // Cache hints that have PageHints.
    if (ShouldProcessPageHints() && !hint.page_hints().empty()) {
      UMA_HISTOGRAM_COUNTS("ResourceLoadingHints.PageHints.ProcessedCount",
                           hint.page_hints().size());

      for (const auto& page_hint : hint.page_hints()) {
        for (const auto& optimization : page_hint.whitelisted_optimizations()) {
          base::Optional<PreviewsType> previews_type =
              ConvertProtoOptimizationTypeToPreviewsOptimizationType(
                  optimization.optimization_type());

          if (!previews_type ||
              previews_type != PreviewsType::RESOURCE_LOADING_HINTS) {
            continue;
          }
          total_page_patterns_with_resource_loading_hints_received++;
          total_resource_loading_hints_received +=
              optimization.resource_loading_hints().size();
        }
      }

      if (total_page_patterns_with_resource_loading_hints_received <=
          previews::params::GetMaxPageHintsInMemoryThreshhold()) {
        hints->initial_hints_.push_back(hint);
      }
    }
  }
  if (ShouldProcessPageHints()) {
    UMA_HISTOGRAM_COUNTS_100000(
        "ResourceLoadingHints.ResourceHints.TotalReceived",
        total_resource_loading_hints_received);
    UMA_HISTOGRAM_COUNTS_1000(
        "ResourceLoadingHints.PageHints.TotalReceived",
        total_page_patterns_with_resource_loading_hints_received);
  }
  hints->url_matcher_.AddConditionSets(all_conditions);

  // Completed processing hints data without crashing so clear sentinel.
  DeleteSentinelFile(sentinel_path);
  RecordProcessHintsResult(
      all_conditions.empty()
          ? PreviewsProcessHintsResult::PROCESSED_NO_PREVIEWS_HINTS
          : PreviewsProcessHintsResult::PROCESSED_PREVIEWS_HINTS);
  return hints;
}

// static
const optimization_guide::proto::PageHint* PreviewsHints::FindPageHint(
    const GURL& document_url,
    const optimization_guide::proto::Hint& hint) {
  if (hint.page_hints_size() == 0)
    return nullptr;
  std::string url = document_url.spec();
  for (const auto& page_hint : hint.page_hints()) {
    if (page_hint.page_pattern().empty())
      continue;
    optimization_guide::URLPatternWithWildcards url_pattern(
        page_hint.page_pattern());
    if (url_pattern.Matches(url)) {
      // Return the first matching page hint.
      return &page_hint;
    }
  }
  return nullptr;
}

void PreviewsHints::Initialize() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!initial_hints_.empty()) {
    if (!hint_cache_)
      hint_cache_ = std::make_unique<HintCache>();
    hint_cache_->AddHints(initial_hints_);
    initial_hints_.clear();
  }
}

bool PreviewsHints::IsWhitelisted(const GURL& url,
                                  PreviewsType type,
                                  int* out_inflation_percent) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!url.has_host())
    return false;

  // First check for being whitelisted at top level for host suffix.
  std::set<url_matcher::URLMatcherConditionSet::ID> matches =
      url_matcher_.MatchURL(url);

  // Only consider the first match in iteration order as it takes precedence
  // if there are multiple matches for the top-level whitelist.
  const auto& first_match = matches.begin();
  if (first_match != matches.end()) {
    const auto whitelist_iter = whitelist_.find(*first_match);
    if (whitelist_iter != whitelist_.end()) {
      const auto& whitelisted_optimizations = whitelist_iter->second;
      for (auto optimization_iter = whitelisted_optimizations.begin();
           optimization_iter != whitelisted_optimizations.end();
           ++optimization_iter) {
        if (optimization_iter->first == type) {
          *out_inflation_percent = optimization_iter->second;
          // Whitelisted on top level whitelist.
          return true;
        }
      }
    }
  }

  if (!hint_cache_)
    return false;

  // Now check HintCache for a loaded entry with a PageHint that matches |url|
  // that whitelists the optimization.
  std::string host = url.host();
  if (!hint_cache_->IsHintLoaded(host)) {
    // TODO(dougarnett): Add UMA histogram counts for both cases of HasHint().
    return false;
  }

  const optimization_guide::proto::Hint* hint = hint_cache_->GetHint(host);
  const optimization_guide::proto::PageHint* matched_page_hint =
      FindPageHint(url, *hint);
  if (!matched_page_hint) {
    return false;
  }

  for (const auto& optimization :
       matched_page_hint->whitelisted_optimizations()) {
    if (ConvertProtoOptimizationTypeToPreviewsOptimizationType(
            optimization.optimization_type()) == type) {
      *out_inflation_percent = optimization.inflation_percent();
      return true;
    }
  }

  return false;
}

bool PreviewsHints::MaybeLoadOptimizationHints(
    const GURL& url,
    HintLoadedCallback callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!hint_cache_ || !url.has_host())
    return false;

  hint_cache_->LoadHint(url.host(), std::move(callback));
  return hint_cache_->HasHint(url.host());
}

}  // namespace previews
