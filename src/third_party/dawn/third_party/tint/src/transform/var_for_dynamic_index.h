// Copyright 2021 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef SRC_TRANSFORM_VAR_FOR_DYNAMIC_INDEX_H_
#define SRC_TRANSFORM_VAR_FOR_DYNAMIC_INDEX_H_

#include <string>
#include <unordered_map>

#include "src/transform/transform.h"

namespace tint {
namespace transform {

/// A transform that extracts array values that are dynamically indexed to a
/// temporary `var` local before performing the index. This transform is used by
/// the SPIR-V writer for dynamically indexing arrays, as there is no SPIR-V
/// instruction that can dynamically index a non-pointer composite.
class VarForDynamicIndex : public Transform {
 public:
  /// Constructor
  VarForDynamicIndex();

  /// Destructor
  ~VarForDynamicIndex() override;

  /// Runs the transform on `program`, returning the transformation result.
  /// @param program the source program to transform
  /// @param data optional extra transform-specific input data
  /// @returns the transformation result
  Output Run(const Program* program, const DataMap& data = {}) override;
};

}  // namespace transform
}  // namespace tint

#endif  // SRC_TRANSFORM_VAR_FOR_DYNAMIC_INDEX_H_
