// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_group_target_writer.h"

#include "base/strings/string_util.h"
#include "tools/gn/deps_iterator.h"
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
  // the deps and data_deps in the group.
  std::vector<OutputFile> output_files;
  for (DepsIterator iter(target_, DepsIterator::LINKED_ONLY);
       !iter.done(); iter.Advance())
    output_files.push_back(iter.target()->dependency_output_file());

  std::vector<OutputFile> data_output_files;
  const LabelTargetVector& data_deps = target_->data_deps();
  for (size_t i = 0; i < data_deps.size(); i++)
    data_output_files.push_back(data_deps[i].ptr->dependency_output_file());

  WriteStampForTarget(output_files, data_output_files);
}
