// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_action_target_writer.h"

#include "base/strings/string_util.h"
#include "tools/gn/err.h"
#include "tools/gn/file_template.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/target.h"

NinjaActionTargetWriter::NinjaActionTargetWriter(const Target* target,
                                                 const Toolchain* toolchain,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, toolchain, out),
      path_output_no_escaping_(
          target->settings()->build_settings()->build_dir(),
          ESCAPE_NONE) {
}

NinjaActionTargetWriter::~NinjaActionTargetWriter() {
}

void NinjaActionTargetWriter::Run() {
  FileTemplate args_template(
      target_->settings(),
      target_->action_values().args(),
      FileTemplate::OUTPUT_RELATIVE,
      target_->settings()->build_settings()->build_dir());
  std::string custom_rule_name = WriteRuleDefinition(args_template);

  // Collect our deps to pass as "extra hard dependencies" for input deps. This
  // will force all of the action's dependencies to be completed before the
  // action is run. Usually, if an action has a dependency, it will be
  // operating on the result of that previous step, so we need to be sure to
  // serialize these.
  std::vector<const Target*> extra_hard_deps;
  for (size_t i = 0; i < target_->deps().size(); i++)
    extra_hard_deps.push_back(target_->deps()[i].ptr);

  // For ACTIONs this is a bit inefficient since it creates an input dep
  // stamp file even though we're only going to use it once. It would save a
  // build step to skip this and write the order-only deps directly on the
  // build rule. This should probably be handled by WriteInputDepsStampAndGetDep
  // automatically if we supply a count of sources (so it can optimize based on
  // how many times things would be duplicated).
  std::string implicit_deps = WriteInputDepsStampAndGetDep(extra_hard_deps);
  out_ << std::endl;

  // Collects all output files for writing below.
  std::vector<OutputFile> output_files;

  if (target_->output_type() == Target::ACTION_FOREACH) {
    // Write separate build lines for each input source file.
    WriteSourceRules(custom_rule_name, implicit_deps, args_template,
                     &output_files);
  } else {
    DCHECK(target_->output_type() == Target::ACTION);

    // Write a rule that invokes the script once with the outputs as outputs,
    // and the data as inputs.
    out_ << "build";
    const std::vector<std::string>& outputs =
        target_->action_values().outputs();
    for (size_t i = 0; i < outputs.size(); i++) {
      OutputFile output_path(
          RemovePrefix(outputs[i],
                       settings_->build_settings()->build_dir().value()));
      output_files.push_back(output_path);
      out_ << " ";
      path_output_.WriteFile(out_, output_path);
    }

    out_ << ": " << custom_rule_name << implicit_deps << std::endl;
    if (target_->action_values().has_depfile()) {
      out_ << "  depfile = ";
      WriteDepfile(SourceFile());
      out_ << std::endl;
    }
  }
  out_ << std::endl;

  WriteStamp(output_files);
}

std::string NinjaActionTargetWriter::WriteRuleDefinition(
    const FileTemplate& args_template) {
  // Make a unique name for this rule.
  //
  // Use a unique name for the response file when there are multiple build
  // steps so that they don't stomp on each other. When there are no sources,
  // there will be only one invocation so we can use a simple name.
  std::string target_label = target_->label().GetUserVisibleName(true);
  std::string custom_rule_name(target_label);
  base::ReplaceChars(custom_rule_name, ":/()", "_", &custom_rule_name);
  custom_rule_name.append("_rule");

  if (settings_->IsWin()) {
    // Send through gyp-win-tool and use a response file.
    std::string rspfile = custom_rule_name;
    if (has_sources())
      rspfile += ".$unique_name";
    rspfile += ".rsp";

    out_ << "rule " << custom_rule_name << std::endl;
    out_ << "  command = ";
    path_output_.WriteFile(out_, settings_->build_settings()->python_path());
    // TODO(brettw) this hardcodes "environment.x86" which is something that
    // the Chrome Windows toolchain writes. We should have a way to invoke
    // python without requiring this gyp_win_tool thing.
    out_ << " gyp-win-tool action-wrapper environment.x86 " << rspfile
         << std::endl;
    out_ << "  description = ACTION " << target_label << std::endl;
    out_ << "  restat = 1" << std::endl;
    out_ << "  rspfile = " << rspfile << std::endl;

    // The build command goes in the rsp file.
    out_ << "  rspfile_content = ";
    path_output_.WriteFile(out_, settings_->build_settings()->python_path());
    out_ << " ";
    path_output_.WriteFile(out_, target_->action_values().script());
    args_template.WriteWithNinjaExpansions(out_);
    out_ << std::endl;
  } else {
    // Posix can execute Python directly.
    out_ << "rule " << custom_rule_name << std::endl;
    out_ << "  command = ";
    path_output_.WriteFile(out_, settings_->build_settings()->python_path());
    out_ << " ";
    path_output_.WriteFile(out_, target_->action_values().script());
    args_template.WriteWithNinjaExpansions(out_);
    out_ << std::endl;
    out_ << "  description = ACTION " << target_label << std::endl;
    out_ << "  restat = 1" << std::endl;
  }

  return custom_rule_name;
}

void NinjaActionTargetWriter::WriteArgsSubstitutions(
    const SourceFile& source,
    const FileTemplate& args_template) {
  EscapeOptions template_escape_options;
  template_escape_options.mode = ESCAPE_NINJA_COMMAND;

  args_template.WriteNinjaVariablesForSubstitution(
      out_, source, template_escape_options);
}

void NinjaActionTargetWriter::WriteSourceRules(
    const std::string& custom_rule_name,
    const std::string& implicit_deps,
    const FileTemplate& args_template,
    std::vector<OutputFile>* output_files) {
  FileTemplate output_template = FileTemplate::GetForTargetOutputs(target_);

  const Target::FileList& sources = target_->sources();
  for (size_t i = 0; i < sources.size(); i++) {
    out_ << "build";
    WriteOutputFilesForBuildLine(output_template, sources[i], output_files);

    out_ << ": " << custom_rule_name << " ";
    path_output_.WriteFile(out_, sources[i]);
    out_ << implicit_deps << std::endl;

    // Windows needs a unique ID for the response file.
    if (target_->settings()->IsWin())
      out_ << "  unique_name = " << i << std::endl;

    if (args_template.has_substitutions())
      WriteArgsSubstitutions(sources[i], args_template);

    if (target_->action_values().has_depfile()) {
      out_ << "  depfile = ";
      WriteDepfile(sources[i]);
      out_ << std::endl;
    }
  }
}

void NinjaActionTargetWriter::WriteStamp(
    const std::vector<OutputFile>& output_files) {
  out_ << "build ";
  path_output_.WriteFile(out_, helper_.GetTargetOutputFile(target_));
  out_ << ": "
       << helper_.GetRulePrefix(target_->settings())
       << "stamp";

  // The action stamp depends on all output files from running the action.
  for (size_t i = 0; i < output_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, output_files[i]);
  }

  // It also depends on all datadeps. These are needed at runtime and should
  // be compiled when the action is, but don't need to be done before we run
  // the action.
  for (size_t i = 0; i < target_->datadeps().size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_,
        helper_.GetTargetOutputFile(target_->datadeps()[i].ptr));
  }

  out_ << std::endl;
}

void NinjaActionTargetWriter::WriteOutputFilesForBuildLine(
    const FileTemplate& output_template,
    const SourceFile& source,
    std::vector<OutputFile>* output_files) {
  std::vector<std::string> output_template_result;
  output_template.Apply(source, &output_template_result);
  for (size_t out_i = 0; out_i < output_template_result.size(); out_i++) {
    // All output files should be in the build directory, so we can rebase
    // them just by trimming the prefix.
    OutputFile output_path(
        RemovePrefix(output_template_result[out_i],
                     settings_->build_settings()->build_dir().value()));
    output_files->push_back(output_path);
    out_ << " ";
    path_output_.WriteFile(out_, output_path);
  }
}

void NinjaActionTargetWriter::WriteDepfile(const SourceFile& source) {
  std::vector<std::string> result;
  GetDepfileTemplate().Apply(source, &result);
  path_output_.WriteFile(out_, OutputFile(result[0]));
}

FileTemplate NinjaActionTargetWriter::GetDepfileTemplate() const {
  std::vector<std::string> template_args;
  std::string depfile_relative_to_build_dir =
      RemovePrefix(target_->action_values().depfile().value(),
                   settings_->build_settings()->build_dir().value());
  template_args.push_back(depfile_relative_to_build_dir);
  return FileTemplate(settings_, template_args, FileTemplate::OUTPUT_ABSOLUTE,
                      SourceDir());
}
