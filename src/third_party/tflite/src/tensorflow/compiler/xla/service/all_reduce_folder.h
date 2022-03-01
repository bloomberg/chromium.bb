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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_ALL_REDUCE_FOLDER_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_ALL_REDUCE_FOLDER_H_

#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_pass_interface.h"
#include "tensorflow/compiler/xla/statusor.h"

namespace xla {

// A pass that folds an all-reduce feeding into another all-reduce by expanding
// the replica groups. As an example:
//
//   ar0 = all-reduce(x) replica_groups={{0,1},{2,3},{4,5},{6,7}}
//   ar1 = all-reduce(all-reduce0) replica_groups={{0,2},{1,3},{4,6},{5,7}}
//
//  Can be combined into a single all-reduce:
//
//   ar1 = all-reduce(x) replica_groups={{0,1,2,3},{4,5,6,7}}
//

class AllReduceFolder : public HloModulePass {
 public:
  absl::string_view name() const override { return "all-reduce-folder"; }

  StatusOr<bool> Run(HloModule* module) override;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_ALL_REDUCE_FOLDER_H_
