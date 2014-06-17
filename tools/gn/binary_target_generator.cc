// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/binary_target_generator.h"

#include "tools/gn/config_values_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/scope.h"
#include "tools/gn/variables.h"

BinaryTargetGenerator::BinaryTargetGenerator(
    Target* target,
    Scope* scope,
    const FunctionCallNode* function_call,
    Target::OutputType type,
    Err* err)
    : TargetGenerator(target, scope, function_call, err),
      output_type_(type) {
}

BinaryTargetGenerator::~BinaryTargetGenerator() {
}

void BinaryTargetGenerator::DoRun() {
  target_->set_output_type(output_type_);

  FillOutputName();
  if (err_->has_error())
    return;

  FillOutputExtension();
  if (err_->has_error())
    return;

  FillSources();
  if (err_->has_error())
    return;

  FillPublic();
  if (err_->has_error())
    return;

  FillInputs();
  if (err_->has_error())
    return;

  FillConfigs();
  if (err_->has_error())
    return;

  // Config values (compiler flags, etc.) set directly on this target.
  ConfigValuesGenerator gen(&target_->config_values(), scope_,
                            scope_->GetSourceDir(), err_);
  gen.Run();
  if (err_->has_error())
    return;
}

void BinaryTargetGenerator::FillOutputName() {
  const Value* value = scope_->GetValue(variables::kOutputName, true);
  if (!value)
    return;
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return;
  target_->set_output_name(value->string_value());
}

void BinaryTargetGenerator::FillOutputExtension() {
  const Value* value = scope_->GetValue(variables::kOutputExtension, true);
  if (!value)
    return;
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return;
  target_->set_output_extension(value->string_value());
}
