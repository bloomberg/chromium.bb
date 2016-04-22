// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_create_bundle_target_writer.h"

#include "tools/gn/filesystem_utils.h"
#include "tools/gn/ninja_utils.h"
#include "tools/gn/output_file.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"

namespace {

void FailWithMissingToolError(Toolchain::ToolType tool, const Target* target) {
  const std::string& tool_name = Toolchain::ToolTypeToName(tool);
  g_scheduler->FailWithError(Err(
      nullptr, tool_name + " tool not defined",
      "The toolchain " +
          target->toolchain()->label().GetUserVisibleName(false) + "\n"
          "used by target " + target->label().GetUserVisibleName(false) + "\n"
          "doesn't define a \"" + tool_name + "\" tool."));
}

}  // namespace

NinjaCreateBundleTargetWriter::NinjaCreateBundleTargetWriter(
    const Target* target,
    std::ostream& out)
    : NinjaTargetWriter(target, out) {}

NinjaCreateBundleTargetWriter::~NinjaCreateBundleTargetWriter() {}

void NinjaCreateBundleTargetWriter::Run() {
  if (!target_->toolchain()->GetTool(Toolchain::TYPE_COPY_BUNDLE_DATA)) {
    FailWithMissingToolError(Toolchain::TYPE_COPY_BUNDLE_DATA, target_);
    return;
  }

  if (!target_->toolchain()->GetTool(Toolchain::TYPE_COMPILE_XCASSETS)) {
    FailWithMissingToolError(Toolchain::TYPE_COMPILE_XCASSETS, target_);
    return;
  }

  if (!target_->toolchain()->GetTool(Toolchain::TYPE_STAMP)) {
    FailWithMissingToolError(Toolchain::TYPE_STAMP, target_);
    return;
  }

  std::vector<OutputFile> output_files;
  OutputFile input_dep =
      WriteInputDepsStampAndGetDep(std::vector<const Target*>());

  for (const BundleFileRule& file_rule : target_->bundle_data().file_rules()) {
    for (const SourceFile& source_file : file_rule.sources()) {
      OutputFile output_file = file_rule.ApplyPatternToSourceAsOutputFile(
          settings_, target_->bundle_data(), source_file);
      output_files.push_back(output_file);

      out_ << "build ";
      path_output_.WriteFile(out_, output_file);
      out_ << ": "
           << GetNinjaRulePrefixForToolchain(settings_)
           << Toolchain::ToolTypeToName(Toolchain::TYPE_COPY_BUNDLE_DATA)
           << " ";
      path_output_.WriteFile(out_, source_file);
      if (!input_dep.value().empty()) {
        out_ << " || ";
        path_output_.WriteFile(out_, input_dep);
      }
      out_ << std::endl;
    }
  }

  if (!target_->bundle_data().asset_catalog_sources().empty()) {
    OutputFile output_file(
        settings_->build_settings(),
        target_->bundle_data().GetCompiledAssetCatalogPath());
    output_files.push_back(output_file);

    out_ << "build ";
    path_output_.WriteFile(out_, output_file);
    out_ << ": "
         << GetNinjaRulePrefixForToolchain(settings_)
         << Toolchain::ToolTypeToName(Toolchain::TYPE_COMPILE_XCASSETS);

    std::set<SourceFile> asset_catalog_bundles;
    for (const auto& source : target_->bundle_data().asset_catalog_sources()) {
      SourceFile asset_catalog_bundle;
      CHECK(IsSourceFileFromAssetCatalog(source, &asset_catalog_bundle));
      if (asset_catalog_bundles.find(asset_catalog_bundle) !=
          asset_catalog_bundles.end())
        continue;
      out_ << " ";
      path_output_.WriteFile(out_, asset_catalog_bundle);
      asset_catalog_bundles.insert(asset_catalog_bundle);
    }

    out_ << " |";
    for (const auto& source : target_->bundle_data().asset_catalog_sources()) {
      out_ << " ";
      path_output_.WriteFile(out_, source);
    }
    if (!input_dep.value().empty()) {
      out_ << " || ";
      path_output_.WriteFile(out_, input_dep);
    }
    out_ << std::endl;
  }

  out_ << std::endl;

  std::vector<OutputFile> order_only_deps;
  for (const auto& pair : target_->data_deps())
    order_only_deps.push_back(pair.ptr->dependency_output_file());
  WriteStampForTarget(output_files, order_only_deps);

  // Write a phony target for the outer bundle directory. This allows other
  // targets to treat the entire bundle as a single unit, even though it is
  // a directory, so that it can be depended upon as a discrete build edge.
  out_ << "build ";
  path_output_.WriteFile(
      out_,
      OutputFile(settings_->build_settings(),
                 target_->bundle_data().GetBundleRootDirOutput(settings_)));
  out_ << ": phony " << target_->dependency_output_file().value();
  out_ << std::endl;
}
