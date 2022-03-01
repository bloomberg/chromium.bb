/* Copyright 2021 The TensorFlow Authors. All Rights Reserved.

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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_ASYNC_COLLECTIVE_CREATOR_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_ASYNC_COLLECTIVE_CREATOR_H_

#include "tensorflow/compiler/xla/service/hlo_pass_interface.h"

namespace xla {

// Transforms each all-reduce instruction to a pair of all-reduce-start and
// all-reduce-done.
class AsyncCollectiveCreator : public HloModulePass {
 public:
  struct CollectiveCreatorConfig {
    bool convert_all_reduce = false;
    bool convert_all_gather = false;
    bool convert_collective_permute = false;
  };
  explicit AsyncCollectiveCreator(CollectiveCreatorConfig creator_config)
      : convert_all_reduce_(creator_config.convert_all_reduce),
        convert_all_gather_(creator_config.convert_all_gather),
        convert_collective_permute_(creator_config.convert_collective_permute) {
  }
  absl::string_view name() const override { return "async-collective-creator"; }

  StatusOr<bool> Run(HloModule* module) override;

 private:
  bool convert_all_reduce_;
  bool convert_all_gather_;
  bool convert_collective_permute_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_ASYNC_ALL_REDUCE_CREATOR_H_
