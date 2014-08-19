// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_group_target_writer.h"

#include "base/strings/string_util.h"
#include "tools/gn/output_file.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/target.h"

NinjaGroupTargetWriter::NinjaGroupTargetWriter(const Target* target,
                                               std::ostream& out)
    : NinjaTargetWriter(target, out) {
}

NinjaGroupTargetWriter::~NinjaGroupTargetWriter() {
}

void NinjaGroupTargetWriter::Run() {
  // A group rule just generates a stamp file with dependencies on each of
  // the deps and datadeps in the group.
  std::vector<OutputFile> output_files;
  const LabelTargetVector& deps = target_->deps();
  for (size_t i = 0; i < deps.size(); i++)
    output_files.push_back(deps[i].ptr->dependency_output_file());

  std::vector<OutputFile> data_output_files;
  const LabelTargetVector& datadeps = target_->datadeps();
  for (size_t i = 0; i < datadeps.size(); i++)
    data_output_files.push_back(deps[i].ptr->dependency_output_file());

  WriteStampForTarget(output_files, data_output_files);
}
