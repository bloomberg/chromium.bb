// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_CREATE_BUNDLE_TARGET_WRITER_H_
#define TOOLS_GN_NINJA_CREATE_BUNDLE_TARGET_WRITER_H_

#include "base/macros.h"
#include "tools/gn/ninja_target_writer.h"

// Writes a .ninja file for a bundle_data target type.
class NinjaCreateBundleTargetWriter : public NinjaTargetWriter {
 public:
  NinjaCreateBundleTargetWriter(const Target* target, std::ostream& out);
  ~NinjaCreateBundleTargetWriter() override;

  void Run() override;

 private:
  // Writes the Ninja rule for invoking the code signing script.
  //
  // Returns the name of the custom rule generated for the code signing step if
  // defined, otherwise returns an empty string.
  std::string WriteCodeSigningRuleDefinition();

  // Writes the rule to copy files into the bundle.
  //
  // input_dep is a file expressing the shared dependencies. It will be a
  // stamp file if there is more than one.
  void WriteCopyBundleDataRules(const OutputFile& input_dep,
                                std::vector<OutputFile>* output_files);

  // Writes the rule to compile assets catalogs.
  //
  // input_dep is a file expressing the shared dependencies. It will be a
  // stamp file if there is more than one.
  void WriteCompileAssetsCatalogRule(const OutputFile& input_dep,
                                     std::vector<OutputFile>* output_files);

  // Writes the code signing rule (if a script is defined).
  //
  // input_dep is a file expressing the shared dependencies. It will be a
  // stamp file if there is more than one. As the code signing may include
  // a manifest of the file, this will depends on all files in output_files
  // too.
  void WriteCodeSigningRules(const std::string& code_signing_rule_name,
                             const OutputFile& input_dep,
                             std::vector<OutputFile>* output_files);

  // Writes the stamp file for the code signing input dependencies.
  OutputFile WriteCodeSigningInputDepsStamp(
      const OutputFile& input_dep,
      std::vector<OutputFile>* output_files);

  DISALLOW_COPY_AND_ASSIGN(NinjaCreateBundleTargetWriter);
};

#endif  // TOOLS_GN_NINJA_CREATE_BUNDLE_TARGET_WRITER_H_
