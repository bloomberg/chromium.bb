// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/create_bundle_target_generator.h"

#include "tools/gn/filesystem_utils.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/substitution_type.h"
#include "tools/gn/target.h"
#include "tools/gn/value.h"
#include "tools/gn/variables.h"

CreateBundleTargetGenerator::CreateBundleTargetGenerator(
    Target* target,
    Scope* scope,
    const FunctionCallNode* function_call,
    Err* err)
    : TargetGenerator(target, scope, function_call, err) {}

CreateBundleTargetGenerator::~CreateBundleTargetGenerator() {}

void CreateBundleTargetGenerator::DoRun() {
  target_->set_output_type(Target::CREATE_BUNDLE);

  BundleData& bundle_data = target_->bundle_data();
  if (!GetBundleDir(SourceDir(),
                    variables::kBundleRootDir,
                    &bundle_data.root_dir()))
    return;
  if (!GetBundleDir(bundle_data.root_dir(),
                    variables::kBundleResourcesDir,
                    &bundle_data.resources_dir()))
    return;
  if (!GetBundleDir(bundle_data.root_dir(),
                    variables::kBundleExecutableDir,
                    &bundle_data.executable_dir()))
    return;
  if (!GetBundleDir(bundle_data.root_dir(),
                    variables::kBundlePlugInsDir,
                    &bundle_data.plugins_dir()))
    return;
}

bool CreateBundleTargetGenerator::GetBundleDir(
    const SourceDir& bundle_root_dir,
    const base::StringPiece& name,
    SourceDir* bundle_dir) {
  const Value* value = scope_->GetValue(name, true);
  if (!value)
    return true;
  if (!value->VerifyTypeIs(Value::STRING, err_))
    return false;
  std::string str = value->string_value();
  if (!EnsureStringIsInOutputDir(GetBuildSettings()->build_dir(), str,
                                 value->origin(), err_))
    return false;
  if (str != bundle_root_dir.value() &&
      !IsStringInOutputDir(bundle_root_dir, str)) {
    *err_ = Err(value->origin(), "Path is not in bundle root dir.",
        "The given file should be in the bundle root directory or below.\n"
        "Normally you would do \"$bundle_root_dir/foo\". I interpreted this\n"
        "as \"" + str + "\".");
    return false;
  }
  bundle_dir->SwapValue(&str);
  return true;
}
