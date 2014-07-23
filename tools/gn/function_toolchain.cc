// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/err.h"
#include "tools/gn/functions.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/scope.h"
#include "tools/gn/settings.h"
#include "tools/gn/toolchain.h"
#include "tools/gn/value_extractors.h"
#include "tools/gn/variables.h"

namespace functions {

namespace {

// This is jsut a unique value to take the address of to use as the key for
// the toolchain property on a scope.
const int kToolchainPropertyKey = 0;

// Reads the given string from the scope (if present) and puts the result into
// dest. If the value is not a string, sets the error and returns false.
bool ReadString(Scope& scope, const char* var, std::string* dest, Err* err) {
  const Value* v = scope.GetValue(var, true);
  if (!v)
    return true;  // Not present is fine.

  if (!v->VerifyTypeIs(Value::STRING, err))
    return false;
  *dest = v->string_value();
  return true;
}

}  // namespace

// toolchain -------------------------------------------------------------------

const char kToolchain[] = "toolchain";
const char kToolchain_HelpShort[] =
    "toolchain: Defines a toolchain.";
const char kToolchain_Help[] =
    "toolchain: Defines a toolchain.\n"
    "\n"
    "  A toolchain is a set of commands and build flags used to compile the\n"
    "  source code. You can have more than one toolchain in use at once in\n"
    "  a build.\n"
    "\n"
    "  A toolchain specifies the commands to run for various input file\n"
    "  types via the \"tool\" call (see \"gn help tool\") and specifies\n"
    "  arguments to be passed to the toolchain build via the\n"
    "  \"toolchain_args\" call (see \"gn help toolchain_args\").\n"
    "\n"
    "  In addition, a toolchain can specify dependencies via the \"deps\"\n"
    "  variable like a target. These dependencies will be resolved before any\n"
    "  target in the toolchain is compiled. To avoid circular dependencies\n"
    "  these must be targets defined in another toolchain.\n"
    "\n"
    "Invoking targets in toolchains:\n"
    "\n"
    "  By default, when a target depends on another, there is an implicit\n"
    "  toolchain label that is inherited, so the dependee has the same one\n"
    "  as the dependent.\n"
    "\n"
    "  You can override this and refer to any other toolchain by explicitly\n"
    "  labeling the toolchain to use. For example:\n"
    "    datadeps = [ \"//plugins:mine(//toolchains:plugin_toolchain)\" ]\n"
    "  The string \"//build/toolchains:plugin_toolchain\" is a label that\n"
    "  identifies the toolchain declaration for compiling the sources.\n"
    "\n"
    "  To load a file in an alternate toolchain, GN does the following:\n"
    "\n"
    "   1. Loads the file with the toolchain definition in it (as determined\n"
    "      by the toolchain label).\n"
    "   2. Re-runs the master build configuration file, applying the\n"
    "      arguments specified by the toolchain_args section of the toolchain\n"
    "      definition (see \"gn help toolchain_args\").\n"
    "   3. Loads the destination build file in the context of the\n"
    "      configuration file in the previous step.\n"
    "\n"
    "Example:\n"
    "  toolchain(\"plugin_toolchain\") {\n"
    "    tool(\"cc\") {\n"
    "      command = \"gcc $in\"\n"
    "    }\n"
    "\n"
    "    toolchain_args() {\n"
    "      is_plugin = true\n"
    "      is_32bit = true\n"
    "      is_64bit = false\n"
    "    }\n"
    "  }\n";

Value RunToolchain(Scope* scope,
                   const FunctionCallNode* function,
                   const std::vector<Value>& args,
                   BlockNode* block,
                   Err* err) {
  if (!EnsureNotProcessingImport(function, scope, err) ||
      !EnsureNotProcessingBuildConfig(function, scope, err))
    return Value();

  // Note that we don't want to use MakeLabelForScope since that will include
  // the toolchain name in the label, and toolchain labels don't themselves
  // have toolchain names.
  const SourceDir& input_dir = scope->GetSourceDir();
  Label label(input_dir, args[0].string_value());
  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Definining toolchain", label.GetUserVisibleName(false));

  // This object will actually be copied into the one owned by the toolchain
  // manager, but that has to be done in the lock.
  scoped_ptr<Toolchain> toolchain(new Toolchain(scope->settings(), label));
  toolchain->set_defined_from(function);
  toolchain->visibility().SetPublic();

  Scope block_scope(scope);
  block_scope.SetProperty(&kToolchainPropertyKey, toolchain.get());
  block->ExecuteBlockInScope(&block_scope, err);
  block_scope.SetProperty(&kToolchainPropertyKey, NULL);
  if (err->has_error())
    return Value();

  // Read deps (if any).
  const Value* deps_value = block_scope.GetValue(variables::kDeps, true);
  if (deps_value) {
    ExtractListOfLabels(
        *deps_value, block_scope.GetSourceDir(),
        ToolchainLabelForScope(&block_scope), &toolchain->deps(), err);
    if (err->has_error())
      return Value();
  }


  if (!block_scope.CheckForUnusedVars(err))
    return Value();

  // Save this toolchain.
  Scope::ItemVector* collector = scope->GetItemCollector();
  if (!collector) {
    *err = Err(function, "Can't define a toolchain in this context.");
    return Value();
  }
  collector->push_back(new scoped_ptr<Item>(toolchain.PassAs<Item>()));
  return Value();
}

// tool ------------------------------------------------------------------------

const char kTool[] = "tool";
const char kTool_HelpShort[] =
    "tool: Specify arguments to a toolchain tool.";
const char kTool_Help[] =
    "tool: Specify arguments to a toolchain tool.\n"
    "\n"
    "  tool(<command type>) { <command flags> }\n"
    "\n"
    "  Used inside a toolchain definition to define a command to run for a\n"
    "  given file type. See also \"gn help toolchain\".\n"
    "\n"
    "Command types\n"
    "\n"
    "  The following values may be passed to the tool() function for the type\n"
    "  of the command:\n"
    "\n"
    "  \"cc\", \"cxx\", \"objc\", \"objcxx\", \"asm\", \"alink\", \"solink\",\n"
    "  \"link\", \"stamp\", \"copy\"\n"
    "\n"
    "Tool-specific notes\n"
    "\n"
    "  copy\n"
    "    The copy command should be a native OS command since it does not\n"
    "    implement toolchain dependencies (which would enable a copy tool to\n"
    "    be compiled by a previous step).\n"
    "\n"
    "    It is legal for the copy to not update the timestamp of the output\n"
    "    file (as long as it's greater than or equal to the input file). This\n"
    "    allows the copy command to be implemented as a hard link which can\n"
    "    be more efficient.\n"
    "\n"
    "Command flags\n"
    "\n"
    "  These variables may be specified in the { } block after the tool call.\n"
    "  They are passed directly to Ninja. See the ninja documentation for how\n"
    "  they work. Don't forget to backslash-escape $ required by Ninja to\n"
    "  prevent GN from doing variable expansion.\n"
    "\n"
    "    command, depfile, depsformat, description, pool, restat, rspfile,\n"
    "    rspfile_content\n"
    "\n"
    "  (Note that GN uses \"depsformat\" for Ninja's \"deps\" variable to\n"
    "  avoid confusion with dependency lists.)\n"
    "\n"
    "  Additionally, lib_prefix and lib_dir_prefix may be used for the link\n"
    "  tools. These strings will be prepended to the libraries and library\n"
    "  search directories, respectively, because linkers differ on how to\n"
    "  specify them.\n"
    "\n"
    "  Note: On Mac libraries with names ending in \".framework\" will be\n"
    "  added to the link like with a \"-framework\" switch and the lib prefix\n"
    "  will be ignored.\n"
    "\n"
    "Ninja variables available to tool invocations\n"
    "\n"
    "  When writing tool commands, you use the various built-in Ninja\n"
    "  variables like \"$in\" and \"$out\" (note that the $ must be escaped\n"
    "  for it to be passed to Ninja, so write \"\\$in\" in the command\n"
    "  string).\n"
    "\n"
    "  GN defines the following variables for binary targets to access the\n"
    "  various computed information needed for compiling:\n"
    "\n"
    "    - Compiler flags: \"cflags\", \"cflags_c\", \"cflags_cc\",\n"
    "          \"cflags_objc\", \"cflags_objcc\"\n"
    "\n"
    "    - Linker flags: \"ldflags\", \"libs\"\n"
    "\n"
    "  GN sets these other variables with target information that can be\n"
    "  used for computing names for supplimetary files:\n"
    "\n"
    "    - \"target_name\": The name of the current target with no\n"
    "      path information. For example \"mylib\".\n"
    "\n"
    "    - \"target_out_dir\": The value of \"target_out_dir\" from the BUILD\n"
    "      file for this target (see \"gn help target_out_dir\"), relative\n"
    "      to the root build directory with no trailing slash.\n"
    "\n"
    "    - \"root_out_dir\": The value of \"root_out_dir\" from the BUILD\n"
    "      file for this target (see \"gn help root_out_dir\"), relative\n"
    "      to the root build directory with no trailing slash.\n"
    "\n"
    "Example\n"
    "\n"
    "  toolchain(\"my_toolchain\") {\n"
    "    # Put these at the top to apply to all tools below.\n"
    "    lib_prefix = \"-l\"\n"
    "    lib_dir_prefix = \"-L\"\n"
    "\n"
    "    tool(\"cc\") {\n"
    "      command = \"gcc \\$in -o \\$out\"\n"
    "      description = \"GCC \\$in\"\n"
    "    }\n"
    "    tool(\"cxx\") {\n"
    "      command = \"g++ \\$in -o \\$out\"\n"
    "      description = \"G++ \\$in\"\n"
    "    }\n"
    "  }\n";

Value RunTool(Scope* scope,
              const FunctionCallNode* function,
              const std::vector<Value>& args,
              BlockNode* block,
              Err* err) {
  // Find the toolchain definition we're executing inside of. The toolchain
  // function will set a property pointing to it that we'll pick up.
  Toolchain* toolchain = reinterpret_cast<Toolchain*>(
      scope->GetProperty(&kToolchainPropertyKey, NULL));
  if (!toolchain) {
    *err = Err(function->function(), "tool() called outside of toolchain().",
        "The tool() function can only be used inside a toolchain() "
        "definition.");
    return Value();
  }

  if (!EnsureSingleStringArg(function, args, err))
    return Value();
  const std::string& tool_name = args[0].string_value();
  Toolchain::ToolType tool_type = Toolchain::ToolNameToType(tool_name);
  if (tool_type == Toolchain::TYPE_NONE) {
    *err = Err(args[0], "Unknown tool type");
    return Value();
  }

  // Run the tool block.
  Scope block_scope(scope);
  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  // Extract the stuff we need.
  Toolchain::Tool t;
  if (!ReadString(block_scope, "command", &t.command, err) ||
      !ReadString(block_scope, "depfile", &t.depfile, err) ||
      // TODO(brettw) delete this once we rename "deps" -> "depsformat" in
      // the toolchain definitions. This will avoid colliding with the
      // toolchain's "deps" list. For now, accept either.
      !ReadString(block_scope, "deps", &t.depsformat, err) ||
      !ReadString(block_scope, "depsformat", &t.depsformat, err) ||
      !ReadString(block_scope, "description", &t.description, err) ||
      !ReadString(block_scope, "lib_dir_prefix", &t.lib_dir_prefix, err) ||
      !ReadString(block_scope, "lib_prefix", &t.lib_prefix, err) ||
      !ReadString(block_scope, "pool", &t.pool, err) ||
      !ReadString(block_scope, "restat", &t.restat, err) ||
      !ReadString(block_scope, "rspfile", &t.rspfile, err) ||
      !ReadString(block_scope, "rspfile_content", &t.rspfile_content, err))
    return Value();

  // Make sure there weren't any vars set in this tool that were unused.
  if (!block_scope.CheckForUnusedVars(err))
    return Value();

  toolchain->SetTool(tool_type, t);
  return Value();
}

// toolchain_args --------------------------------------------------------------

extern const char kToolchainArgs[] = "toolchain_args";
extern const char kToolchainArgs_HelpShort[] =
    "toolchain_args: Set build arguments for toolchain build setup.";
extern const char kToolchainArgs_Help[] =
    "toolchain_args: Set build arguments for toolchain build setup.\n"
    "\n"
    "  Used inside a toolchain definition to pass arguments to an alternate\n"
    "  toolchain's invocation of the build.\n"
    "\n"
    "  When you specify a target using an alternate toolchain, the master\n"
    "  build configuration file is re-interpreted in the context of that\n"
    "  toolchain (see \"gn help toolchain\"). The toolchain_args function\n"
    "  allows you to control the arguments passed into this alternate\n"
    "  invocation of the build.\n"
    "\n"
    "  Any default system arguments or arguments passed in on the command-\n"
    "  line will also be passed to the alternate invocation unless explicitly\n"
    "  overridden by toolchain_args.\n"
    "\n"
    "  The toolchain_args will be ignored when the toolchain being defined\n"
    "  is the default. In this case, it's expected you want the default\n"
    "  argument values.\n"
    "\n"
    "  See also \"gn help buildargs\" for an overview of these arguments.\n"
    "\n"
    "Example:\n"
    "  toolchain(\"my_weird_toolchain\") {\n"
    "    ...\n"
    "    toolchain_args() {\n"
    "      # Override the system values for a generic Posix system.\n"
    "      is_win = false\n"
    "      is_posix = true\n"
    "\n"
    "      # Pass this new value for specific setup for my toolchain.\n"
    "      is_my_weird_system = true\n"
    "    }\n"
    "  }\n";

Value RunToolchainArgs(Scope* scope,
                       const FunctionCallNode* function,
                       const std::vector<Value>& args,
                       BlockNode* block,
                       Err* err) {
  // Find the toolchain definition we're executing inside of. The toolchain
  // function will set a property pointing to it that we'll pick up.
  Toolchain* toolchain = reinterpret_cast<Toolchain*>(
      scope->GetProperty(&kToolchainPropertyKey, NULL));
  if (!toolchain) {
    *err = Err(function->function(),
               "toolchain_args() called outside of toolchain().",
               "The toolchain_args() function can only be used inside a "
               "toolchain() definition.");
    return Value();
  }

  if (!args.empty()) {
    *err = Err(function->function(), "This function takes no arguments.");
    return Value();
  }

  // This function makes a new scope with various variable sets on it, which
  // we then save on the toolchain to use when re-invoking the build.
  Scope block_scope(scope);
  block->ExecuteBlockInScope(&block_scope, err);
  if (err->has_error())
    return Value();

  Scope::KeyValueMap values;
  block_scope.GetCurrentScopeValues(&values);
  toolchain->args() = values;

  return Value();
}

}  // namespace functions
