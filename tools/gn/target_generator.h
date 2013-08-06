// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_TARGET_GENERATOR_H_
#define TOOLS_GN_TARGET_GENERATOR_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/target.h"

class BuildSettings;
class Err;
class Location;
class Scope;
class Token;
class Value;

// Creates Target objects from a Scope (the result of a script execution).
class TargetGenerator {
 public:
  TargetGenerator(Target* target,
                  Scope* scope,
                  const Token& function_token,
                  const std::vector<Value>& args,
                  const std::string& output_type,
                  Err* err);
  ~TargetGenerator();

  void Run();

  // The function token is the token of the function name of the generator for
  // this target. err() will be set on failure.
  static void GenerateTarget(Scope* scope,
                             const Token& function_token,
                             const std::vector<Value>& args,
                             const std::string& output_type,
                             Err* err);

 private:
  // Sets err_ on failure.
  Target::OutputType GetOutputType() const;

  // Reads configs/deps from the given var name, and uses the given setting on
  // the target to save them.
  void FillGenericConfigs(const char* var_name,
                          void (Target::*setter)(std::vector<const Config*>*));
  void FillGenericDeps(const char* var_name,
                       void (Target::*setter)(std::vector<const Target*>*));

  void FillConfigs();
  void FillAllDependentConfigs();
  void FillDirectDependentConfigs();
  void FillSources();
  void FillData();
  void FillDependencies();
  void FillDataDependencies();
  void FillDestDir();
  void FillScript();
  void FillScriptArgs();
  void FillOutputs();

  const BuildSettings* GetBuildSettings() const;

  Target* target_;
  Scope* scope_;
  const Token& function_token_;
  std::vector<Value> args_;
  std::string output_type_;
  Err* err_;

  // Sources are relative to this. This comes from the input file which doesn't
  // get freed so we don't acautlly have to make a copy.
  const SourceDir& input_directory_;

  FRIEND_TEST_ALL_PREFIXES(TargetGenerator, ResolveInputPath);

  DISALLOW_COPY_AND_ASSIGN(TargetGenerator);
};

#endif  // TOOLS_GN_TARGET_GENERATOR_H_
