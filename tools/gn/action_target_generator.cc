// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/action_target_generator.h"

#include "tools/gn/build_settings.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"
#include "tools/gn/value_extractors.h"
#include "tools/gn/variables.h"

namespace {

// Returns true if the list of files looks like it might have a {{ }} pattern
// in it. Used for error checking.
bool FileListHasPattern(const Target::FileList& files) {
  for (size_t i = 0; i < files.size(); i++) {
    if (files[i].value().find("{{") != std::string::npos &&
        files[i].value().find("}}") != std::string::npos)
      return true;
  }
  return false;
}

}  // namespace

ActionTargetGenerator::ActionTargetGenerator(
    Target* target,
    Scope* scope,
    const FunctionCallNode* function_call,
    Target::OutputType type,
    Err* err)
    : TargetGenerator(target, scope, function_call, err),
      output_type_(type) {
}

ActionTargetGenerator::~ActionTargetGenerator() {
}

void ActionTargetGenerator::DoRun() {
  target_->set_output_type(output_type_);

  FillExternal();
  if (err_->has_error())
    return;

  FillSources();
  if (err_->has_error())
    return;
  if (output_type_ == Target::ACTION_FOREACH && target_->sources().empty()) {
    // Foreach rules must always have some sources to have an effect.
    *err_ = Err(function_call_, "action_foreach target has no sources.",
        "If you don't specify any sources, there is nothing to run your\n"
        "script over.");
    return;
  }

  FillSourcePrereqs();
  if (err_->has_error())
    return;

  FillScript();
  if (err_->has_error())
    return;

  FillScriptArgs();
  if (err_->has_error())
    return;

  FillOutputs();
  if (err_->has_error())
    return;

  FillDepfile();
  if (err_->has_error())
    return;

  CheckOutputs();
  if (err_->has_error())
    return;

  // Action outputs don't depend on the current toolchain so we can skip adding
  // that dependency.
}

void ActionTargetGenerator::FillScript() {
  // If this gets called, the target type requires a script, so error out
  // if it doesn't have one.
  const Value* value = scope_->GetValue(variables::kScript, true);
  if (!value) {
    *err_ = Err(function_call_, "This target type requires a \"script\".");
    return;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return;

  target_->action_values().set_script(
      scope_->GetSourceDir().ResolveRelativeFile(value->string_value()));
}

void ActionTargetGenerator::FillScriptArgs() {
  const Value* value = scope_->GetValue(variables::kArgs, true);
  if (!value)
    return;

  std::vector<std::string> args;
  if (!ExtractListOfStringValues(*value, &args, err_))
    return;
  target_->action_values().swap_in_args(&args);
}

void ActionTargetGenerator::FillDepfile() {
  const Value* value = scope_->GetValue(variables::kDepfile, true);
  if (!value)
    return;
  target_->action_values().set_depfile(
      scope_->settings()->build_settings()->build_dir().ResolveRelativeFile(
          value->string_value()));
}

void ActionTargetGenerator::CheckOutputs() {
  const Target::FileList& outputs = target_->action_values().outputs();
  if (outputs.empty()) {
    *err_ = Err(function_call_, "Action has no outputs.",
        "If you have no outputs, the build system can not tell when your\n"
        "script needs to be run.");
    return;
  }

  if (output_type_ == Target::ACTION) {
    // Make sure the outputs for an action have no patterns in them.
    if (FileListHasPattern(outputs)) {
      *err_ = Err(function_call_, "Action has patterns in the output.",
          "An action target should have the outputs completely specified. If\n"
          "you want to provide a mapping from source to output, use an\n"
          "\"action_foreach\" target.");
      return;
    }
  } else if (output_type_ == Target::ACTION_FOREACH) {
    // A foreach target should always have a pattern in the outputs.
    if (!FileListHasPattern(outputs)) {
      *err_ = Err(function_call_,
          "action_foreach should have a pattern in the output.",
          "An action_foreach target should have a source expansion pattern in\n"
          "it to map source file to unique output file name. Otherwise, the\n"
          "build system can't determine when your script needs to be run.");
      return;
    }
  }
}
