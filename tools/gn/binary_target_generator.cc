// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/binary_target_generator.h"

#include "tools/gn/config_values_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/functions.h"
#include "tools/gn/scope.h"
#include "tools/gn/value_extractors.h"
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

  FillCheckIncludes();
  if (err_->has_error())
    return;

  FillInputs();
  if (err_->has_error())
    return;

  FillConfigs();
  if (err_->has_error())
    return;

  FillAllowCircularIncludesFrom();
  if (err_->has_error())
    return;

  // Config values (compiler flags, etc.) set directly on this target.
  ConfigValuesGenerator gen(&target_->config_values(), scope_,
                            scope_->GetSourceDir(), err_);
  gen.Run();
  if (err_->has_error())
    return;
}

void BinaryTargetGenerator::FillCheckIncludes() {
  const Value* value = scope_->GetValue(variables::kCheckIncludes, true);
  if (!value)
    return;
  if (!value->VerifyTypeIs(Value::BOOLEAN, err_))
    return;
  target_->set_check_includes(value->boolean_value());
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

void BinaryTargetGenerator::FillAllowCircularIncludesFrom() {
  const Value* value = scope_->GetValue(
      variables::kAllowCircularIncludesFrom, true);
  if (!value)
    return;

  UniqueVector<Label> circular;
  ExtractListOfUniqueLabels(*value, scope_->GetSourceDir(),
                            ToolchainLabelForScope(scope_), &circular, err_);
  if (err_->has_error())
    return;

  // Validate that all circular includes entries are in the deps.
  const LabelTargetVector& deps = target_->deps();
  for (size_t circular_i = 0; circular_i < circular.size(); circular_i++) {
    bool found_dep = false;
    for (size_t dep_i = 0; dep_i < deps.size(); dep_i++) {
      if (deps[dep_i].label == circular[circular_i]) {
        found_dep = true;
        break;
      }
    }
    if (!found_dep) {
      *err_ = Err(*value, "Label not in deps.",
          "The label \"" + circular[circular_i].GetUserVisibleName(false) +
          "\"\nwas not in the deps of this target. "
          "allow_circular_includes_from only allows\ntargets present in the "
          "deps.");
      return;
    }
  }

  // Add to the set.
  for (size_t i = 0; i < circular.size(); i++)
    target_->allow_circular_includes_from().insert(circular[i]);
}
