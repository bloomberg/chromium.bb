// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/functions.h"

#include "tools/gn/config_values_generator.h"
#include "tools/gn/err.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scope.h"
#include "tools/gn/target_generator.h"
#include "tools/gn/value.h"
#include "tools/gn/variables.h"

#define DEPENDENT_CONFIG_VARS \
    "  Dependent configs: all_dependent_configs, direct_dependent_configs\n"
#define DEPS_VARS \
    "  Deps: data, datadeps, deps, forward_dependent_configs_from, hard_dep\n"
#define GENERAL_TARGET_VARS \
    "  General: configs, external, source_prereqs, sources\n"

namespace functions {

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

  TargetGenerator::GenerateTarget(&block_scope, function, args,
                                  target_type, err);
  if (err->has_error())
    return Value();

  block_scope.CheckForUnusedVars(err);
  return Value();
}

}  // namespace

// component -------------------------------------------------------------------

const char kComponent[] = "component";
const char kComponent_Help[] =
    "component: Declare a component target.\n"
    "\n"
    "  A component is a shared library, static library, or source set\n"
    "  depending on the component mode. This allows a project to separate\n"
    "  out a build into shared libraries for faster development (link time is\n"
    "  reduced) but to switch to a static build for releases (for better\n"
    "  performance).\n"
    "\n"
    "  To use this function you must set the value of the \"component_mode\"\n"
    "  variable to one of the following strings:\n"
    "    - \"shared_library\"\n"
    "    - \"static_library\"\n"
    "    - \"source_set\"\n"
    "  It is an error to call \"component\" without defining the mode\n"
    "  (typically this is done in the master build configuration file).\n";

Value RunComponent(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   BlockNode* block,
                   Err* err) {
  // A component is either a shared or static library, depending on the value
  // of |component_mode|.
  const Value* component_mode_value =
      scope->GetValue(variables::kComponentMode);

  static const char helptext[] =
      "You're declaring a component here but have not defined "
      "\"component_mode\" to\neither \"shared_library\" or \"static_library\".";
  if (!component_mode_value) {
    *err = Err(function->function(), "No component mode set.", helptext);
    return Value();
  }
  if (component_mode_value->type() != Value::STRING ||
      (component_mode_value->string_value() != functions::kSharedLibrary &&
       component_mode_value->string_value() != functions::kStaticLibrary &&
       component_mode_value->string_value() != functions::kSourceSet)) {
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

  TargetGenerator::GenerateTarget(&block_scope, function, args,
                                  component_mode, err);
  return Value();
}

// copy ------------------------------------------------------------------------

const char kCopy[] = "copy";
const char kCopy_Help[] =
    "copy: Declare a target that copies files.\n"
    "\n"
    "File name handling\n"
    "\n"
    "  All output files must be inside the output directory of the build.\n"
    "  You would generally use |$target_out_dir| or |$target_gen_dir| to\n"
    "  reference the output or generated intermediate file directories,\n"
    "  respectively.\n"
    "\n"
    "  Both \"sources\" and \"outputs\" must be specified. Sources can\n"
    "  as many files as you want, but there can only be one item in the\n"
    "  outputs list (plural is used for the name for consistency with\n"
    "  other target types).\n"
    "\n"
    "  If there is more than one source file, your output name should specify\n"
    "  a mapping from each source files to output file names using source\n"
    "  expansion (see \"gn help source_expansion\"). The placeholders will\n"
    "  will look like \"{{source_name_part}}\", for example.\n"
    "\n"
    "Examples\n"
    "\n"
    "  # Write a rule that copies a checked-in DLL to the output directory.\n"
    "  copy(\"mydll\") {\n"
    "    sources = [ \"mydll.dll\" ]\n"
    "    outputs = [ \"$target_out_dir/mydll.dll\" ]\n"
    "  }\n"
    "\n"
    "  # Write a rule to copy several files to the target generated files\n"
    "  # directory.\n"
    "  copy(\"myfiles\") {\n"
    "    sources = [ \"data1.dat\", \"data2.dat\", \"data3.dat\" ]\n"
    "\n"
    "    # Use source expansion to generate output files with the\n"
    "    # corresponding file names in the gen dir. This will just copy each\n"
    "    # file.\n"
    "    outputs = [ \"$target_gen_dir/{{source_file_part}}\" ]\n"
    "  }\n";

Value RunCopy(const FunctionCallNode* function,
              const std::vector<Value>& args,
              Scope* scope,
              Err* err) {
  if (!EnsureNotProcessingImport(function, scope, err) ||
      !EnsureNotProcessingBuildConfig(function, scope, err))
    return Value();
  TargetGenerator::GenerateTarget(scope, function, args, functions::kCopy, err);
  return Value();
}

// custom ----------------------------------------------------------------------

const char kCustom[] = "custom";
const char kCustom_Help[] =
    "custom: Declare a script-generated target.\n"
    "\n"
    "  This target type allows you to run a script over a set of source\n"
    "  files and generate a set of output files.\n"
    "\n"
    "  The script will be executed with the given arguments with the current\n"
    "  directory being that of the root build directory. If you pass files\n"
    "  to your script, see \"gn help to_build_path\" for how to convert\n"
    "  file names to be relative to the build directory (file names in the\n"
    "  sources, outputs, and source_prereqs will be all treated as relative\n"
    "  to the current build file and converted as needed automatically).\n"
    "\n"
    "  There are two modes. The first mode is the \"per-file\" mode where you\n"
    "  specify a list of sources and the script is run once for each one as a\n"
    "  build rule. In this case, each file specified in the |outputs|\n"
    "  variable must be unique when applied to each source file (normally you\n"
    "  would reference |{{source_name_part}}| from within each one) or the\n"
    "  build system will get confused about how to build those files. You\n"
    "  should use the |source_prereqs| variable to list all additional\n"
    "  dependencies of your script: these will be added as dependencies for\n"
    "  each build step.\n"
    "\n"
    "  The second mode is when you just want to run a script once rather than\n"
    "  as a general rule over a set of files. In this case you don't list any\n"
    "  sources. Dependencies of your script are specified only in the\n"
    "  |source_prereqs| variable and your |outputs| variable should just list\n"
    "  all outputs.\n"
    "\n"
    "File name handling\n"
    "\n"
    "  All output files must be inside the output directory of the build.\n"
    "  You would generally use |$target_out_dir| or |$target_gen_dir| to\n"
    "  reference the output or generated intermediate file directories,\n"
    "  respectively.\n"
    "\n"
    "  You can specify a mapping from source files to output files using\n"
    "  source expansion (see \"gn help source_expansion\"). The placeholders\n"
    "  will look like \"{{source}}\", for example, and can appear in\n"
    "  either the outputs or the args lists.\n"
    "\n"
    "Variables\n"
    "\n"
    "  args, deps, outputs, script*, source_prereqs, sources\n"
    "  * = required\n"
    "\n"
    "Examples\n"
    "\n"
    "  # Runs the script over each IDL file. The IDL script will generate\n"
    "  # both a .cc and a .h file for each input.\n"
    "  custom(\"general_rule\") {\n"
    "    script = \"idl_processor.py\"\n"
    "    sources = [ \"foo.idl\", \"bar.idl\" ]\n"
    "    source_prereqs = [ \"my_configuration.txt\" ]\n"
    "    outputs = [ \"$target_gen_dir/{{source_name_part}}.h\",\n"
    "                \"$target_gen_dir/{{source_name_part}}.cc\" ]\n"
    "\n"
    "    # Note that since \"args\" is opaque to GN, if you specify paths\n"
    "    # here, you will need to convert it to be relative to the build\n"
    "    # directory using \"to_build_path()\".\n"
    "    args = [ \"{{source}}\",\n"
    "             \"-o\",\n"
    "             to_build_path(relative_target_gen_dir) + \"/\" +\n"
    "                 {{source_name_part}}.h\" ]\n"
    "  }\n"
    "\n"
    "  custom(\"just_run_this_guy_once\") {\n"
    "    script = \"doprocessing.py\"\n"
    "    source_prereqs = [ \"my_configuration.txt\" ]\n"
    "    outputs = [ \"$target_gen_dir/insightful_output.txt\" ]\n"
    "    args = [ \"--output_dir\", to_build_path(target_gen_dir) ]\n"
    "  }\n";

Value RunCustom(Scope* scope,
                const FunctionCallNode* function,
                const std::vector<Value>& args,
                BlockNode* block,
                Err* err) {
  return ExecuteGenericTarget(functions::kCustom, scope, function, args,
                              block, err);
}

// executable ------------------------------------------------------------------

const char kExecutable[] = "executable";
const char kExecutable_Help[] =
    "executable: Declare an executable target.\n"
    "\n"
    "Variables\n"
    "\n"
    CONFIG_VALUES_VARS_HELP
    DEPS_VARS
    DEPENDENT_CONFIG_VARS
    GENERAL_TARGET_VARS;

Value RunExecutable(Scope* scope,
                    const FunctionCallNode* function,
                    const std::vector<Value>& args,
                    BlockNode* block,
                    Err* err) {
  return ExecuteGenericTarget(functions::kExecutable, scope, function, args,
                              block, err);
}

// group -----------------------------------------------------------------------

const char kGroup[] = "group";
const char kGroup_Help[] =
    "group: Declare a named group of targets.\n"
    "\n"
    "  This target type allows you to create meta-targets that just collect a\n"
    "  set of dependencies into one named target. Groups can additionally\n"
    "  specify configs that apply to their dependents.\n"
    "\n"
    "  Depending on a group is exactly like depending directly on that\n"
    "  group's deps. Direct dependent configs will get automatically\n"
    "  forwarded through the group so you shouldn't need to use\n"
    "  \"forward_dependent_configs_from.\n"
    "\n"
    "Variables\n"
    "\n"
    DEPS_VARS
    DEPENDENT_CONFIG_VARS
    "  Other variables: external\n"
    "\n"
    "Example\n"
    "\n"
    "  group(\"all\") {\n"
    "    deps = [\n"
    "      \"//project:runner\",\n"
    "      \"//project:unit_tests\",\n"
    "    ]\n"
    "  }\n";

Value RunGroup(Scope* scope,
               const FunctionCallNode* function,
               const std::vector<Value>& args,
               BlockNode* block,
               Err* err) {
  return ExecuteGenericTarget(functions::kGroup, scope, function, args,
                              block, err);
}

// shared_library --------------------------------------------------------------

const char kSharedLibrary[] = "shared_library";
const char kSharedLibrary_Help[] =
    "shared_library: Declare a shared library target.\n"
    "\n"
    "  A shared library will be specified on the linker line for targets\n"
    "  listing the shared library in its \"deps\". If you don't want this\n"
    "  (say you dynamically load the library at runtime), then you should\n"
    "  depend on the shared library via \"datadeps\" instead.\n"
    "\n"
    "Variables\n"
    "\n"
    CONFIG_VALUES_VARS_HELP
    DEPS_VARS
    DEPENDENT_CONFIG_VARS
    GENERAL_TARGET_VARS;

Value RunSharedLibrary(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       BlockNode* block,
                       Err* err) {
  return ExecuteGenericTarget(functions::kSharedLibrary, scope, function, args,
                              block, err);
}

// source_set ------------------------------------------------------------------

extern const char kSourceSet[] = "source_set";
extern const char kSourceSet_Help[] =
    "source_set: Declare a source set target.\n"
    "\n"
    "  A source set is a collection of sources that get compiled, but are not\n"
    "  linked to produce any kind of library. Instead, the resulting object\n"
    "  files are implicitly added to the linker line of all targets that\n"
    "  depend on the source set.\n"
    "\n"
    "  In most cases, a source set will behave like a static library, except\n"
    "  no actual library file will be produced. This will make the build go\n"
    "  a little faster by skipping creation of a large static library, while\n"
    "  maintaining the organizational benefits of focused build targets.\n"
    "\n"
    "  The main difference between a source set and a static library is\n"
    "  around handling of exported symbols. Most linkers assume declaring\n"
    "  a function exported means exported from the static library. The linker\n"
    "  can then do dead code elimination to delete code not reachable from\n"
    "  exported functions.\n"
    "\n"
    "  A source set will not do this code elimination since there is no link\n"
    "  step. This allows you to link many sources sets into a shared library\n"
    "  and have the \"exported symbol\" notation indicate \"export from the\n"
    "  final shared library and not from the intermediate targets.\" There is\n"
    "  no way to express this concept when linking multiple static libraries\n"
    "  into a shared library.\n"
    "\n"
    "Variables\n"
    "\n"
    CONFIG_VALUES_VARS_HELP
    DEPS_VARS
    DEPENDENT_CONFIG_VARS
    GENERAL_TARGET_VARS;

Value RunSourceSet(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   BlockNode* block,
                   Err* err) {
  return ExecuteGenericTarget(functions::kSourceSet, scope, function, args,
                              block, err);
}

// static_library --------------------------------------------------------------

const char kStaticLibrary[] = "static_library";
const char kStaticLibrary_Help[] =
    "static_library: Declare a static library target.\n"
    "\n"
    "  Make a \".a\" / \".lib\" file.\n"
    "\n"
    "  If you only need the static library for intermediate results in the\n"
    "  build, you should consider a source_set instead since it will skip\n"
    "  the (potentially slow) step of creating the intermediate library file.\n"
    "\n"
    "Variables\n"
    "\n"
    CONFIG_VALUES_VARS_HELP
    DEPS_VARS
    DEPENDENT_CONFIG_VARS
    GENERAL_TARGET_VARS;

Value RunStaticLibrary(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       BlockNode* block,
                       Err* err) {
  return ExecuteGenericTarget(functions::kStaticLibrary, scope, function, args,
                              block, err);
}

// test ------------------------------------------------------------------------

const char kTest[] = "test";
const char kTest_Help[] =
    "test: Declares a test target.\n"
    "\n"
    "  This is like an executable target, but is named differently to make\n"
    "  the purpose of the target more obvious. It's possible in the future\n"
    "  we can do some enhancements like \"list all of the tests in a given\n"
    "  directory\".\n"
    "\n"
    "  See \"gn help executable\" for usage.\n";

Value RunTest(Scope* scope,
              const FunctionCallNode* function,
              const std::vector<Value>& args,
              BlockNode* block,
              Err* err) {
  return ExecuteGenericTarget(functions::kExecutable, scope, function, args,
                              block, err);
}

}  // namespace functions
