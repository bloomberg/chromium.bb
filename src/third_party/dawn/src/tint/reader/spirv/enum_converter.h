// Copyright 2020 The Tint Authors.
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

#ifndef SRC_TINT_READER_SPIRV_ENUM_CONVERTER_H_
#define SRC_TINT_READER_SPIRV_ENUM_CONVERTER_H_

#include "spirv/unified1/spirv.h"
#include "src/tint/ast/builtin.h"
#include "src/tint/ast/pipeline_stage.h"
#include "src/tint/ast/storage_class.h"
#include "src/tint/reader/spirv/fail_stream.h"
#include "src/tint/sem/storage_texture.h"

namespace tint::reader::spirv {

/// A converter from SPIR-V enums to Tint AST enums.
class EnumConverter {
  public:
    /// Creates a new enum converter.
    /// @param fail_stream the error reporting stream.
    explicit EnumConverter(const FailStream& fail_stream);
    /// Destructor
    ~EnumConverter();

    /// Converts a SPIR-V execution model to a Tint pipeline stage.
    /// On failure, logs an error and returns kNone
    /// @param model the SPIR-V entry point execution model
    /// @returns a Tint AST pipeline stage
    ast::PipelineStage ToPipelineStage(SpvExecutionModel model);

    /// Converts a SPIR-V storage class to a Tint storage class.
    /// On failure, logs an error and returns kNone
    /// @param sc the SPIR-V storage class
    /// @returns a Tint AST storage class
    ast::StorageClass ToStorageClass(const SpvStorageClass sc);

    /// Converts a SPIR-V Builtin value a Tint Builtin.
    /// On failure, logs an error and returns kNone
    /// @param b the SPIR-V builtin
    /// @returns a Tint AST builtin
    ast::Builtin ToBuiltin(SpvBuiltIn b);

    /// Converts a possibly arrayed SPIR-V Dim to a Tint texture dimension.
    /// On failure, logs an error and returns kNone
    /// @param dim the SPIR-V Dim value
    /// @param arrayed true if the texture is arrayed
    /// @returns a Tint AST texture dimension
    ast::TextureDimension ToDim(SpvDim dim, bool arrayed);

    /// Converts a SPIR-V Image Format to a TexelFormat
    /// On failure, logs an error and returns kNone
    /// @param fmt the SPIR-V format
    /// @returns a Tint AST format
    ast::TexelFormat ToTexelFormat(SpvImageFormat fmt);

  private:
    /// Registers a failure and returns a stream for log diagnostics.
    /// @returns a failure stream
    FailStream Fail() { return fail_stream_.Fail(); }

    FailStream fail_stream_;
};

}  // namespace tint::reader::spirv

#endif  // SRC_TINT_READER_SPIRV_ENUM_CONVERTER_H_
