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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_ALL_GATHER_BROADCAST_REORDER_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_ALL_GATHER_BROADCAST_REORDER_H_

#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_pass_interface.h"
#include "tensorflow/compiler/xla/statusor.h"

namespace xla {

// A pass that reorders all-gather(broadcast(x)) -> broadcast(all-gather(x)).
// The intent is to reduce the size of all-gather when possible by doing an
// all-gather on the (smaller) pre-broadcasted data and then applying the
// broadcast.
class AllGatherBroadcastReorder : public HloModulePass {
 public:
  absl::string_view name() const override { return "all-gather-bcast-reorder"; }

  StatusOr<bool> Run(HloModule* module) override;
};

}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_ALL_GATHER_BROADCAST_REORDER_H_
