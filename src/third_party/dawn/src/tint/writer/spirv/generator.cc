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

#include "src/tint/writer/spirv/generator.h"

#include <utility>

#include "src/tint/writer/spirv/generator_impl.h"

namespace tint::writer::spirv {

Result::Result() = default;
Result::~Result() = default;
Result::Result(const Result&) = default;

Result Generate(const Program* program, const Options& options) {
    Result result;
    if (!program->IsValid()) {
        result.error = "input program is not valid";
        return result;
    }

    // Sanitize the program.
    auto sanitized_result = Sanitize(program, options);
    if (!sanitized_result.program.IsValid()) {
        result.success = false;
        result.error = sanitized_result.program.Diagnostics().str();
        return result;
    }

    // Generate the SPIR-V code.
    bool zero_initialize_workgroup_memory =
        !options.disable_workgroup_init && options.use_zero_initialize_workgroup_memory_extension;

    auto impl = std::make_unique<GeneratorImpl>(&sanitized_result.program,
                                                zero_initialize_workgroup_memory);
    result.success = impl->Generate();
    result.error = impl->error();
    result.spirv = std::move(impl->result());

    return result;
}

}  // namespace tint::writer::spirv
