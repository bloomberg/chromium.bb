// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_
#define COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_

#include <cinttypes>
#include <cstddef>

#include "base/memory/raw_ptr.h"
#include "components/segmentation_platform/internal/database/ukm_types.h"
#include "components/segmentation_platform/internal/proto/model_metadata.pb.h"

namespace segmentation_platform {

// Utility to write metadata proto for default models.
class MetadataWriter {
 public:
  explicit MetadataWriter(proto::SegmentationModelMetadata* metadata);
  ~MetadataWriter();

  MetadataWriter(MetadataWriter&) = delete;
  MetadataWriter& operator=(MetadataWriter&) = delete;

  // Defines a feature based on UMA metric.
  struct UMAFeature {
    const proto::SignalType signal_type{proto::SignalType::UNKNOWN_SIGNAL_TYPE};
    const char* name{nullptr};
    const uint64_t bucket_count{0};
    const uint64_t tensor_length{0};
    const proto::Aggregation aggregation{proto::Aggregation::UNKNOWN};
    const size_t enum_ids_size{0};
    const int32_t* const accepted_enum_ids = nullptr;

    static constexpr UMAFeature FromUserAction(const char* name,
                                               uint64_t bucket_count) {
      return MetadataWriter::UMAFeature{
          .signal_type = proto::SignalType::USER_ACTION,
          .name = name,
          .bucket_count = bucket_count,
          .tensor_length = 1,
          .aggregation = proto::Aggregation::COUNT,
          .enum_ids_size = 0};
    }
    static constexpr UMAFeature FromEnumHistogram(const char* name,
                                                  uint64_t bucket_count,
                                                  const int32_t* const enum_ids,
                                                  size_t enum_ids_size) {
      return MetadataWriter::UMAFeature{
          .signal_type = proto::SignalType::HISTOGRAM_ENUM,
          .name = name,
          .bucket_count = bucket_count,
          .tensor_length = 1,
          .aggregation = proto::Aggregation::COUNT,
          .enum_ids_size = enum_ids_size,
          .accepted_enum_ids = enum_ids};
    }
  };

  // Defines a feature based on a SQL query.
  // TODO(ssid): Support custom inputs.
  struct SqlFeature {
    const char* const sql{nullptr};
    struct EventAndMetrics {
      const UkmEventHash event_hash;
      const raw_ptr<const UkmMetricHash> metrics{nullptr};
      const size_t metrics_size{0};
    };
    const EventAndMetrics* const events{nullptr};
    const size_t events_size{0};
  };

  // Defines a feature based on a custom input.
  struct CustomInput {
    const uint64_t tensor_length{0};
    const proto::CustomInput::FillPolicy fill_policy{
        proto::CustomInput_FillPolicy_UNKNOWN_FILL_POLICY};
    const float default_value{0};
    const char* name{nullptr};
    // TODO(shaktisahu): Support additional_args.
  };

  // Appends the list of UMA features in order.
  void AddUmaFeatures(const UMAFeature features[], size_t features_size);

  // Appends the list of SQL features in order.
  void AddSqlFeatures(const SqlFeature features[], size_t features_size);

  // Appends the list of SQL features in order.
  void AddCustomInput(const CustomInput& feature);

  // Appends a list of discrete mapping in order.
  void AddDiscreteMappingEntries(const std::string& key,
                                 const std::pair<float, int>* mappings,
                                 size_t mappings_size);

  // Writes the model metadata with the given parameters.
  void SetSegmentationMetadataConfig(proto::TimeUnit time_unit,
                                     uint64_t bucket_duration,
                                     int64_t signal_storage_length,
                                     int64_t min_signal_collection_length,
                                     int64_t result_time_to_live);

 private:
  const raw_ptr<proto::SegmentationModelMetadata> metadata_;
};

}  // namespace segmentation_platform

#endif  // COMPONENTS_SEGMENTATION_PLATFORM_INTERNAL_METADATA_METADATA_WRITER_H_
