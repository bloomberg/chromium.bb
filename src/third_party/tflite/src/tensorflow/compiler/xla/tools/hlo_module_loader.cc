/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

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

// Emits an HLO module in a text form suitable for diffing.

#include "tensorflow/compiler/xla/tools/hlo_module_loader.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "tensorflow/compiler/xla/debug_options_flags.h"
#include "tensorflow/compiler/xla/service/hlo_computation.h"
#include "tensorflow/compiler/xla/service/hlo_instruction.h"
#include "tensorflow/compiler/xla/service/hlo_parser.h"
#include "tensorflow/core/lib/io/path.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/protobuf.h"
#include "tensorflow/core/platform/regexp.h"

namespace xla {
namespace {

Status OverrideConfig(const hlo_module_loader_details::Config& ovr_config,
                      HloModuleConfig* config) {
  config->set_replica_count(ovr_config.num_replicas);
  return Status::OK();
}

}  // namespace

string StripLogHeaders(const string& hlo_string) {
  // I0521 12:04:45.883483    1509 service.cc:186] ...
  static RE2* matcher = new RE2(
      "[IWEF]\\d{4} "
      "\\d{2}:\\d{2}:\\d{2}\\.\\d+\\s+\\d+\\s+[^:]+:\\d+\\]\\s?(.*)");
  absl::string_view matches[4];
  std::vector<string> lines = absl::StrSplit(hlo_string, '\n');
  for (auto& line : lines) {
    if (matcher->Match(line, 0, line.size(), RE2::ANCHOR_START, matches, 4)) {
      line = string(matches[1]);
    }
  }
  return absl::StrJoin(lines, "\n", [](string* out, const string& line) {
    absl::StrAppend(out, line);
  });
}

StatusOr<std::unique_ptr<HloModule>> LoadModuleFromData(
    const string& data, const string& format,
    hlo_module_loader_details::Config ovr_config,
    const std::function<void(HloModuleConfig*)>& config_modifier_hook) {
  DebugOptions debug_options = GetDebugOptionsFromFlags();
  std::unique_ptr<HloModule> module;
  if (format == "hlo" || format == "txt") {
    string hlo_string = StripLogHeaders(data);
    HloModuleConfig config;
    config.set_debug_options(debug_options);
    TF_RETURN_IF_ERROR(OverrideConfig(ovr_config, &config));
    if (config_modifier_hook) {
      config_modifier_hook(&config);
    }
    TF_ASSIGN_OR_RETURN(module,
                        ParseAndReturnUnverifiedModule(hlo_string, config));
  } else {
    HloSnapshot proto;
    if (format == "pb") {
      if (!proto.ParseFromString(data) &&
          !proto.mutable_hlo()->ParseFromString(data) &&
          !proto.mutable_hlo()->mutable_hlo_module()->ParseFromString(data)) {
        return InvalidArgument("Failed to parse input as HLO protobuf binary");
      }
    } else if (format == "pbtxt") {
      if (!tensorflow::protobuf::TextFormat::ParseFromString(data, &proto) &&
          !tensorflow::protobuf::TextFormat::ParseFromString(
              data, proto.mutable_hlo()) &&
          !tensorflow::protobuf::TextFormat::ParseFromString(
              data, proto.mutable_hlo()->mutable_hlo_module())) {
        return InvalidArgument("Failed to parse input as HLO protobuf text");
      }
    } else {
      return InvalidArgument(
          "Invalid format from file extension: '%s'. Expected: hlo, txt, pb, "
          "or pbtxt",
          format);
    }
    TF_ASSIGN_OR_RETURN(HloModuleConfig config,
                        HloModule::CreateModuleConfigFromProto(
                            proto.hlo().hlo_module(), debug_options));
    TF_RETURN_IF_ERROR(OverrideConfig(ovr_config, &config));
    if (config_modifier_hook) {
      config_modifier_hook(&config);
    }
    TF_ASSIGN_OR_RETURN(
        module, HloModule::CreateFromProto(proto.hlo().hlo_module(), config));
  }
  return std::move(module);
}

StatusOr<std::unique_ptr<HloModule>> LoadModuleFromFile(
    const string& path, hlo_module_loader_details::Config ovr_config,
    string format,
    const std::function<void(HloModuleConfig*)>& config_modifier_hook) {
  string data;
  if (format.empty()) {
    format = string(tensorflow::io::Extension(path));
  }
  TF_RETURN_IF_ERROR(
      tensorflow::ReadFileToString(tensorflow::Env::Default(), path, &data));
  return LoadModuleFromData(data, format, ovr_config, config_modifier_hook);
}

}  // namespace xla
