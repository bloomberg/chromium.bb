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

#include "src/tint/fuzzers/tint_common_fuzzer.h"

#include <cassert>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if TINT_BUILD_SPV_READER
#include "spirv-tools/libspirv.hpp"
#endif  // TINT_BUILD_SPV_READER

#include "src/tint/ast/module.h"
#include "src/tint/diagnostic/formatter.h"
#include "src/tint/program.h"
#include "src/tint/utils/hash.h"

namespace tint::fuzzers {

namespace {

// A macro is used to avoid FATAL_ERROR creating its own stack frame. This leads
// to better de-duplication of bug reports, because ClusterFuzz only uses the
// top few stack frames for de-duplication, and a FATAL_ERROR stack frame
// provides no useful information.
#define FATAL_ERROR(diags, msg_string)                        \
  do {                                                        \
    std::string msg = msg_string;                             \
    auto printer = tint::diag::Printer::create(stderr, true); \
    if (!msg.empty()) {                                       \
      printer->write(msg + "\n", {diag::Color::kRed, true});  \
    }                                                         \
    tint::diag::Formatter().format(diags, printer.get());     \
    __builtin_trap();                                         \
  } while (false)

[[noreturn]] void TintInternalCompilerErrorReporter(
    const tint::diag::List& diagnostics) {
  FATAL_ERROR(diagnostics, "");
}

// Wrapping in a macro, so it can be a one-liner in the code, but not
// introduce another level in the stack trace. This will help with de-duping
// ClusterFuzz issues.
#define CHECK_INSPECTOR(program, inspector)                    \
  do {                                                         \
    if ((inspector).has_error()) {                             \
      if (!enforce_validity) {                                 \
        return;                                                \
      }                                                        \
      FATAL_ERROR((program)->Diagnostics(),                    \
                  "Inspector failed: " + (inspector).error()); \
    }                                                          \
  } while (false)

// Wrapping in a macro to make code more readable and help with issue de-duping.
#define VALIDITY_ERROR(diags, msg_string) \
  do {                                    \
    if (!enforce_validity) {              \
      return 0;                           \
    }                                     \
    FATAL_ERROR(diags, msg_string);       \
  } while (false)

bool SPIRVToolsValidationCheck(const tint::Program& program,
                               const std::vector<uint32_t>& spirv) {
  spvtools::SpirvTools tools(SPV_ENV_VULKAN_1_1);
  const tint::diag::List& diags = program.Diagnostics();
  tools.SetMessageConsumer([diags](spv_message_level_t, const char*,
                                   const spv_position_t& pos, const char* msg) {
    std::stringstream out;
    out << "Unexpected spirv-val error:\n"
        << (pos.line + 1) << ":" << (pos.column + 1) << ": " << msg
        << std::endl;

    auto printer = tint::diag::Printer::create(stderr, true);
    printer->write(out.str(), {diag::Color::kYellow, false});
    tint::diag::Formatter().format(diags, printer.get());
  });

  return tools.Validate(spirv.data(), spirv.size(),
                        spvtools::ValidatorOptions());
}

}  // namespace

void GenerateSpirvOptions(DataBuilder* b, writer::spirv::Options* options) {
  *options = b->build<writer::spirv::Options>();
}

void GenerateWgslOptions(DataBuilder* b, writer::wgsl::Options* options) {
  *options = b->build<writer::wgsl::Options>();
}

void GenerateHlslOptions(DataBuilder* b, writer::hlsl::Options* options) {
  *options = b->build<writer::hlsl::Options>();
}

void GenerateMslOptions(DataBuilder* b, writer::msl::Options* options) {
  *options = b->build<writer::msl::Options>();
}

CommonFuzzer::CommonFuzzer(InputFormat input, OutputFormat output)
    : input_(input), output_(output) {}

CommonFuzzer::~CommonFuzzer() = default;

int CommonFuzzer::Run(const uint8_t* data, size_t size) {
  tint::SetInternalCompilerErrorReporter(&TintInternalCompilerErrorReporter);

#if TINT_BUILD_WGSL_WRITER
  tint::Program::printer = [](const tint::Program* program) {
    auto result = tint::writer::wgsl::Generate(program, {});
    if (!result.error.empty()) {
      return "error: " + result.error;
    }
    return result.wgsl;
  };
#endif  // TINT_BUILD_WGSL_WRITER

  Program program;

#if TINT_BUILD_SPV_READER
  std::vector<uint32_t> spirv_input(size / sizeof(uint32_t));

#endif  // TINT_BUILD_SPV_READER

#if TINT_BUILD_WGSL_READER || TINT_BUILD_SPV_READER
  auto dump_input_data = [&](auto& content, const char* extension) {
    size_t hash = utils::Hash(content);
    auto filename = "fuzzer_input_" + std::to_string(hash) + extension;  //
    std::ofstream fout(filename, std::ios::binary);
    fout.write(reinterpret_cast<const char*>(data),
               static_cast<std::streamsize>(size));
    std::cout << "Dumped input data to " << filename << std::endl;
  };
#endif

  switch (input_) {
#if TINT_BUILD_WGSL_READER
    case InputFormat::kWGSL: {
      // Clear any existing diagnostics, as these will hold pointers to file_,
      // which we are about to release.
      diagnostics_ = {};
      std::string str(reinterpret_cast<const char*>(data), size);
      file_ = std::make_unique<Source::File>("test.wgsl", str);
      if (dump_input_) {
        dump_input_data(str, ".wgsl");
      }
      program = reader::wgsl::Parse(file_.get());
      break;
    }
#endif  // TINT_BUILD_WGSL_READER
#if TINT_BUILD_SPV_READER
    case InputFormat::kSpv: {
      // `spirv_input` has been initialized with the capacity to store `size /
      // sizeof(uint32_t)` uint32_t values. If `size` is not a multiple of
      // sizeof(uint32_t) then not all of `data` can be copied into
      // `spirv_input`, and any trailing bytes are discarded.
      std::memcpy(spirv_input.data(), data,
                  spirv_input.size() * sizeof(uint32_t));
      if (spirv_input.empty()) {
        return 0;
      }
      if (dump_input_) {
        dump_input_data(spirv_input, ".spv");
      }
      program = reader::spirv::Parse(spirv_input);
      break;
    }
#endif  // TINT_BUILD_SPV_READER
  }

  if (!program.IsValid()) {
    diagnostics_ = program.Diagnostics();
    return 0;
  }

#if TINT_BUILD_SPV_READER
  if (input_ == InputFormat::kSpv &&
      !SPIRVToolsValidationCheck(program, spirv_input)) {
    FATAL_ERROR(
        program.Diagnostics(),
        "Fuzzing detected invalid input spirv not being caught by Tint");
  }
#endif  // TINT_BUILD_SPV_READER

  RunInspector(&program);
  diagnostics_ = program.Diagnostics();

  if (transform_manager_) {
    auto out = transform_manager_->Run(&program, *transform_inputs_);
    if (!out.program.IsValid()) {
      // Transforms can produce error messages for bad input.
      // Catch ICEs and errors from non transform systems.
      for (const auto& diag : out.program.Diagnostics()) {
        if (diag.severity > diag::Severity::Error ||
            diag.system != diag::System::Transform) {
          VALIDITY_ERROR(program.Diagnostics(),
                         "Fuzzing detected valid input program being "
                         "transformed into an invalid output program");
        }
      }
    }

    program = std::move(out.program);
    RunInspector(&program);
  }

  switch (output_) {
    case OutputFormat::kWGSL: {
#if TINT_BUILD_WGSL_WRITER
      auto result = writer::wgsl::Generate(&program, options_wgsl_);
      generated_wgsl_ = std::move(result.wgsl);
      if (!result.success) {
        VALIDITY_ERROR(
            program.Diagnostics(),
            "WGSL writer errored on validated input:\n" + result.error);
      }
#endif  // TINT_BUILD_WGSL_WRITER
      break;
    }
    case OutputFormat::kSpv: {
#if TINT_BUILD_SPV_WRITER
      auto result = writer::spirv::Generate(&program, options_spirv_);
      generated_spirv_ = std::move(result.spirv);
      if (!result.success) {
        VALIDITY_ERROR(
            program.Diagnostics(),
            "SPIR-V writer errored on validated input:\n" + result.error);
      }

      if (!SPIRVToolsValidationCheck(program, generated_spirv_)) {
        VALIDITY_ERROR(program.Diagnostics(),
                       "Fuzzing detected invalid spirv being emitted by Tint");
      }

#endif  // TINT_BUILD_SPV_WRITER
      break;
    }
    case OutputFormat::kHLSL: {
#if TINT_BUILD_HLSL_WRITER
      auto result = writer::hlsl::Generate(&program, options_hlsl_);
      generated_hlsl_ = std::move(result.hlsl);
      if (!result.success) {
        VALIDITY_ERROR(
            program.Diagnostics(),
            "HLSL writer errored on validated input:\n" + result.error);
      }
#endif  // TINT_BUILD_HLSL_WRITER
      break;
    }
    case OutputFormat::kMSL: {
#if TINT_BUILD_MSL_WRITER
      auto result = writer::msl::Generate(&program, options_msl_);
      generated_msl_ = std::move(result.msl);
      if (!result.success) {
        VALIDITY_ERROR(
            program.Diagnostics(),
            "MSL writer errored on validated input:\n" + result.error);
      }
#endif  // TINT_BUILD_MSL_WRITER
      break;
    }
  }

  return 0;
}

void CommonFuzzer::RunInspector(Program* program) {
  inspector::Inspector inspector(program);
  diagnostics_ = program->Diagnostics();

  auto entry_points = inspector.GetEntryPoints();
  CHECK_INSPECTOR(program, inspector);

  auto constant_ids = inspector.GetConstantIDs();
  CHECK_INSPECTOR(program, inspector);

  auto constant_name_to_id = inspector.GetConstantNameToIdMap();
  CHECK_INSPECTOR(program, inspector);

  for (auto& ep : entry_points) {
    inspector.GetStorageSize(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetUniformBufferResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetStorageBufferResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetReadOnlyStorageBufferResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetSamplerResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetComparisonSamplerResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetSampledTextureResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetMultisampledTextureResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetWriteOnlyStorageTextureResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetDepthTextureResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetDepthMultisampledTextureResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetExternalTextureResourceBindings(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetSamplerTextureUses(ep.name);
    CHECK_INSPECTOR(program, inspector);

    inspector.GetWorkgroupStorageSize(ep.name);
    CHECK_INSPECTOR(program, inspector);
  }
}

}  // namespace tint::fuzzers
