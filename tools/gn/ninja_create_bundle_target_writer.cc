// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_create_bundle_target_writer.h"

#include "base/macros.h"
#include "base/strings/string_util.h"
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

bool EnsureAllToolsAvailable(const Target* target) {
  const Toolchain::ToolType kRequiredTools[] = {
      Toolchain::TYPE_COPY_BUNDLE_DATA, Toolchain::TYPE_COMPILE_XCASSETS,
      Toolchain::TYPE_STAMP,
  };

  for (size_t i = 0; i < arraysize(kRequiredTools); ++i) {
    if (!target->toolchain()->GetTool(kRequiredTools[i])) {
      FailWithMissingToolError(kRequiredTools[i], target);
      return false;
    }
  }

  return true;
}

}  // namespace

NinjaCreateBundleTargetWriter::NinjaCreateBundleTargetWriter(
    const Target* target,
    std::ostream& out)
    : NinjaTargetWriter(target, out) {}

NinjaCreateBundleTargetWriter::~NinjaCreateBundleTargetWriter() {}

void NinjaCreateBundleTargetWriter::Run() {
  if (!EnsureAllToolsAvailable(target_))
    return;

  std::string code_signing_rule_name = WriteCodeSigningRuleDefinition();

  OutputFile input_dep =
      WriteInputDepsStampAndGetDep(std::vector<const Target*>());

  std::vector<OutputFile> output_files;
  WriteCopyBundleDataRules(input_dep, &output_files);
  WriteCompileAssetsCatalogRule(input_dep, &output_files);
  WriteCodeSigningRules(code_signing_rule_name, input_dep, &output_files);

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

std::string NinjaCreateBundleTargetWriter::WriteCodeSigningRuleDefinition() {
  if (target_->bundle_data().code_signing_script().is_null())
    return std::string();

  std::string target_label = target_->label().GetUserVisibleName(true);
  std::string custom_rule_name(target_label);
  base::ReplaceChars(custom_rule_name, ":/()", "_", &custom_rule_name);
  custom_rule_name.append("_code_signing_rule");

  out_ << "rule " << custom_rule_name << std::endl;
  out_ << "  command = ";
  path_output_.WriteFile(out_, settings_->build_settings()->python_path());
  out_ << " ";
  path_output_.WriteFile(out_, target_->bundle_data().code_signing_script());

  const SubstitutionList& args = target_->bundle_data().code_signing_args();
  EscapeOptions args_escape_options;
  args_escape_options.mode = ESCAPE_NINJA_COMMAND;

  for (const auto& arg : args.list()) {
    out_ << " ";
    SubstitutionWriter::WriteWithNinjaVariables(arg, args_escape_options, out_);
  }
  out_ << std::endl;
  out_ << "  description = CODE SIGNING " << target_label << std::endl;
  out_ << "  restat = 1" << std::endl;
  out_ << std::endl;

  return custom_rule_name;
}

void NinjaCreateBundleTargetWriter::WriteCopyBundleDataRules(
    const OutputFile& input_dep,
    std::vector<OutputFile>* output_files) {
  for (const BundleFileRule& file_rule : target_->bundle_data().file_rules()) {
    for (const SourceFile& source_file : file_rule.sources()) {
      OutputFile output_file = file_rule.ApplyPatternToSourceAsOutputFile(
          settings_, target_->bundle_data(), source_file);
      output_files->push_back(output_file);

      out_ << "build ";
      path_output_.WriteFile(out_, output_file);
      out_ << ": "
           << GetNinjaRulePrefixForToolchain(settings_)
           << Toolchain::ToolTypeToName(Toolchain::TYPE_COPY_BUNDLE_DATA)
           << " ";
      path_output_.WriteFile(out_, source_file);
      if (!input_dep.value().empty()) {
        out_ << " | ";
        path_output_.WriteFile(out_, input_dep);
      }
      out_ << std::endl;
    }
  }
}

void NinjaCreateBundleTargetWriter::WriteCompileAssetsCatalogRule(
    const OutputFile& input_dep,
    std::vector<OutputFile>* output_files) {
  if (target_->bundle_data().asset_catalog_sources().empty())
    return;

  OutputFile output_file(settings_->build_settings(),
                         target_->bundle_data().GetCompiledAssetCatalogPath());
  output_files->push_back(output_file);

  out_ << "build ";
  path_output_.WriteFile(out_, output_file);
  out_ << ": " << GetNinjaRulePrefixForToolchain(settings_)
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
    path_output_.WriteFile(
        out_, OutputFile(settings_->build_settings(), source));
  }

  if (!input_dep.value().empty()) {
    out_ << " ";
    path_output_.WriteFile(out_, input_dep);
  }
  out_ << std::endl;
}

void NinjaCreateBundleTargetWriter::WriteCodeSigningRules(
    const std::string& code_signing_rule_name,
    const OutputFile& input_dep,
    std::vector<OutputFile>* output_files) {
  if (code_signing_rule_name.empty())
    return;

  OutputFile code_signing_input_stamp_file =
      WriteCodeSigningInputDepsStamp(input_dep, output_files);

  out_ << "build";
  std::vector<OutputFile> code_signing_output_files;
  SubstitutionWriter::GetListAsOutputFiles(
      settings_, target_->bundle_data().code_signing_outputs(),
      &code_signing_output_files);
  path_output_.WriteFiles(out_, code_signing_output_files);

  // Since the code signature step depends on all the files from the bundle,
  // the create_bundle stamp can just depends on the output of the signature.
  output_files->swap(code_signing_output_files);

  out_ << ": " << code_signing_rule_name;
  if (!code_signing_input_stamp_file.value().empty()) {
    out_ << " | ";
    path_output_.WriteFile(out_, code_signing_input_stamp_file);
  }
  out_ << std::endl;
}

OutputFile NinjaCreateBundleTargetWriter::WriteCodeSigningInputDepsStamp(
    const OutputFile& input_dep,
    std::vector<OutputFile>* output_files) {
  std::vector<SourceFile> code_signing_input_files;
  code_signing_input_files.push_back(
      target_->bundle_data().code_signing_script());
  code_signing_input_files.insert(
      code_signing_input_files.end(),
      target_->bundle_data().code_signing_sources().begin(),
      target_->bundle_data().code_signing_sources().end());
  for (const OutputFile& output_file : *output_files) {
    code_signing_input_files.push_back(
        output_file.AsSourceFile(settings_->build_settings()));
  }
  if (!input_dep.value().empty()) {
    code_signing_input_files.push_back(
        input_dep.AsSourceFile(settings_->build_settings()));
  }

  DCHECK(!code_signing_input_files.empty());
  if (code_signing_input_files.size() == 1)
    return OutputFile(settings_->build_settings(), code_signing_input_files[0]);

  OutputFile code_signing_input_stamp_file =
      OutputFile(RebasePath(GetTargetOutputDir(target_).value(),
                            settings_->build_settings()->build_dir(),
                            settings_->build_settings()->root_path_utf8()));
  code_signing_input_stamp_file.value().append(target_->label().name());
  code_signing_input_stamp_file.value().append(".codesigning.inputdeps.stamp");

  out_ << "build ";
  path_output_.WriteFile(out_, code_signing_input_stamp_file);
  out_ << ": " << GetNinjaRulePrefixForToolchain(settings_)
       << Toolchain::ToolTypeToName(Toolchain::TYPE_STAMP);

  for (const SourceFile& source : code_signing_input_files) {
    out_ << " ";
    path_output_.WriteFile(out_, source);
  }
  out_ << std::endl;
  return code_signing_input_stamp_file;
}
