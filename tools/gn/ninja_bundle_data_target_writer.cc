// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_bundle_data_target_writer.h"

#include "tools/gn/output_file.h"

NinjaBundleDataTargetWriter::NinjaBundleDataTargetWriter(const Target* target,
                                                         std::ostream& out)
    : NinjaTargetWriter(target, out) {}

NinjaBundleDataTargetWriter::~NinjaBundleDataTargetWriter() {}

void NinjaBundleDataTargetWriter::Run() {
  std::vector<OutputFile> files;
  files.push_back(WriteInputDepsStampAndGetDep(std::vector<const Target*>()));
  WriteStampForTarget(files, std::vector<OutputFile>());
}
