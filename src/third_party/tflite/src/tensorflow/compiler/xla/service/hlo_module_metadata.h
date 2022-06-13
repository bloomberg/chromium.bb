/* Copyright 2020 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#ifndef THIRD_PARTY_TENSORFLOW_COMPILER_XLA_SERVICE_HLO_MODULE_METADATA_H_
#define THIRD_PARTY_TENSORFLOW_COMPILER_XLA_SERVICE_HLO_MODULE_METADATA_H_

#include <functional>

#include "absl/types/optional.h"
#include "tensorflow/compiler/xla/service/hlo.pb.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/core/platform/env.h"

namespace xla {

// Wrapper class for HloModuleMetadataProto to avoid allowing callers to mutate
// arbitrary fields. Specifically, callers cannot set timestamps or ids or
// set the fields of any pass not currently running.
class HloModuleMetadata {
 public:
  explicit HloModuleMetadata(tensorflow::Env* env) : env_(env) {}

  const HloModuleMetadataProto& proto() const { return module_metadata_; }

  // Creates a new HloPassMetadata. All calls to RecordPassStart should be
  // matched by a later call to RecordPassEnd.
  void RecordPassStart();

  // Marks the currently running pass as finished. Returns NotFound if metadata
  // for the currently running pass cannot be found.
  Status RecordPassEnd();

  const absl::optional<HloModuleMetadataProto>& prepartitioning_metadata()
      const {
    return prepartitioning_metadata_;
  }
  void set_prepartitioning_metadata(
      const HloModuleMetadata& prepartitioning_metadata);

  // Setters for HloModuleMetadataProto.
  void set_module_group_name(const std::string& name) {
    module_metadata_.set_module_group_name(name);
  }
  void set_canonical_module_id(int64_t id) {
    module_metadata_.set_canonical_module_id(id);
  }
  void add_partitioned_module_id(int64_t id) {
    module_metadata_.add_partitioned_module_ids(id);
  }

  StatusOr<int64_t> current_pass_id() {
    TF_ASSIGN_OR_RETURN(HloPassMetadata * pass_metadata,
                        GetCurrentHloPassMetadata());
    return pass_metadata->pass_id();
  }

  // Setters for the current HloPassMetadata.
  Status set_current_pass_name(const std::string& pass_name) {
    return MutateCurrentHloPassMetadata(
        [&pass_name](HloPassMetadata* pass_metadata) {
          pass_metadata->set_pass_name(pass_name);
        });
  }
  Status set_current_pass_pipeline_name(const std::string& pipeline_name) {
    return MutateCurrentHloPassMetadata(
        [&pipeline_name](HloPassMetadata* pass_metadata) {
          pass_metadata->set_pipeline_name(pipeline_name);
        });
  }
  Status add_current_pass_dump_filename(const std::string& dump_filename) {
    return MutateCurrentHloPassMetadata(
        [&dump_filename](HloPassMetadata* pass_metadata) {
          pass_metadata->add_dump_filenames(dump_filename);
        });
  }
  Status set_current_pass_module_changed(bool module_changed) {
    return MutateCurrentHloPassMetadata(
        [&module_changed](HloPassMetadata* pass_metadata) {
          pass_metadata->set_module_changed(module_changed);
        });
  }
  Status set_current_pass_module_id(int64_t module_id) {
    return MutateCurrentHloPassMetadata(
        [&module_id](HloPassMetadata* pass_metadata) {
          pass_metadata->set_module_id(module_id);
        });
  }
  Status add_current_pass_module_group_module_id(int64_t module_id) {
    return MutateCurrentHloPassMetadata(
        [&module_id](HloPassMetadata* pass_metadata) {
          pass_metadata->add_module_group_module_ids(module_id);
        });
  }

 private:
  // Gets mutable metadata for the currently running pass. If passes are nested,
  // finds the deepest one still running. Returns NotFound if metadata for the
  // currently running pass cannot be found.
  StatusOr<HloPassMetadata*> GetCurrentHloPassMetadata();

  Status MutateCurrentHloPassMetadata(
      const std::function<void(HloPassMetadata*)>& mutator);

  HloModuleMetadataProto module_metadata_;
  tensorflow::Env* env_;
  int64_t next_pass_id_ = 1;

  // Stack of metadata for passes that are currently running. Size > 1 iff
  // passes are nested.
  std::vector<HloPassMetadata*> running_passes_;

  // Metadata from before the module was partitioned, if applicable.
  absl::optional<HloModuleMetadataProto> prepartitioning_metadata_ =
      absl::nullopt;
};

}  // namespace xla

#endif  // THIRD_PARTY_TENSORFLOW_COMPILER_XLA_SERVICE_HLO_MODULE_METADATA_H_
