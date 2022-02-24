// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/segmentation_platform/internal/database/metadata_utils.h"

#include <inttypes.h>

#include "base/metrics/metrics_hashes.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/segmentation_platform/internal/database/signal_key.h"
#include "components/segmentation_platform/internal/proto/aggregation.pb.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"
#include "components/segmentation_platform/internal/proto/model_prediction.pb.h"
#include "components/segmentation_platform/internal/proto/types.pb.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace segmentation_platform {
namespace metadata_utils {

namespace {
uint64_t GetExpectedTensorLength(const proto::UMAFeature& feature) {
  switch (feature.aggregation()) {
    case proto::Aggregation::COUNT:
    case proto::Aggregation::COUNT_BOOLEAN:
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN_TRUE_COUNT:
    case proto::Aggregation::SUM:
    case proto::Aggregation::SUM_BOOLEAN:
    case proto::Aggregation::BUCKETED_SUM_BOOLEAN_TRUE_COUNT:
      return feature.bucket_count() == 0 ? 0 : 1;
    case proto::Aggregation::BUCKETED_COUNT:
    case proto::Aggregation::BUCKETED_COUNT_BOOLEAN:
    case proto::Aggregation::BUCKETED_CUMULATIVE_COUNT:
    case proto::Aggregation::BUCKETED_SUM:
    case proto::Aggregation::BUCKETED_SUM_BOOLEAN:
    case proto::Aggregation::BUCKETED_CUMULATIVE_SUM:
      return feature.bucket_count();
    case proto::Aggregation::UNKNOWN:
      NOTREACHED();
      return 0;
  }
}

std::string FeatureToString(const proto::UMAFeature& feature) {
  std::string result;
  if (feature.has_type()) {
    result = "type:" + proto::SignalType_Name(feature.type()) + ", ";
  }
  if (feature.has_name()) {
    result.append("name:" + feature.name() + ", ");
  }
  if (feature.has_name_hash()) {
    result.append(
        base::StringPrintf("name_hash:0x%" PRIx64 ", ", feature.name_hash()));
  }
  if (feature.has_bucket_count()) {
    result.append(base::StringPrintf("bucket_count:%" PRIu64 ", ",
                                     feature.bucket_count()));
  }
  if (feature.has_tensor_length()) {
    result.append(base::StringPrintf("tensor_length:%" PRIu64 ", ",
                                     feature.tensor_length()));
  }
  if (feature.has_aggregation()) {
    result.append("aggregation:" +
                  proto::Aggregation_Name(feature.aggregation()));
  }
  if (base::EndsWith(result, ", "))
    result.resize(result.size() - 2);
  return result;
}

}  // namespace

ValidationResult ValidateSegmentInfo(const proto::SegmentInfo& segment_info) {
  if (segment_info.segment_id() ==
      optimization_guide::proto::OPTIMIZATION_TARGET_UNKNOWN) {
    return ValidationResult::kSegmentIDNotFound;
  }

  if (!segment_info.has_model_metadata())
    return ValidationResult::kMetadataNotFound;

  return ValidateMetadata(segment_info.model_metadata());
}

ValidationResult ValidateMetadata(
    const proto::SegmentationModelMetadata& model_metadata) {
  if (proto::CurrentVersion::METADATA_VERSION <
      model_metadata.version_info().metadata_min_version()) {
    return ValidationResult::kVersionNotSupported;
  }

  if (model_metadata.time_unit() == proto::TimeUnit::UNKNOWN_TIME_UNIT)
    return ValidationResult::kTimeUnitInvald;

  if (model_metadata.features_size() != 0 &&
      model_metadata.input_features_size() != 0) {
    return ValidationResult::kFeatureListInvalid;
  }

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateMetadataUmaFeature(const proto::UMAFeature& feature) {
  if (feature.type() == proto::SignalType::UNKNOWN_SIGNAL_TYPE)
    return ValidationResult::kSignalTypeInvalid;

  if ((feature.type() == proto::SignalType::HISTOGRAM_ENUM ||
       feature.type() == proto::SignalType::HISTOGRAM_VALUE) &&
      feature.name().empty()) {
    return ValidationResult::kFeatureNameNotFound;
  }

  if (feature.name_hash() == 0)
    return ValidationResult::kFeatureNameHashNotFound;

  if (!feature.name().empty() &&
      base::HashMetricName(feature.name()) != feature.name_hash()) {
    return ValidationResult::kFeatureNameHashDoesNotMatchName;
  }

  if (feature.aggregation() == proto::Aggregation::UNKNOWN)
    return ValidationResult::kFeatureAggregationNotFound;

  if (GetExpectedTensorLength(feature) != feature.tensor_length())
    return ValidationResult::kFeatureTensorLengthInvalid;

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateMetadataCustomInput(
    const proto::CustomInput& custom_input) {
  if (custom_input.fill_policy() == proto::CustomInput::UNKNOWN_FILL_POLICY) {
    // If the current fill policy is not supported or not filled, we must use
    // the given default value list, therefore the default value list must
    // provide enough input values as specified by tensor length.
    if (custom_input.tensor_length() > custom_input.default_value_size()) {
      return ValidationResult::kCustomInputInvalid;
    }
  } else if (custom_input.fill_policy() ==
             proto::CustomInput::FILL_PREDICTION_TIME) {
    // Current time can only provide up to one input tensor value, so column
    // weight must not exceed 1.
    if (custom_input.tensor_length() > 1) {
      return ValidationResult::kCustomInputInvalid;
    }
  }
  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateMetadataAndFeatures(
    const proto::SegmentationModelMetadata& model_metadata) {
  auto metadata_result = ValidateMetadata(model_metadata);
  if (metadata_result != ValidationResult::kValidationSuccess)
    return metadata_result;

  for (int i = 0; i < model_metadata.features_size(); ++i) {
    auto feature = model_metadata.features(i);
    auto feature_result = ValidateMetadataUmaFeature(feature);
    if (feature_result != ValidationResult::kValidationSuccess)
      return feature_result;
  }

  for (int i = 0; i < model_metadata.input_features_size(); ++i) {
    auto feature = model_metadata.input_features(i);
    if (feature.has_uma_feature()) {
      auto feature_result = ValidateMetadataUmaFeature(feature.uma_feature());
      if (feature_result != ValidationResult::kValidationSuccess)
        return feature_result;
    } else if (feature.has_custom_input()) {
      auto feature_result = ValidateMetadataCustomInput(feature.custom_input());
      if (feature_result != ValidationResult::kValidationSuccess)
        return feature_result;
    } else {
      return ValidationResult::kFeatureListInvalid;
    }
  }

  return ValidationResult::kValidationSuccess;
}

ValidationResult ValidateSegmentInfoMetadataAndFeatures(
    const proto::SegmentInfo& segment_info) {
  auto segment_info_result = ValidateSegmentInfo(segment_info);
  if (segment_info_result != ValidationResult::kValidationSuccess)
    return segment_info_result;

  return ValidateMetadataAndFeatures(segment_info.model_metadata());
}

void SetFeatureNameHashesFromName(
    proto::SegmentationModelMetadata* model_metadata) {
  for (int i = 0; i < model_metadata->features_size(); ++i) {
    proto::UMAFeature* feature = model_metadata->mutable_features(i);
    feature->set_name_hash(base::HashMetricName(feature->name()));
  }
}

bool HasExpiredOrUnavailableResult(const proto::SegmentInfo& segment_info,
                                   const base::Time& now) {
  if (!segment_info.has_prediction_result())
    return true;

  base::Time last_result_timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(segment_info.prediction_result().timestamp_us()));

  base::TimeDelta result_ttl =
      segment_info.model_metadata().result_time_to_live() *
      GetTimeUnit(segment_info.model_metadata());

  return last_result_timestamp + result_ttl < now;
}

bool HasFreshResults(const proto::SegmentInfo& segment_info,
                     const base::Time& now) {
  if (!segment_info.has_prediction_result())
    return false;

  const proto::SegmentationModelMetadata& metadata =
      segment_info.model_metadata();

  base::Time last_result_timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(segment_info.prediction_result().timestamp_us()));
  base::TimeDelta result_ttl =
      metadata.result_time_to_live() * GetTimeUnit(metadata);

  return now - last_result_timestamp < result_ttl;
}

base::TimeDelta GetTimeUnit(
    const proto::SegmentationModelMetadata& model_metadata) {
  proto::TimeUnit time_unit = model_metadata.time_unit();
  switch (time_unit) {
    case proto::TimeUnit::YEAR:
      return base::Days(365);
    case proto::TimeUnit::MONTH:
      return base::Days(30);
    case proto::TimeUnit::WEEK:
      return base::Days(7);
    case proto::TimeUnit::DAY:
      return base::Days(1);
    case proto::TimeUnit::HOUR:
      return base::Hours(1);
    case proto::TimeUnit::MINUTE:
      return base::Minutes(1);
    case proto::TimeUnit::SECOND:
      return base::Seconds(1);
    case proto::TimeUnit::UNKNOWN_TIME_UNIT:
      [[fallthrough]];
    default:
      NOTREACHED();
      return base::TimeDelta();
  }
}

SignalKey::Kind SignalTypeToSignalKind(proto::SignalType signal_type) {
  switch (signal_type) {
    case proto::SignalType::USER_ACTION:
      return SignalKey::Kind::USER_ACTION;
    case proto::SignalType::HISTOGRAM_ENUM:
      return SignalKey::Kind::HISTOGRAM_ENUM;
    case proto::SignalType::HISTOGRAM_VALUE:
      return SignalKey::Kind::HISTOGRAM_VALUE;
    case proto::SignalType::UNKNOWN_SIGNAL_TYPE:
      return SignalKey::Kind::UNKNOWN;
  }
}

int ConvertToDiscreteScore(const std::string& mapping_key,
                           float input_score,
                           const proto::SegmentationModelMetadata& metadata) {
  auto iter = metadata.discrete_mappings().find(mapping_key);
  if (iter == metadata.discrete_mappings().end()) {
    iter =
        metadata.discrete_mappings().find(metadata.default_discrete_mapping());
    if (iter == metadata.discrete_mappings().end())
      return 0;
  }
  DCHECK(iter != metadata.discrete_mappings().end());

  const auto& mapping = iter->second;

  // Iterate over the entries and find the largest entry whose min result is
  // equal to or less than the input.
  int discrete_result = 0;
  float largest_score_below_input_score = std::numeric_limits<float>::min();
  for (int i = 0; i < mapping.entries_size(); i++) {
    const auto& entry = mapping.entries(i);
    if (entry.min_result() <= input_score &&
        entry.min_result() > largest_score_below_input_score) {
      largest_score_below_input_score = entry.min_result();
      discrete_result = entry.rank();
    }
  }

  return discrete_result;
}

std::string SegmetationModelMetadataToString(
    const proto::SegmentationModelMetadata& model_metadata) {
  std::string result;
  for (const auto& feature : model_metadata.features()) {
    result.append("feature:{" + FeatureToString(feature) + "}, ");
  }
  if (model_metadata.has_time_unit()) {
    result.append(
        "time_unit:" + proto::TimeUnit_Name(model_metadata.time_unit()) + ", ");
  }
  if (model_metadata.has_bucket_duration()) {
    result.append(base::StringPrintf("bucket_duration:%" PRIu64 ", ",
                                     model_metadata.bucket_duration()));
  }
  if (model_metadata.has_signal_storage_length()) {
    result.append(base::StringPrintf("signal_storage_length:%" PRId64 ", ",
                                     model_metadata.signal_storage_length()));
  }
  if (model_metadata.has_min_signal_collection_length()) {
    result.append(
        base::StringPrintf("min_signal_collection_length:%" PRId64 ", ",
                           model_metadata.min_signal_collection_length()));
  }
  if (model_metadata.has_result_time_to_live()) {
    result.append(base::StringPrintf("result_time_to_live:%" PRId64,
                                     model_metadata.result_time_to_live()));
  }

  if (base::EndsWith(result, ", "))
    result.resize(result.size() - 2);
  return result;
}

std::vector<proto::UMAFeature> GetAllUmaFeatures(
    const proto::SegmentationModelMetadata& model_metadata) {
  std::vector<proto::UMAFeature> features;
  for (int i = 0; i < model_metadata.features_size(); ++i) {
    features.push_back(model_metadata.features(i));
  }

  for (int i = 0; i < model_metadata.input_features_size(); ++i) {
    auto feature = model_metadata.input_features(i);
    if (feature.has_uma_feature()) {
      features.push_back(feature.uma_feature());
    }
  }

  return features;
}

}  // namespace metadata_utils
}  // namespace segmentation_platform
