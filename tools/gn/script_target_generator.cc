// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/script_target_generator.h"

#include "tools/gn/build_settings.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/value.h"
#include "tools/gn/value_extractors.h"
#include "tools/gn/variables.h"

ScriptTargetGenerator::ScriptTargetGenerator(
    Target* target,
    Scope* scope,
    const FunctionCallNode* function_call,
    Err* err)
    : TargetGenerator(target, scope, function_call, err) {
}

ScriptTargetGenerator::~ScriptTargetGenerator() {
}

void ScriptTargetGenerator::DoRun() {
  target_->set_output_type(Target::CUSTOM);

  FillExternal();
  if (err_->has_error())
    return;

  FillSources();
  if (err_->has_error())
    return;

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

  // Script outputs don't depend on the current toolchain so we can skip adding
  // that dependency.
}

void ScriptTargetGenerator::FillScript() {
  // If this gets called, the target type requires a script, so error out
  // if it doesn't have one.
  const Value* value = scope_->GetValue(variables::kScript, true);
  if (!value) {
    *err_ = Err(function_call_, "This target type requires a \"script\".");
    return;
  }
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return;

  target_->script_values().set_script(
      scope_->GetSourceDir().ResolveRelativeFile(value->string_value()));
}

void ScriptTargetGenerator::FillScriptArgs() {
  const Value* value = scope_->GetValue(variables::kArgs, true);
  if (!value)
    return;

  std::vector<std::string> args;
  if (!ExtractListOfStringValues(*value, &args, err_))
    return;
  target_->script_values().swap_in_args(&args);
}

void ScriptTargetGenerator::FillDepfile() {
  const Value* value = scope_->GetValue(variables::kDepfile, true);
  if (!value)
    return;
  target_->script_values().set_depfile(
      scope_->settings()->build_settings()->build_dir().ResolveRelativeFile(
          value->string_value()));
}
