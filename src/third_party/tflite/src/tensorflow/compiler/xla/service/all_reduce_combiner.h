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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_ALL_REDUCE_COMBINER_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_ALL_REDUCE_COMBINER_H_

#include "absl/strings/string_view.h"
#include "tensorflow/compiler/xla/array2d.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_pass_interface.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"

namespace xla {

// Combines small non-dependent AllReduce ops into larger combined
// AllReduce ops. A typical AllReduce implementation has a minimum
// latency-induced time for a AllReduce op so a single combined op can be
// more efficient than many small ones.
class AllReduceCombiner : public HloModulePass {
 public:
  AllReduceCombiner(int64 combine_threshold_in_bytes,
                    int64 combine_threshold_count);

  absl::string_view name() const override { return "all-reduce-combiner"; }

  StatusOr<bool> Run(HloModule* module) override;

 private:
  // Combine all reduce ops up to this threshold.
  int64 combine_threshold_in_bytes_;

  // Combine all reduce ops up to this threshold (number of operands).
  int64 combine_threshold_count_;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_ALL_REDUCE_COMBINER_H_
