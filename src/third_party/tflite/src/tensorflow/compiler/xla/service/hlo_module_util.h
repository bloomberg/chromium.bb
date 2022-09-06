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

#ifndef TENSORFLOW_COMPILER_XLA_SERVICE_HLO_MODULE_UTIL_H_
#define TENSORFLOW_COMPILER_XLA_SERVICE_HLO_MODULE_UTIL_H_

#include <functional>
#include <memory>
#include <optional>

#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "tensorflow/compiler/xla/service/compiler.h"
#include "tensorflow/compiler/xla/service/hlo_module_config.h"
#include "tensorflow/compiler/xla/shape.h"
#include "tensorflow/compiler/xla/status.h"
#include "tensorflow/compiler/xla/statusor.h"

namespace xla {

// Creates an HloModuleConfig for a given program shape and arguments.
// If execution_options does not set num_replicas, default_num_replicas is used.
// num_threads is optional; if not given, intra_op_parallelism_threads not set.
// aot_options is optional; if not given a default is used.
StatusOr<std::unique_ptr<HloModuleConfig>> CreateModuleConfig(
    const ProgramShape& program_shape,
    absl::Span<const Shape* const> argument_shapes,
    const ExecutionOptions* execution_options, int default_num_replicas,
    absl::optional<int> num_threads = absl::nullopt,
    const AotCompilationOptions* aot_options = nullptr);

typedef std::function<Shape(const Shape&)> DeviceShapeRepresentationFn;

// Update entry computation's computation layout by translating each shape
// with shape_representation_fn(shape). It can be used for example to add
// tiling info for each shape.
void UpdateEntryComputationLayout(
    HloModule* module, DeviceShapeRepresentationFn shape_representation_fn,
    bool empty_tiles_only = true);
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_XLA_SERVICE_HLO_MODULE_UTIL_H_
