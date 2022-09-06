// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/browsing_topics_calculator.h"

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/browsing_topics/util.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/privacy_sandbox/canonical_topic.h"
#include "components/privacy_sandbox/privacy_sandbox_settings.h"
#include "content/public/browser/browsing_topics_site_data_manager.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/features.h"

namespace browsing_topics {

namespace {

void RecordCalculatorResultMetrics(
    const BrowsingTopicsCalculator::CalculatorResultStatus& status,
    const EpochTopics& epoch_topics) {
  base::UmaHistogramEnumeration(
      "BrowsingTopics.EpochTopicsCalculation.CalculatorResultStatus", status);

  ukm::UkmRecorder* ukm_recorder = ukm::UkmRecorder::Get();
  ukm::builders::BrowsingTopics_EpochTopicsCalculationResult builder(
      ukm::NoURLSourceId());

  if (status == BrowsingTopicsCalculator::CalculatorResultStatus::kSuccess) {
    const std::vector<TopicAndDomains>& topics =
        epoch_topics.top_topics_and_observing_domains();

    if (topics.size() > 0 && topics[0].IsValid())
      builder.SetTopTopic0(topics[0].topic().value());

    if (topics.size() > 1 && topics[1].IsValid())
      builder.SetTopTopic1(topics[1].topic().value());

    if (topics.size() > 2 && topics[2].IsValid())
      builder.SetTopTopic2(topics[2].topic().value());

    if (topics.size() > 3 && topics[3].IsValid())
      builder.SetTopTopic3(topics[3].topic().value());

    if (topics.size() > 4 && topics[4].IsValid())
      builder.SetTopTopic4(topics[4].topic().value());

    builder.SetTaxonomyVersion(epoch_topics.taxonomy_version())
        .SetModelVersion(epoch_topics.model_version())
        .SetPaddedTopicsStartIndex(
            epoch_topics.padded_top_topics_start_index());
  }

  builder.Record(ukm_recorder->Get());
}

// Derive the mapping from hosts to topics and the mapping from topics to hosts.
// Precondition: the annotation didn't fail in general (e.g. `ModelInfo` is
// valid).
void DeriveHostTopicsMapAndTopicHostsMap(
    const std::vector<optimization_guide::BatchAnnotationResult>& results,
    std::map<HashedHost, std::set<Topic>>& host_topics_map,
    std::map<Topic, std::set<HashedHost>>& topic_hosts_map) {
  DCHECK(host_topics_map.empty());
  DCHECK(topic_hosts_map.empty());

  for (size_t i = 0; i < results.size(); ++i) {
    const optimization_guide::BatchAnnotationResult& result = results[i];
    const std::string& original_host = result.input();

    const absl::optional<std::vector<optimization_guide::WeightedIdentifier>>&
        annotation_result_topics = result.topics();
    if (!annotation_result_topics)
      continue;

    HashedHost host = HashMainFrameHostForStorage(original_host);

    for (const optimization_guide::WeightedIdentifier& annotation_result_topic :
         *annotation_result_topics) {
      // Note that `annotation_result_topic.weight()` is ignored. This is the
      // intended use of the model for the Topics API.
      Topic topic = Topic(annotation_result_topic.value());

      topic_hosts_map[topic].insert(host);
      host_topics_map[host].insert(topic);
    }
  }
}

// For `topic`, derive the context domains that observed it. This is done by
// first getting the hosts about `topic` from `topic_hosts_map`, and
// for each site, get the callers (context domains) that were on that site and
// add the callers to a result set.
std::set<HashedDomain> GetTopicObservationDomains(
    const Topic& topic,
    const std::map<Topic, std::set<HashedHost>>& topic_hosts_map,
    const std::map<HashedHost, std::vector<HashedDomain>>&
        host_context_domains_map) {
  std::set<HashedDomain> topic_observation_domains;

  // If `topic` was padded, it may not exist in `topic_hosts_map`. In this
  // case, return an empty set.
  auto it = topic_hosts_map.find(topic);
  if (it == topic_hosts_map.end())
    return std::set<HashedDomain>();

  const std::set<HashedHost>& hosts = it->second;

  for (const HashedHost& host : hosts) {
    // `host` came from the history database, and it may not exist in the
    // `host_context_domains_map` which came from the usage contexts
    // database, due to e.g. per-context data deletion, database errors, etc.
    // In this case, continue checking other hosts.
    auto it = host_context_domains_map.find(host);
    if (it == host_context_domains_map.end())
      continue;

    const std::vector<HashedDomain>& context_domains = it->second;

    for (const HashedDomain& context_domain : context_domains) {
      topic_observation_domains.insert(context_domain);

      // To limit memory usage, cap the number of context domains to keep
      // per-topic. The larger `HashedDomain`s will be kept. This is fair, as
      // the hashing for context domains is per-user, so we are not
      // prioritizing any domains in general.
      if (topic_observation_domains.size() >
          static_cast<size_t>(
              blink::features::
                  kBrowsingTopicsMaxNumberOfApiUsageContextDomainsToKeepPerTopic
                      .Get())) {
        topic_observation_domains.erase(topic_observation_domains.begin());
      }
    }
  }

  return topic_observation_domains;
}

}  // namespace

BrowsingTopicsCalculator::BrowsingTopicsCalculator(
    privacy_sandbox::PrivacySandboxSettings* privacy_sandbox_settings,
    history::HistoryService* history_service,
    content::BrowsingTopicsSiteDataManager* site_data_manager,
    optimization_guide::PageContentAnnotationsService* annotations_service,
    CalculateCompletedCallback callback)
    : privacy_sandbox_settings_(privacy_sandbox_settings),
      history_service_(history_service),
      site_data_manager_(site_data_manager),
      annotations_service_(annotations_service),
      calculate_completed_callback_(std::move(callback)),
      calculation_time_(base::Time::Now()) {
  // Continue asynchronously so that `calculate_completed_callback_` isn't
  // called synchronously while `this` is being constructed.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&BrowsingTopicsCalculator::CheckCanCalculate,
                                weak_ptr_factory_.GetWeakPtr()));
}

BrowsingTopicsCalculator::~BrowsingTopicsCalculator() = default;

uint64_t BrowsingTopicsCalculator::GenerateRandUint64() {
  return base::RandUint64();
}

void BrowsingTopicsCalculator::DeriveTopTopics(
    const std::map<HashedHost, size_t>& history_hosts_count,
    const std::map<HashedHost, std::set<Topic>>& host_topics_map,
    size_t taxonomy_size,
    std::vector<Topic>& top_topics,
    size_t& padded_top_topics_start_index) {
  DCHECK(top_topics.empty());
  DCHECK_EQ(padded_top_topics_start_index, 0u);

  // Derive the frequency of each topic, by summing up the frequencies of the
  // associated hosts. TODO(yaoxia): consider applying inverse frequency of
  // topics (https://github.com/jkarlin/topics/issues/42).
  std::map<Topic, size_t> topics_count;
  for (auto const& [host, host_count] : history_hosts_count) {
    // A host wouldn't be found if there were no topics associated with it.
    auto it = host_topics_map.find(host);
    if (it == host_topics_map.end())
      continue;

    const std::set<Topic>& topics = it->second;
    for (const Topic& topic : topics) {
      topics_count[topic] += host_count;
    }
  }

  DCHECK_LE(
      static_cast<size_t>(
          blink::features::kBrowsingTopicsNumberOfTopTopicsPerEpoch.Get()),
      taxonomy_size);

  // Get the top up to `kBrowsingTopicsNumberOfTopTopicsPerEpoch` topics,
  // sorted by decreasing count.
  std::vector<std::pair<Topic, size_t>> top_topics_count(std::min(
      static_cast<size_t>(
          blink::features::kBrowsingTopicsNumberOfTopTopicsPerEpoch.Get()),
      topics_count.size()));

  std::partial_sort_copy(
      topics_count.begin(), topics_count.end(), top_topics_count.begin(),
      top_topics_count.end(),
      [](auto& left, auto& right) { return left.second > right.second; });

  std::transform(top_topics_count.begin(), top_topics_count.end(),
                 std::back_inserter(top_topics),
                 [](auto& topic_count) { return topic_count.first; });

  padded_top_topics_start_index = top_topics.size();

  // Pad the top topics with distinct random topics until we have
  // `kBrowsingTopicsNumberOfTopTopicsPerEpoch` topics.
  while (top_topics.size() <
         static_cast<size_t>(
             blink::features::kBrowsingTopicsNumberOfTopTopicsPerEpoch.Get())) {
    Topic padded_topic(0);

    do {
      int padded_topic_index =
          base::checked_cast<int>(GenerateRandUint64() % taxonomy_size);
      padded_topic = Topic(padded_topic_index + 1);
    } while (base::Contains(top_topics, padded_topic));

    top_topics.emplace_back(std::move(padded_topic));
  }
}

void BrowsingTopicsCalculator::CheckCanCalculate() {
  if (!privacy_sandbox_settings_->IsTopicsAllowed()) {
    OnCalculateCompleted(CalculatorResultStatus::kFailurePermissionDenied);
    return;
  }

  // Get the the api usages context map (from the calling context domain to a
  // set of history hosts) so that we can figure out which topics the APIs were
  // called on.
  site_data_manager_->GetBrowsingTopicsApiUsage(
      /*begin_time=*/DeriveApiUsageContextDataStartTime(
          calculation_time_,
          privacy_sandbox_settings_->TopicsDataAccessibleSince()),
      /*end_time=*/calculation_time_,
      base::BindOnce(&BrowsingTopicsCalculator::
                         OnGetRecentBrowsingTopicsApiUsagesCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void BrowsingTopicsCalculator::OnGetRecentBrowsingTopicsApiUsagesCompleted(
    browsing_topics::ApiUsageContextQueryResult result) {
  DCHECK(host_context_domains_map_.empty());

  if (!result.success) {
    OnCalculateCompleted(
        CalculatorResultStatus::kFailureApiUsageContextQueryError);
    return;
  }

  for (const ApiUsageContext& usage_context : result.api_usage_contexts) {
    host_context_domains_map_[usage_context.hashed_main_frame_host]
        .emplace_back(usage_context.hashed_context_domain);
  }

  // `ApiUsageContext::hashed_main_frame_host` is a hashed number. To get the
  // topic associated with it, we will need to match it against a set of raw
  // hosts with topics. Thus, here we query the history with the larger time
  // range (from DeriveApiUsageContextDataStartTime() to `calculation_time_`) to
  // get the raw hosts.
  history::QueryOptions options;
  options.begin_time = DeriveApiUsageContextDataStartTime(
      calculation_time_,
      privacy_sandbox_settings_->TopicsDataAccessibleSince());
  options.end_time = calculation_time_;
  options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;

  history_service_->QueryHistory(
      std::u16string(), options,
      base::BindOnce(
          &BrowsingTopicsCalculator::OnGetRecentlyVisitedURLsCompleted,
          weak_ptr_factory_.GetWeakPtr()),
      &history_task_tracker_);
}

void BrowsingTopicsCalculator::OnGetRecentlyVisitedURLsCompleted(
    history::QueryResults results) {
  DCHECK(history_hosts_count_.empty());

  std::set<std::string> raw_hosts;

  for (const history::URLResult& url_result : results) {
    if (!(url_result.content_annotations().annotation_flags &
          history::VisitContentAnnotationFlag::kBrowsingTopicsEligible)) {
      continue;
    }

    std::string raw_host = url_result.url().host();
    raw_hosts.insert(raw_host);

    if (url_result.visit_time() >=
        DeriveHistoryDataStartTime(
            calculation_time_,
            privacy_sandbox_settings_->TopicsDataAccessibleSince())) {
      HashedHost host = HashMainFrameHostForStorage(raw_host);
      history_hosts_count_[host]++;
    }
  }

  base::UmaHistogramCounts1000(
      "BrowsingTopics.EpochTopicsCalculation.EligibleDistinctHistoryHostsCount",
      history_hosts_count_.size());

  // When the input is empty, we still want to wait for the model availability
  // status to be known, before querying the model version. Thus we simply
  // always call `RequestAndNotifyWhenModelAvailable()` first. If the model
  // availability status is already known, the function will be cheap and the
  // callback will be synchronously called.
  annotations_service_->RequestAndNotifyWhenModelAvailable(
      optimization_guide::AnnotationType::kPageTopics,
      base::BindOnce(
          &BrowsingTopicsCalculator::OnRequestModelCompleted,
          weak_ptr_factory_.GetWeakPtr(),
          std::vector<std::string>(raw_hosts.begin(), raw_hosts.end())));
}

void BrowsingTopicsCalculator::OnRequestModelCompleted(
    std::vector<std::string> raw_hosts,
    bool successful) {
  // Ignore `successful`. In `OnGetTopicsForHostsCompleted()`, it will need to
  // check the model again anyway in case there's a race.
  if (raw_hosts.empty()) {
    OnGetTopicsForHostsCompleted(/*results=*/{});
    return;
  }

  annotations_service_->BatchAnnotate(
      base::BindOnce(&BrowsingTopicsCalculator::OnGetTopicsForHostsCompleted,
                     weak_ptr_factory_.GetWeakPtr()),
      raw_hosts, optimization_guide::AnnotationType::kPageTopics);
}

void BrowsingTopicsCalculator::OnGetTopicsForHostsCompleted(
    const std::vector<optimization_guide::BatchAnnotationResult>& results) {
  absl::optional<optimization_guide::ModelInfo> model_info =
      annotations_service_->GetModelInfoForType(
          optimization_guide::AnnotationType::kPageTopics);

  if (!model_info) {
    OnCalculateCompleted(
        CalculatorResultStatus::kFailureAnnotationExecutionError);
    return;
  }

  absl::optional<size_t> taxonomy_size = GetTaxonomySize();
  if (!taxonomy_size) {
    OnCalculateCompleted(
        CalculatorResultStatus::kFailureTaxonomyVersionNotSupportedInBinary);
    return;
  }

  const int64_t model_version = model_info->GetVersion();
  DCHECK_GT(model_version, 0);

  std::map<HashedHost, std::set<Topic>> host_topics_map;
  std::map<Topic, std::set<HashedHost>> topic_hosts_map;
  DeriveHostTopicsMapAndTopicHostsMap(results, host_topics_map,
                                      topic_hosts_map);

  std::vector<Topic> top_topics;
  size_t padded_top_topics_start_index = 0u;
  DeriveTopTopics(history_hosts_count_, host_topics_map, *taxonomy_size,
                  top_topics, padded_top_topics_start_index);

  base::UmaHistogramCounts100(
      "BrowsingTopics.EpochTopicsCalculation.TopTopicsCountBeforePadding",
      padded_top_topics_start_index);

  // For each top topic, derive the context domains that observed it
  std::vector<TopicAndDomains> top_topics_and_observing_domains;

  for (const Topic& topic : top_topics) {
    if (!privacy_sandbox_settings_->IsTopicAllowed(
            privacy_sandbox::CanonicalTopic(
                topic,
                blink::features::kBrowsingTopicsTaxonomyVersion.Get()))) {
      top_topics_and_observing_domains.emplace_back(TopicAndDomains());
      continue;
    }

    std::set<HashedDomain> topic_observation_domains =
        GetTopicObservationDomains(topic, topic_hosts_map,
                                   host_context_domains_map_);

    base::UmaHistogramCounts1000(
        "BrowsingTopics.EpochTopicsCalculation."
        "ObservationContextDomainsCountPerTopTopic",
        topic_observation_domains.size());

    top_topics_and_observing_domains.emplace_back(
        TopicAndDomains(topic, std::move(topic_observation_domains)));
  }

  OnCalculateCompleted(
      CalculatorResultStatus::kSuccess,
      EpochTopics(std::move(top_topics_and_observing_domains),
                  padded_top_topics_start_index, *taxonomy_size,
                  blink::features::kBrowsingTopicsTaxonomyVersion.Get(),
                  model_version, calculation_time_));
}

void BrowsingTopicsCalculator::OnCalculateCompleted(
    CalculatorResultStatus status,
    EpochTopics epoch_topics) {
  DCHECK(status != CalculatorResultStatus::kSuccess || !epoch_topics.empty());

  RecordCalculatorResultMetrics(status, epoch_topics);

  std::move(calculate_completed_callback_).Run(std::move(epoch_topics));

  // Do not add code after this. BrowsingTopicsCalculator has been destroyed.
}

}  // namespace browsing_topics
