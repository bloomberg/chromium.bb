// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/group_target_generator.h"

#include "tools/gn/variables.h"

GroupTargetGenerator::GroupTargetGenerator(
    Target* target,
    Scope* scope,
    const FunctionCallNode* function_call,
    Err* err)
    : TargetGenerator(target, scope, function_call, err) {
}

GroupTargetGenerator::~GroupTargetGenerator() {
}

void GroupTargetGenerator::DoRun() {
  target_->set_output_type(Target::GROUP);
  // Groups only have the default types filled in by the target generator
  // base class.

  // Before there was a deps/public_deps split, a group acted like all deps
  // are public. During a transition period, if public_deps is not defined,
  // treat all deps as public. This should be removed and existing groups
  // updated to use "public_deps" when needed.
  if (scope_->GetValue(variables::kDeps, false) &&
      !scope_->GetValue(variables::kPublicDeps, false)) {
    std::swap(target_->private_deps(), target_->public_deps());
  }
}
