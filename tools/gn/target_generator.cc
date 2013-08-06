// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/target_generator.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/config.h"
#include "tools/gn/config_values_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/functions.h"
#include "tools/gn/input_file.h"
#include "tools/gn/item_node.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/target_manager.h"
#include "tools/gn/token.h"
#include "tools/gn/value.h"
#include "tools/gn/value_extractors.h"
#include "tools/gn/variables.h"

namespace {

bool TypeHasConfigs(Target::OutputType type) {
  return type == Target::EXECUTABLE ||
         type == Target::SHARED_LIBRARY ||
         type == Target::STATIC_LIBRARY ||
         type == Target::LOADABLE_MODULE;
}

bool TypeHasConfigValues(Target::OutputType type) {
  return type == Target::EXECUTABLE ||
         type == Target::SHARED_LIBRARY ||
         type == Target::STATIC_LIBRARY ||
         type == Target::LOADABLE_MODULE;
}

bool TypeHasSources(Target::OutputType type) {
  return type != Target::NONE;
}

bool TypeHasData(Target::OutputType type) {
  return type != Target::NONE;
}

bool TypeHasDestDir(Target::OutputType type) {
  return type == Target::COPY_FILES;
}

bool TypeHasOutputs(Target::OutputType type) {
  return type == Target::CUSTOM;
}

}  // namespace

TargetGenerator::TargetGenerator(Target* target,
                                 Scope* scope,
                                 const Token& function_token,
                                 const std::vector<Value>& args,
                                 const std::string& output_type,
                                 Err* err)
    : target_(target),
      scope_(scope),
      function_token_(function_token),
      args_(args),
      output_type_(output_type),
      err_(err),
      input_directory_(function_token.location().file()->dir()) {
}

TargetGenerator::~TargetGenerator() {
}

void TargetGenerator::Run() {
  // Output type.
  Target::OutputType output_type = GetOutputType();
  target_->set_output_type(output_type);
  if (err_->has_error())
    return;

  if (TypeHasConfigs(output_type)) {
    FillConfigs();
    FillAllDependentConfigs();
    FillDirectDependentConfigs();
  }
  if (TypeHasSources(output_type))
    FillSources();
  if (TypeHasData(output_type))
    FillData();
  if (output_type == Target::CUSTOM) {
    FillScript();
    FillScriptArgs();
  }
  if (TypeHasOutputs(output_type))
    FillOutputs();
  FillDependencies();  // All types have dependencies.
  FillDataDependencies();  // All types have dependencies.

  if (TypeHasConfigValues(output_type)) {
    ConfigValuesGenerator gen(&target_->config_values(), scope_,
                              function_token_, input_directory_, err_);
    gen.Run();
    if (err_->has_error())
      return;
  }

  if (TypeHasDestDir(output_type))
    FillDestDir();

  // Set the toolchain as a dependency of the target.
  // TODO(brettw) currently we lock separately for each config, dep, and
  // toolchain we add which is bad! Do this in one lock.
  {
    ItemTree* tree = &GetBuildSettings()->item_tree();
    base::AutoLock lock(tree->lock());
    ItemNode* tc_node =
        tree->GetExistingNodeLocked(ToolchainLabelForScope(scope_));
    if (!tree->GetExistingNodeLocked(target_->label())->AddDependency(
            GetBuildSettings(), function_token_.range(), tc_node, err_))
      return;
  }

  target_->SetGenerated(&function_token_);
  GetBuildSettings()->target_manager().TargetGenerationComplete(
      target_->label(), err_);
}

// static
void TargetGenerator::GenerateTarget(Scope* scope,
                                     const Token& function_token,
                                     const std::vector<Value>& args,
                                     const std::string& output_type,
                                     Err* err) {
  // Name is the argument to the function.
  if (args.size() != 1u || args[0].type() != Value::STRING) {
    *err = Err(function_token,
        "Target generator requires one string argument.",
        "Otherwise I'm not sure what to call this target.");
    return;
  }

  // The location of the target is the directory name with no slash at the end.
  // FIXME(brettw) validate name.
  const Label& toolchain_label = ToolchainLabelForScope(scope);
  Label label(function_token.location().file()->dir(),
              args[0].string_value(),
              toolchain_label.dir(), toolchain_label.name());

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Generating target", label.GetUserVisibleName(true));

  Target* t = scope->settings()->build_settings()->target_manager().GetTarget(
      label, function_token.range(), NULL, err);
  if (err->has_error())
    return;

  TargetGenerator gen(t, scope, function_token, args, output_type, err);
  gen.Run();
}

Target::OutputType TargetGenerator::GetOutputType() const {
  if (output_type_ == functions::kGroup)
    return Target::NONE;
  if (output_type_ == functions::kExecutable)
    return Target::EXECUTABLE;
  if (output_type_ == functions::kSharedLibrary)
    return Target::SHARED_LIBRARY;
  if (output_type_ == functions::kStaticLibrary)
    return Target::STATIC_LIBRARY;
  // TODO(brettw) what does loadable module mean?
  //if (output_type_ == ???)
  //  return Target::LOADABLE_MODULE;
  if (output_type_ == functions::kCopy)
    return Target::COPY_FILES;
  if (output_type_ == functions::kCustom)
    return Target::CUSTOM;

  *err_ = Err(function_token_, "Not a known output type",
              "I am very confused.");
  return Target::NONE;
}

void TargetGenerator::FillGenericConfigs(
    const char* var_name,
    void (Target::*setter)(std::vector<const Config*>*)) {
  const Value* value = scope_->GetValue(var_name, true);
  if (!value)
    return;

  std::vector<Label> labels;
  if (!ExtractListOfLabels(*value, input_directory_,
                           ToolchainLabelForScope(scope_), &labels, err_))
    return;

  std::vector<const Config*> dest_configs;
  dest_configs.resize(labels.size());
  for (size_t i = 0; i < labels.size(); i++) {
    dest_configs[i] = Config::GetConfig(
        scope_->settings(),
        value->list_value()[i].origin()->GetRange(),
        labels[i], target_, err_);
    if (err_->has_error())
      return;
  }
  (target_->*setter)(&dest_configs);
}

void TargetGenerator::FillGenericDeps(
    const char* var_name,
    void (Target::*setter)(std::vector<const Target*>*)) {
  const Value* value = scope_->GetValue(var_name, true);
  if (!value)
    return;

  std::vector<Label> labels;
  if (!ExtractListOfLabels(*value, input_directory_,
                           ToolchainLabelForScope(scope_), &labels, err_))
    return;

  std::vector<const Target*> dest_deps;
  dest_deps.resize(labels.size());
  for (size_t i = 0; i < labels.size(); i++) {
    dest_deps[i] = GetBuildSettings()->target_manager().GetTarget(
        labels[i], value->list_value()[i].origin()->GetRange(), target_, err_);
    if (err_->has_error())
      return;
  }

  (target_->*setter)(&dest_deps);
}

void TargetGenerator::FillConfigs() {
  FillGenericConfigs(variables::kConfigs, &Target::swap_in_configs);
}

void TargetGenerator::FillAllDependentConfigs() {
  FillGenericConfigs(variables::kAllDependentConfigs,
                     &Target::swap_in_all_dependent_configs);
}

void TargetGenerator::FillDirectDependentConfigs() {
  FillGenericConfigs(variables::kDirectDependentConfigs,
                     &Target::swap_in_direct_dependent_configs);
}

void TargetGenerator::FillSources() {
  const Value* value = scope_->GetValue(variables::kSources, true);
  if (!value)
    return;

  Target::FileList dest_sources;
  if (!ExtractListOfRelativeFiles(*value, input_directory_, &dest_sources,
                                  err_))
    return;
  target_->swap_in_sources(&dest_sources);
}

void TargetGenerator::FillData() {
  // TODO(brettW) hook this up to the constant when we have cleaned up
  // how data files are used.
  const Value* value = scope_->GetValue("data", true);
  if (!value)
    return;

  Target::FileList dest_data;
  if (!ExtractListOfRelativeFiles(*value, input_directory_, &dest_data,
                                  err_))
    return;
  target_->swap_in_data(&dest_data);
}

void TargetGenerator::FillDependencies() {
  FillGenericDeps(variables::kDeps, &Target::swap_in_deps);
}

void TargetGenerator::FillDataDependencies() {
  FillGenericDeps(variables::kDatadeps, &Target::swap_in_datadeps);
}

void TargetGenerator::FillDestDir() {
  // Destdir is required for all targets that use it.
  const Value* value = scope_->GetValue("destdir", true);
  if (!value) {
    *err_ = Err(function_token_, "This target type requires a \"destdir\".");
    return;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return;

  if (!EnsureStringIsInOutputDir(
          GetBuildSettings()->build_dir(),
          value->string_value(), *value, err_))
    return;
  target_->set_destdir(SourceDir(value->string_value()));
}

void TargetGenerator::FillScript() {
  // If this gets called, the target type requires a script, so error out
  // if it doesn't have one.
  const Value* value = scope_->GetValue("script", true);
  if (!value) {
    *err_ = Err(function_token_, "This target type requires a \"script\".");
    return;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return;

  target_->set_script(
      input_directory_.ResolveRelativeFile(value->string_value()));
}

void TargetGenerator::FillScriptArgs() {
  const Value* value = scope_->GetValue("args", true);
  if (!value)
    return;

  std::vector<std::string> args;
  if (!ExtractListOfStringValues(*value, &args, err_))
    return;
  target_->swap_in_script_args(&args);
}

void TargetGenerator::FillOutputs() {
  const Value* value = scope_->GetValue("outputs", true);
  if (!value)
    return;

  Target::FileList outputs;
  if (!ExtractListOfRelativeFiles(*value, input_directory_, &outputs, err_))
    return;

  // Validate that outputs are in the output dir.
  CHECK(outputs.size() == value->list_value().size());
  for (size_t i = 0; i < outputs.size(); i++) {
    if (!EnsureStringIsInOutputDir(
            GetBuildSettings()->build_dir(),
            outputs[i].value(), value->list_value()[i], err_))
      return;
  }
  target_->swap_in_outputs(&outputs);
}

const BuildSettings* TargetGenerator::GetBuildSettings() const {
  return scope_->settings()->build_settings();
}
