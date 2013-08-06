// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/variables.h"

namespace variables {

// Built-in variables ----------------------------------------------------------

extern const char kCurrentToolchain[] = "current_toolchain";
extern const char kCurrentToolchain_HelpShort[] =
    "current_toolchain: [string] Label of the current toolchain.";
extern const char kCurrentToolchain_Help[] =
    "current_toolchain: Label of the current toolchain.\n"
    "\n"
    "  A fully-qualified label representing the current toolchain. You can\n"
    "  use this to make toolchain-related decisions in the build. See also\n"
    "  \"default_toolchain\".\n"
    "\n"
    "Example:\n"
    "\n"
    "  if (current_toolchain == \"//build:64_bit_toolchain\") {\n"
    "    executable(\"output_thats_64_bit_only\") {\n"
    "      ...\n";

extern const char kDefaultToolchain[] = "default_toolchain";
extern const char kDefaultToolchain_HelpShort[] =
    "default_toolchain: [string] Label of the default toolchain.";
extern const char kDefaultToolchain_Help[] =
    "default_toolchain: [string] Label of the default toolchain.\n"
    "\n"
    "  A fully-qualified label representing the default toolchain, which may\n"
    "  not necessarily be the current one (see \"current_toolchain\").\n";

extern const char kPythonPath[] = "python_path";
extern const char kPythonPath_HelpShort[] =
    "python_path: [string] Absolute path of Python.";
extern const char kPythonPath_Help[] =
    "python_path: Absolute path of Python.\n"
    "\n"
    "  Normally used in toolchain definitions if running some command\n"
    "  requires Python. You will normally not need this when invoking scripts\n"
    "  since GN automatically finds it for you.\n";

extern const char kRelativeRootGenDir[] = "relative_root_gen_dir";
extern const char kRelativeRootGenDir_HelpShort[] =
    "relative_root_gen_dir: [string] Relative root dir for generated files.";
extern const char kRelativeRootGenDir_Help[] =
    "relative_root_gen_dir: Relative root for generated files.\n"
    "\n"
    "  Relative path from the directory of the current build file to the\n"
    "  root of the generated file directory hierarchy for the current\n"
    "  toolchain.\n"
    "\n"
    "  Generally scripts should use \"relative_target_output_dir\" instead.\n"
    "\n"
    "Example:\n"
    "\n"
    "  If your current build file is in \"//tools\", you might write\n"
    "  args = [ \"$relative_root_gen_dir/output.txt\" ]\n";

extern const char kRelativeRootOutputDir[] = "relative_root_output_dir";
extern const char kRelativeRootOutputDir_HelpShort[] =
    "relative_root_output_dir: [string] Relative dir for output files.";
extern const char kRelativeRootOutputDir_Help[] =
    "relative_root_output_dir: Relative dir for output files.\n"
    "\n"
    "  Relative path from the directory of the current build file to the\n"
    "  current toolchain's root build output directory.\n"
    "\n"
    "  Generally scripts should use \"relative_target_output_dir\" instead.\n";

extern const char kRelativeTargetGenDir[] = "relative_target_gen_dir";
extern const char kRelativeTargetGenDir_HelpShort[] =
    "relative_target_gen_dir: [string] Relative dir for generated files.";
extern const char kRelativeTargetGenDir_Help[] =
    "relative_target_gen_dir: Relative dir for generated files.\n"
    "\n"
    "  Relative path from the directory of the current build file to the\n"
    "  current target's generated file directory.\n"
    "\n"
    "  Normally used when invoking scripts (the current directory of which is\n"
    "  that of the invoking buildfile) that need to write files.\n"
    "\n"
    "  Scripts generating final rather than intermetiate files should use the\n"
    "  \"relative_target_output_dir\" instead.\n"
    "\n"
    "Example:\n"
    "\n"
    "  If your current build file is in \"//tools\", you might write\n"
    "  args = [ \"$relative_root_gen_dir/output.txt\" ]\n";

extern const char kRelativeTargetOutputDir[] = "relative_target_output_dir";
extern const char kRelativeTargetOutputDir_HelpShort[] =
    "relative_target_output_dir: [string] Relative dir for build results.";
extern const char kRelativeTargetOutputDir_Help[] =
    "relative_target_output_dir: Relative dir for build results."
    "\n"
    "  Relative path from the directory of the current build file to the\n"
    "  current target's generated file directory.\n"
    "\n"
    "  Normally used when invoking scripts (the current directory of which is\n"
    "  that of the invoking buildfile) that need to write files.\n"
    "\n"
    "  Scripts generating intermediate files rather than final output files\n"
    "  should use the \"relative_target_output_dir\" instead.\n"
    "\n"
    "Example:\n"
    "\n"
    "  If your current build file is in \"//tools\", you might write\n"
    "  args = [ \"$relative_target_output_dir/final.lib\" ]\n";

// Target variables ------------------------------------------------------------

const char kAllDependentConfigs[] = "all_dependent_configs";
const char kAllDependentConfigs_HelpShort[] =
    "all_dependent_configs: [label list] Configs to be forced on dependents.";
const char kAllDependentConfigs_Help[] =
    "all_dependent_configs: Configs to be forced on dependents.\n"
    "\n"
    "  A list of config labels.\n"
    "\n"
    "  All targets depending on this one, and recursively, all targets\n"
    "  depending on those, will have the configs listed in this variable\n"
    "  added to them.\n"
    "\n"
    "  This addition happens in a second phase once a target and all of its\n"
    "  dependencies have been resolved. Therefore, a target will not see\n"
    "  these force-added configs in their \"configs\" variable while the\n"
    "  script is running, and then can not be removed. As a result, this\n"
    "  capability should generally only be used to add defines and include\n"
    "  directories necessary to compile a target's headers.\n"
    "\n"
    "  See also \"direct_dependent_configs\".\n";

const char kCflags[] = "cflags";
const char kCflags_HelpShort[] =
    "cflags: [string list] Flags passed to all C compiler variants.";
// Avoid writing long help for each variant.
const char kCommonCflagsHelp[] =
    "cflags*: Flags passed to the C compiler.\n"
    "\n"
    "  A list of strings.\n"
    "\n"
    "  \"cflags\" are passed to all invocations of the C, C++, Objective C,\n"
    "  and Objective C++ compilers.\n"
    "\n"
    "  To target one of these variants individually, use \"cflags_c\",\n"
    "  \"cflags_cc\", \"cflags_objc\", and \"cflags_objcc\", respectively.\n"
    "  These variant-specific versions will be appended to the \"cflags\".\n";
const char* kCflags_Help = kCommonCflagsHelp;

const char kCflagsC[] = "cflags_c";
const char kCflagsC_HelpShort[] =
    "cflags_c: [string list] Flags passed to the C compiler.";
const char* kCflagsC_Help = kCommonCflagsHelp;

const char kCflagsCC[] = "cflags_cc";
const char kCflagsCC_HelpShort[] =
    "cflags_cc: [string list] Flags passed to the C++ compiler.";
const char* kCflagsCC_Help = kCommonCflagsHelp;

const char kCflagsObjC[] = "cflags_objc";
const char kCflagsObjC_HelpShort[] =
    "cflags_objc: [string list] Flags passed to the Objective C compiler.";
const char* kCflagsObjC_Help = kCommonCflagsHelp;

const char kCflagsObjCC[] = "cflags_objcc";
const char kCflagsObjCC_HelpShort[] =
    "cflags_objcc: [string list] Flags passed to the Objective C++ compiler.";
const char* kCflagsObjCC_Help = kCommonCflagsHelp;

const char kConfigs[] = "configs";
const char kConfigs_HelpShort[] =
    "configs: [label list] Configs applying to this target.";
const char kConfigs_Help[] =
    "configs: Configs applying to this target.\n"
    "\n"
    "  A list of config labels.\n"
    "\n"
    "  The includes, defines, etc. in each config are appended in the order\n"
    "  they appear to the compile command for each file in the target. They\n"
    "  will appear after the includes, defines, etc. that the target sets\n"
    "  directly.\n"
    "\n"
    "  The build configuration script will generally set up the default\n"
    "  configs applying to a given target type (see \"set_defaults\").\n"
    "  When a target is being defined, it can add to or remove from this\n"
    "  list.\n"
    "\n"
    "Example:\n"
    "  static_library(\"foo\") {\n"
    "    configs -= \"//build:no_rtti\"  # Don't use the default RTTI config.\n"
    "    configs += \":mysettings\"      # Add some of our own settings.\n"
    "  }\n";

const char kDatadeps[] = "datadeps";
const char kDatadeps_HelpShort[] =
    "datadeps: [label list] Non-linked dependencies.";
const char kDatadeps_Help[] =
    "datadeps: Non-linked dependencies.\n"
    "\n"
    "  A list of target labels.\n"
    "\n"
    "  Specifies dependencies of a target that are not actually linked into\n"
    "  the current target. Such dependencies will built and will be available\n"
    "  at runtime.\n"
    "\n"
    "  This is normally used for things like plugins or helper programs that\n"
    "  a target needs at runtime.\n"
    "\n"
    "  See also \"deps\".\n";

const char kDefines[] = "defines";
const char kDefines_HelpShort[] =
    "defines: [string list] C preprocessor defines.";
const char kDefines_Help[] =
    "defines: C preprocessor defines.\n"
    "\n"
    "  A list of strings\n"
    "\n"
    "  These strings will be passed to the C/C++ compiler as #defines. The\n"
    "  strings may or may not include an \"=\" to assign a value.\n"
    "\n"
    "Example:\n"
    "  defines = [ \"AWESOME_FEATURE\", \"LOG_LEVEL=3\" ]\n";

const char kDeps[] = "deps";
const char kDeps_HelpShort[] =
    "deps: [label list] Linked dependencies.";
const char kDeps_Help[] =
    "deps: Linked dependencies.\n"
    "\n"
    "  A list of target labels.\n"
    "\n"
    "  Specifies dependencies of a target. Shared and dynamic libraries will\n"
    "  be linked into the current target. Other target types that can't be\n"
    "  linked (like custom scripts and groups) listed in \"deps\" will be\n"
    "  treated as \"datadeps\". Likewise, if the current target isn't\n"
    "  linkable, then all deps will be treated as \"datadeps\".\n"
    "\n"
    "  See also \"datadeps\".\n";

const char kDirectDependentConfigs[] = "direct_dependent_configs";
const char kDirectDependentConfigs_HelpShort[] =
    "direct_dependent_configs: [label list] Configs to be forced on "
    "dependents.";
const char kDirectDependentConfigs_Help[] =
    "direct_dependent_configs: Configs to be forced on dependents.\n"
    "\n"
    "  A list of config labels.\n"
    "\n"
    "  Targets directly referencing this one will have the configs listed in\n"
    "  this variable added to them.\n"
    "\n"
    "  This addition happens in a second phase once a target and all of its\n"
    "  dependencies have been resolved. Therefore, a target will not see\n"
    "  these force-added configs in their \"configs\" variable while the\n"
    "  script is running, and then can not be removed. As a result, this\n"
    "  capability should generally only be used to add defines and include\n"
    "  directories necessary to compile a target's headers.\n"
    "\n"
    "  See also \"all_dependent_configs\".\n";

const char kLdflags[] = "ldflags";
const char kLdflags_HelpShort[] =
    "ldflags: [string list] Flags passed to the linker.";
const char kLdflags_Help[] =
    "ldflags: Flags passed to the linker.\n"
    "\n"
    "  A list of strings.\n"
    "\n"
    "  These flags are passed on the command-line to the linker.\n";

const char kSources[] = "sources";
const char kSources_HelpShort[] =
    "sources: [file list] Source files for a target.";
const char kSources_Help[] =
    "sources: Source files for a target\n"
    "\n"
    "  A list of files relative to the current buildfile.\n";

// -----------------------------------------------------------------------------

VariableInfo::VariableInfo()
    : help_short(NULL),
      help(NULL) {
}

VariableInfo::VariableInfo(const char* in_help_short, const char* in_help)
    : help_short(in_help_short),
      help(in_help) {
}

#define INSERT_VARIABLE(var) \
    info_map[k##var] = VariableInfo(k##var##_HelpShort, k##var##_Help);

const VariableInfoMap& GetBuiltinVariables() {
  static VariableInfoMap info_map;
  if (info_map.empty()) {
    INSERT_VARIABLE(CurrentToolchain)
    INSERT_VARIABLE(DefaultToolchain)
    INSERT_VARIABLE(PythonPath)
    INSERT_VARIABLE(RelativeRootGenDir)
    INSERT_VARIABLE(RelativeRootOutputDir)
    INSERT_VARIABLE(RelativeTargetGenDir)
    INSERT_VARIABLE(RelativeTargetOutputDir)
  }
  return info_map;
}

const VariableInfoMap& GetTargetVariables() {
  static VariableInfoMap info_map;
  if (info_map.empty()) {
    INSERT_VARIABLE(AllDependentConfigs)
    INSERT_VARIABLE(Cflags)
    INSERT_VARIABLE(CflagsC)
    INSERT_VARIABLE(CflagsCC)
    INSERT_VARIABLE(CflagsObjC)
    INSERT_VARIABLE(CflagsObjCC)
    INSERT_VARIABLE(Configs)
    INSERT_VARIABLE(Datadeps)
    INSERT_VARIABLE(Deps)
    INSERT_VARIABLE(DirectDependentConfigs)
    INSERT_VARIABLE(Ldflags)
    INSERT_VARIABLE(Sources)
  }
  return info_map;
}

#undef INSERT_VARIABLE

}  // namespace variables
