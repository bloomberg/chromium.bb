// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_script_target_writer.h"

#include "base/strings/string_util.h"
#include "tools/gn/err.h"
#include "tools/gn/file_template.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/target.h"

NinjaScriptTargetWriter::NinjaScriptTargetWriter(const Target* target,
                                                 const Toolchain* toolchain,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, toolchain, out),
      path_output_no_escaping_(
          target->settings()->build_settings()->build_dir(),
          ESCAPE_NONE, false) {
}

NinjaScriptTargetWriter::~NinjaScriptTargetWriter() {
}

void NinjaScriptTargetWriter::Run() {
  FileTemplate args_template(target_->script_values().args());
  std::string custom_rule_name = WriteRuleDefinition(args_template);
  std::string implicit_deps = GetSourcesImplicitDeps();

  // Collects all output files for writing below.
  std::vector<OutputFile> output_files;

  if (has_sources()) {
    // Write separate build lines for each input source file.
    WriteSourceRules(custom_rule_name, implicit_deps, args_template,
                     &output_files);
  } else {
    // No sources, write a rule that invokes the script once with the
    // outputs as outputs, and the data as inputs.
    out_ << "build";
    if (target_->script_values().has_depfile()) {
      out_ << " ";
      WriteDepfile(SourceFile());
    }
    const Target::FileList& outputs = target_->script_values().outputs();
    for (size_t i = 0; i < outputs.size(); i++) {
      OutputFile output_path(
          RemovePrefix(outputs[i].value(),
                       settings_->build_settings()->build_dir().value()));
      output_files.push_back(output_path);
      out_ << " ";
      path_output_.WriteFile(out_, output_path);
    }
    out_ << ": " << custom_rule_name << implicit_deps << std::endl;
    if (target_->script_values().has_depfile()) {
      out_ << "  depfile = ";
      WriteDepfile(SourceFile());
      out_ << std::endl;
    }
  }
  out_ << std::endl;

  WriteStamp(output_files);
}

std::string NinjaScriptTargetWriter::WriteRuleDefinition(
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
    out_ << "  description = CUSTOM " << target_label << std::endl;
    out_ << "  restat = 1" << std::endl;
    out_ << "  rspfile = " << rspfile << std::endl;

    // The build command goes in the rsp file.
    out_ << "  rspfile_content = ";
    path_output_.WriteFile(out_, settings_->build_settings()->python_path());
    out_ << " ";
    path_output_.WriteFile(out_, target_->script_values().script());
    args_template.WriteWithNinjaExpansions(out_);
    out_ << std::endl;
  } else {
    // Posix can execute Python directly.
    out_ << "rule " << custom_rule_name << std::endl;
    out_ << "  command = ";
    path_output_.WriteFile(out_, settings_->build_settings()->python_path());
    out_ << " ";
    path_output_.WriteFile(out_, target_->script_values().script());
    args_template.WriteWithNinjaExpansions(out_);
    out_ << std::endl;
    out_ << "  description = CUSTOM " << target_label << std::endl;
    out_ << "  restat = 1" << std::endl;
  }

  out_ << std::endl;
  return custom_rule_name;
}

void NinjaScriptTargetWriter::WriteArgsSubstitutions(
    const SourceFile& source,
    const FileTemplate& args_template) {
  std::ostringstream source_file_stream;
  path_output_no_escaping_.WriteFile(source_file_stream, source);

  EscapeOptions template_escape_options;
  template_escape_options.mode = ESCAPE_NINJA_SHELL;
  template_escape_options.inhibit_quoting = true;

  args_template.WriteNinjaVariablesForSubstitution(
      out_, source_file_stream.str(), template_escape_options);
}

void NinjaScriptTargetWriter::WriteSourceRules(
    const std::string& custom_rule_name,
    const std::string& implicit_deps,
    const FileTemplate& args_template,
    std::vector<OutputFile>* output_files) {
  FileTemplate output_template(GetOutputTemplate());

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

    if (target_->script_values().has_depfile()) {
      out_ << "  depfile = ";
      WriteDepfile(sources[i]);
      out_ << std::endl;
    }
  }
}

void NinjaScriptTargetWriter::WriteStamp(
    const std::vector<OutputFile>& output_files) {
  out_ << "build ";
  path_output_.WriteFile(out_, helper_.GetTargetOutputFile(target_));
  out_ << ": "
       << helper_.GetRulePrefix(target_->settings())
       << "stamp";
  for (size_t i = 0; i < output_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, output_files[i]);
  }
  out_ << std::endl;
}

void NinjaScriptTargetWriter::WriteOutputFilesForBuildLine(
    const FileTemplate& output_template,
    const SourceFile& source,
    std::vector<OutputFile>* output_files) {
  // If there is a depfile specified we need to list it as the first output as
  // that is what ninja will expect the depfile to refer to itself as.
  if (target_->script_values().has_depfile()) {
    out_ << " ";
    WriteDepfile(source);
  }
  std::vector<std::string> output_template_result;
  output_template.ApplyString(source.value(), &output_template_result);
  for (size_t out_i = 0; out_i < output_template_result.size(); out_i++) {
    OutputFile output_path(output_template_result[out_i]);
    output_files->push_back(output_path);
    out_ << " ";
    path_output_.WriteFile(out_, output_path);
  }
}

void NinjaScriptTargetWriter::WriteDepfile(const SourceFile& source) {
  std::vector<std::string> result;
  GetDepfileTemplate().ApplyString(source.value(), &result);
  path_output_.WriteFile(out_, OutputFile(result[0]));
}

FileTemplate NinjaScriptTargetWriter::GetDepfileTemplate() const {
  std::vector<std::string> template_args;
  std::string depfile_relative_to_build_dir =
      RemovePrefix(target_->script_values().depfile().value(),
                   settings_->build_settings()->build_dir().value());
  template_args.push_back(depfile_relative_to_build_dir);
  return FileTemplate(template_args);
}
