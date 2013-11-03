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
  CHECK(target_->script_values().outputs().size() == 1);
  FileTemplate output_template(GetOutputTemplate());

  std::vector<OutputFile> output_files;

  std::string rule_prefix = helper_.GetRulePrefix(target_->settings());

  for (size_t i = 0; i < target_->sources().size(); i++) {
    const SourceFile& input_file = target_->sources()[i];

    // Make the output file from the template.
    std::vector<std::string> template_result;
    output_template.ApplyString(input_file.value(), &template_result);
    CHECK(template_result.size() == 1);
    OutputFile output_file(template_result[0]);

    output_files.push_back(output_file);

    out_ << "build ";
    path_output_.WriteFile(out_, output_file);
    out_ << ": " << rule_prefix << "copy ";
    path_output_.WriteFile(out_, input_file);
    out_ << std::endl;
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
