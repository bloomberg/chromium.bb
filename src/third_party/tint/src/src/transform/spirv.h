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

#ifndef SRC_TRANSFORM_SPIRV_H_
#define SRC_TRANSFORM_SPIRV_H_

#include "src/transform/transform.h"

namespace tint {

// Forward declarations
class CloneContext;

namespace transform {

/// Spirv is a transform used to sanitize a Program for use with the Spirv
/// writer. Passing a non-sanitized Program to the Spirv writer will result in
/// undefined behavior.
class Spirv : public Transform {
 public:
  /// Constructor
  Spirv();
  ~Spirv() override;

  /// Runs the transform on `program`, returning the transformation result.
  /// @param program the source program to transform
  /// @returns the transformation result
  Output Run(const Program* program) override;

 private:
  /// Change type of sample mask builtin variables to single element arrays.
  void HandleSampleMaskBuiltins(CloneContext& ctx) const;
};

}  // namespace transform
}  // namespace tint

#endif  // SRC_TRANSFORM_SPIRV_H_
