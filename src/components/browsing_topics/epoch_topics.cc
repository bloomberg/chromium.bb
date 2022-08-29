// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_topics/epoch_topics.h"

#include "base/hash/legacy_hash.h"
#include "base/json/values_util.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "components/browsing_topics/util.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"

namespace browsing_topics {

namespace {

const char kTopTopicsAndObservingDomainsNameKey[] =
    "top_topics_and_observing_domains";
const char kPaddedTopTopicsStartIndexNameKey[] =
    "padded_top_topics_start_index";
const char kTaxonomySizeNameKey[] = "taxonomy_size";
const char kTaxonomyVersionNameKey[] = "taxonomy_version";
const char kModelVersionNameKey[] = "model_version";
const char kCalculationTimeNameKey[] = "calculation_time";

bool ShouldUseRandomTopic(uint64_t random_or_top_topic_decision_hash) {
  return base::checked_cast<int>(random_or_top_topic_decision_hash % 100) <
         blink::features::kBrowsingTopicsUseRandomTopicProbabilityPercent.Get();
}

}  // namespace

EpochTopics::EpochTopics() = default;

EpochTopics::EpochTopics(
    std::vector<TopicAndDomains> top_topics_and_observing_domains,
    size_t padded_top_topics_start_index,
    size_t taxonomy_size,
    int taxonomy_version,
    int64_t model_version,
    base::Time calculation_time)
    : top_topics_and_observing_domains_(
          std::move(top_topics_and_observing_domains)),
      padded_top_topics_start_index_(padded_top_topics_start_index),
      taxonomy_size_(taxonomy_size),
      taxonomy_version_(taxonomy_version),
      model_version_(model_version),
      calculation_time_(calculation_time) {
  DCHECK_EQ(base::checked_cast<int>(top_topics_and_observing_domains_.size()),
            blink::features::kBrowsingTopicsNumberOfTopTopicsPerEpoch.Get());
  DCHECK_LE(padded_top_topics_start_index,
            top_topics_and_observing_domains_.size());
  DCHECK_GT(taxonomy_version_, 0);
  DCHECK_GT(model_version_, 0);
}

EpochTopics::EpochTopics(EpochTopics&&) = default;

EpochTopics& EpochTopics::operator=(EpochTopics&&) = default;

EpochTopics::~EpochTopics() = default;

// static
EpochTopics EpochTopics::FromDictValue(const base::Value::Dict& dict_value) {
  std::vector<TopicAndDomains> top_topics_and_observing_domains;
  const base::Value::List* top_topics_and_observing_domains_value =
      dict_value.FindList(kTopTopicsAndObservingDomainsNameKey);
  if (!top_topics_and_observing_domains_value)
    return EpochTopics();

  for (const base::Value& topic_and_observing_domains_value :
       *top_topics_and_observing_domains_value) {
    const base::Value::Dict* topic_and_observing_domains_dict_value =
        topic_and_observing_domains_value.GetIfDict();
    if (!topic_and_observing_domains_dict_value)
      return EpochTopics();

    top_topics_and_observing_domains.push_back(TopicAndDomains::FromDictValue(
        *topic_and_observing_domains_dict_value));
  }

  if (top_topics_and_observing_domains.empty())
    return EpochTopics();

  absl::optional<int> padded_top_topics_start_index_value =
      dict_value.FindInt(kPaddedTopTopicsStartIndexNameKey);
  if (!padded_top_topics_start_index_value)
    return EpochTopics();

  size_t padded_top_topics_start_index =
      static_cast<size_t>(*padded_top_topics_start_index_value);

  absl::optional<int> taxonomy_size_value =
      dict_value.FindInt(kTaxonomySizeNameKey);
  if (!taxonomy_size_value)
    return EpochTopics();

  size_t taxonomy_size = static_cast<size_t>(*taxonomy_size_value);

  absl::optional<int> taxonomy_version_value =
      dict_value.FindInt(kTaxonomyVersionNameKey);
  if (!taxonomy_version_value)
    return EpochTopics();

  int taxonomy_version = *taxonomy_version_value;

  const base::Value* model_version_value =
      dict_value.Find(kModelVersionNameKey);
  if (!model_version_value)
    return EpochTopics();

  absl::optional<int64_t> model_version_int64_value =
      base::ValueToInt64(model_version_value);
  if (!model_version_int64_value)
    return EpochTopics();

  int64_t model_version = *model_version_int64_value;

  const base::Value* calculation_time_value =
      dict_value.Find(kCalculationTimeNameKey);
  if (!calculation_time_value)
    return EpochTopics();

  base::Time calculation_time =
      base::ValueToTime(calculation_time_value).value();

  return EpochTopics(std::move(top_topics_and_observing_domains),
                     padded_top_topics_start_index, taxonomy_size,
                     taxonomy_version, model_version, calculation_time);
}

base::Value::Dict EpochTopics::ToDictValue() const {
  base::Value::List top_topics_and_observing_domains_list;
  for (const TopicAndDomains& topic_and_domains :
       top_topics_and_observing_domains_) {
    top_topics_and_observing_domains_list.Append(
        topic_and_domains.ToDictValue());
  }

  base::Value::Dict result_dict;
  result_dict.Set(kTopTopicsAndObservingDomainsNameKey,
                  std::move(top_topics_and_observing_domains_list));
  result_dict.Set(kPaddedTopTopicsStartIndexNameKey,
                  base::checked_cast<int>(padded_top_topics_start_index_));
  result_dict.Set(kTaxonomySizeNameKey,
                  base::checked_cast<int>(taxonomy_size_));
  result_dict.Set(kTaxonomyVersionNameKey, taxonomy_version_);
  result_dict.Set(kModelVersionNameKey, base::Int64ToValue(model_version_));
  result_dict.Set(kCalculationTimeNameKey,
                  base::TimeToValue(calculation_time_));
  return result_dict;
}

absl::optional<Topic> EpochTopics::TopicForSite(
    const std::string& top_domain,
    const HashedDomain& hashed_context_domain,
    ReadOnlyHmacKey hmac_key,
    bool& output_is_true_topic,
    bool& candidate_topic_filtered) const {
  return TopicForSiteHelper(top_domain, /*need_filtering=*/true,
                            /*allow_random_or_padded_topic=*/true,
                            hashed_context_domain, hmac_key,
                            output_is_true_topic, candidate_topic_filtered);
}

absl::optional<Topic> EpochTopics::TopicForSiteForDisplay(
    const std::string& top_domain,
    ReadOnlyHmacKey hmac_key) const {
  bool output_is_true_topic = false;
  bool candidate_topic_filtered = false;
  return TopicForSiteHelper(top_domain, /*need_filtering=*/false,
                            /*allow_random_or_padded_topic=*/false,
                            /*hashed_context_domain=*/{}, hmac_key,
                            output_is_true_topic, candidate_topic_filtered);
}

void EpochTopics::ClearTopics() {
  top_topics_and_observing_domains_.clear();
  padded_top_topics_start_index_ = 0;
}

void EpochTopics::ClearTopic(Topic topic) {
  for (TopicAndDomains& topic_and_domains : top_topics_and_observing_domains_) {
    if (topic_and_domains.topic() != topic)
      continue;

    // Invalidate `topic_and_domains`. We cannot delete the entry from
    // `top_topics_and_observing_domains_` because it would modify the list of
    // topics, and would break the ability to return the same topic for the same
    // site for the epoch .
    topic_and_domains = TopicAndDomains();
  }
}

void EpochTopics::ClearContextDomain(
    const HashedDomain& hashed_context_domain) {
  for (TopicAndDomains& topic_and_domains : top_topics_and_observing_domains_) {
    topic_and_domains.ClearDomain(hashed_context_domain);
  }
}

absl::optional<Topic> EpochTopics::TopicForSiteHelper(
    const std::string& top_domain,
    bool need_filtering,
    bool allow_random_or_padded_topic,
    const HashedDomain& hashed_context_domain,
    ReadOnlyHmacKey hmac_key,
    bool& output_is_true_topic,
    bool& candidate_topic_filtered) const {
  DCHECK(!output_is_true_topic);
  DCHECK(!candidate_topic_filtered);

  // The topics calculation failed, or the topics has been cleared.
  if (empty())
    return absl::nullopt;

  uint64_t random_or_top_topic_decision_hash =
      HashTopDomainForRandomOrTopTopicDecision(hmac_key, calculation_time_,
                                               top_domain);

  if (ShouldUseRandomTopic(random_or_top_topic_decision_hash)) {
    if (!allow_random_or_padded_topic)
      return absl::nullopt;

    uint64_t random_topic_index_decision =
        HashTopDomainForRandomTopicIndexDecision(hmac_key, calculation_time_,
                                                 top_domain);

    size_t random_topic_index = random_topic_index_decision % taxonomy_size_;

    return Topic(base::checked_cast<int>(random_topic_index + 1));
  }

  uint64_t top_topic_index_decision_hash =
      HashTopDomainForTopTopicIndexDecision(hmac_key, calculation_time_,
                                            top_domain);

  size_t top_topic_index =
      top_topic_index_decision_hash % top_topics_and_observing_domains_.size();

  if (!allow_random_or_padded_topic &&
      padded_top_topics_start_index_ <= top_topic_index)
    return absl::nullopt;

  const TopicAndDomains& topic_and_observing_domains =
      top_topics_and_observing_domains_[top_topic_index];

  if (!topic_and_observing_domains.IsValid())
    return absl::nullopt;

  // Only add the topic if the context has observed it before.
  if (need_filtering && !topic_and_observing_domains.hashed_domains().count(
                            hashed_context_domain)) {
    candidate_topic_filtered = true;
    return absl::nullopt;
  }

  if (top_topic_index < padded_top_topics_start_index_)
    output_is_true_topic = true;

  return topic_and_observing_domains.topic();
}

}  // namespace browsing_topics
