// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/functions.h"

#include "tools/gn/err.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/target_generator.h"
#include "tools/gn/value.h"

namespace {

Value ExecuteGenericTarget(const char* target_type,
                           Scope* scope,
                           const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           BlockNode* block,
                           Err* err) {
  if (!EnsureNotProcessingImport(function, scope, err) ||
      !EnsureNotProcessingBuildConfig(function, scope, err))
    return Value();
  Scope block_scope(scope);
  if (!FillTargetBlockScope(scope, function, target_type, block,
                            args, &block_scope, err))
    return Value();

  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  TargetGenerator::GenerateTarget(&block_scope, function->function(), args,
                                  target_type, err);

  block_scope.CheckForUnusedVars(err);
  return Value();
}

}  // namespace

Value ExecuteComponent(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       BlockNode* block,
                       Err* err) {
  // A component is either a shared or static library, depending on the value
  // of |component_mode|.
  const Value* component_mode_value = scope->GetValue("component_mode");

  static const char helptext[] =
      "You're declaring a component here but have not defined "
      "\"component_mode\" to\neither \"shared_library\" or \"static_library\".";
  if (!component_mode_value) {
    *err = Err(function->function(), "No component mode set.", helptext);
    return Value();
  }
  if (component_mode_value->type() != Value::STRING ||
      (component_mode_value->string_value() != functions::kSharedLibrary &&
       component_mode_value->string_value() != functions::kStaticLibrary)) {
    *err = Err(function->function(), "Invalid component mode set.", helptext);
    return Value();
  }
  const std::string& component_mode = component_mode_value->string_value();

  if (!EnsureNotProcessingImport(function, scope, err))
    return Value();
  Scope block_scope(scope);
  if (!FillTargetBlockScope(scope, function, component_mode.c_str(), block,
                            args, &block_scope, err))
    return Value();

  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  TargetGenerator::GenerateTarget(&block_scope, function->function(), args,
                                  component_mode, err);
  return Value();
}

Value ExecuteCopy(Scope* scope,
                  const FunctionCallNode* function,
                  const std::vector<Value>& args,
                  Err* err) {
  if (!EnsureNotProcessingImport(function, scope, err) ||
      !EnsureNotProcessingBuildConfig(function, scope, err))
    return Value();
  TargetGenerator::GenerateTarget(scope, function->function(), args,
                                  functions::kCopy, err);
  return Value();
}

/*
custom: Declare a script-generated target.

  This target type allows you to run a script over a set of sources files and
  generate a set of output files.

  The script will be executed with the given arguments with the current
  directory being that of the current BUILD file.

  There are two modes. The first mode is the "per-file" mode where you
  specify a list of sources and the script is run once for each one as a build
  rule. In this case, each file specified in the |outputs| variable must be
  unique when applied to each source file (normally you would reference
  "{{source_name_part}}" from within each one) or the build system will get
  confused about how to build those files. You should use the |data| variable
  to list all additional dependencies of your script: these will be added
  as dependencies for each build step.

  The second mode is when you just want to run a script once rather than as a
  general rule over a set of files. In this case you don't list any sources.
  Dependencies of your script are specified only in the |data| variable and
  your |outputs| variable should just list all outputs.

Variables:

  args, data, deps, outputs, script*, sources
  * = required

  There are some special substrings that will be searched for when processing
  some variables:

    "{{source}}"
        Expanded in |args|, this is the name of the source file relative to the
        current directory when running the script. This is how you specify
        the current input file to your script.

    "{{source_name_part}}"
        Expanded in |args| and |outputs|, this is just the filename part of the
        current source file with no directory or extension. This is how you
        specify a name transoformation to the output. Normally you would
        write an output as "$target_output_dir/{{source_name_part}}.o".

  All |outputs| files must be inside the output directory of the build. You
  would generally use "$target_output_dir" or "$target_gen_dir" to reference
  the output or generated intermediate file directories, respectively.

Examples:

  custom("general_rule") {
    script = "do_processing.py"
    sources = [ "foo.idl" ]
    data = [ "my_configuration.txt" ]
    outputs = [ "$target_gen_dir/{{source_name_part}}.h" ]
    args = [ "{{source}}",
             "-o", "$relative_target_gen_dir/{{source_name_part}}.h" ]
  }

  custom("just_run_this_guy_once") {
    script = "doprocessing.py"
    data = [ "my_configuration.txt" ]
    outputs = [ "$target_gen_dir/insightful_output.txt" ]
    args = [ "--output_dir", $target_gen_dir ]
  }
*/
Value ExecuteCustom(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    BlockNode* block,
                    Err* err) {
  return ExecuteGenericTarget(functions::kCustom, scope, function, args,
                              block, err);
}

Value ExecuteExecutable(Scope* scope,
                        const FunctionCallNode* function,
                        const std::vector<Value>& args,
                        BlockNode* block,
                        Err* err) {
  return ExecuteGenericTarget(functions::kExecutable, scope, function, args,
                              block, err);
}

Value ExecuteSharedLibrary(Scope* scope,
                           const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           BlockNode* block,
                           Err* err) {
  return ExecuteGenericTarget(functions::kSharedLibrary, scope, function, args,
                              block, err);
}

Value ExecuteStaticLibrary(Scope* scope,
                           const FunctionCallNode* function,
                           const std::vector<Value>& args,
                           BlockNode* block,
                           Err* err) {
  return ExecuteGenericTarget(functions::kStaticLibrary, scope, function, args,
                              block, err);
}

/*
group: Declare a group of targets.

  This target type allows you to create meta-targets that just collect a set
  of dependencies into one named target.

Variables:

  deps

Example:

  group("all") {
    deps = [
      "//project:runner",
      "//project:unit_tests",
    ]
  }
*/
Value ExecuteGroup(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   BlockNode* block,
                   Err* err) {
  return ExecuteGenericTarget(functions::kGroup, scope, function, args,
                              block, err);
}

