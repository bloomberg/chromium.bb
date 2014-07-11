// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_copy_target_writer.h"

#include "base/strings/string_util.h"
#include "tools/gn/file_template.h"
#include "tools/gn/string_utils.h"

NinjaCopyTargetWriter::NinjaCopyTargetWriter(const Target* target,
                                             const Toolchain* toolchain,
                                             std::ostream& out)
    : NinjaTargetWriter(target, toolchain, out) {
}

NinjaCopyTargetWriter::~NinjaCopyTargetWriter() {
}

void NinjaCopyTargetWriter::Run() {
  CHECK(target_->action_values().outputs().size() == 1);
  FileTemplate output_template = FileTemplate::GetForTargetOutputs(target_);

  std::vector<OutputFile> output_files;

  std::string rule_prefix = helper_.GetRulePrefix(target_->settings());

  std::string implicit_deps =
      WriteInputDepsStampAndGetDep(std::vector<const Target*>());

  for (size_t i = 0; i < target_->sources().size(); i++) {
    const SourceFile& input_file = target_->sources()[i];

    // Make the output file from the template.
    std::vector<std::string> template_result;
    output_template.Apply(input_file, &template_result);
    CHECK(template_result.size() == 1);

    // All output files should be in the build directory, so we can rebase
    // them just by trimming the prefix.
    OutputFile output_file(
        RemovePrefix(template_result[0],
                     settings_->build_settings()->build_dir().value()));
    output_files.push_back(output_file);

    out_ << "build ";
    path_output_.WriteFile(out_, output_file);
    out_ << ": " << rule_prefix << "copy ";
    path_output_.WriteFile(out_, input_file);
    out_ << implicit_deps << std::endl;
  }

  // Write out the rule for the target to copy all of them.
  out_ << std::endl << "build ";
  path_output_.WriteFile(out_, helper_.GetTargetOutputFile(target_));
  out_ << ": " << rule_prefix << "stamp";
  for (size_t i = 0; i < output_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, output_files[i]);
  }
  out_ << std::endl;
}
