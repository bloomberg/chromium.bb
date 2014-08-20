// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/variables.h"

namespace variables {

// Built-in variables ----------------------------------------------------------

const char kComponentMode[] = "component_mode";
const char kComponentMode_HelpShort[] =
    "component_mode: [string] Specifies the meaning of the component() call.";
const char kComponentMode_Help[] =
    "component_mode: Specifies the meaning of the component() call.\n"
    "\n"
    "  This value is looked up whenever a \"component\" target type is\n"
    "  encountered. The value controls whether the given target is a shared\n"
    "  or a static library.\n"
    "\n"
    "  The initial value will be empty, which will cause a call to\n"
    "  component() to throw an error. Typically this value will be set in the\n"
    "  build config script.\n"
    "\n"
    "Possible values:\n"
    "  - \"shared_library\"\n"
    "  - \"source_set\"\n"
    "  - \"static_library\"\n";

const char kCpuArch[] = "cpu_arch";
const char kCpuArch_HelpShort[] =
    "cpu_arch: [string] Current processor architecture.";
const char kCpuArch_Help[] =
    "cpu_arch: Current processor architecture.\n"
    "\n"
    "  The initial value is based on the current architecture of the host\n"
    "  system. However, the build configuration can set this to any value.\n"
    "\n"
    "  This value is not used internally by GN for any purpose, so you can\n"
    "  set it to whatever value is relevant to your build.\n"
    "\n"
    "Possible initial values set by GN:\n"
    "  - \"x86\"\n"
    "  - \"x64\"\n"
    "  - \"arm\"\n"
    "  - \"mipsel\"\n";

const char kCurrentToolchain[] = "current_toolchain";
const char kCurrentToolchain_HelpShort[] =
    "current_toolchain: [string] Label of the current toolchain.";
const char kCurrentToolchain_Help[] =
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

const char kBuildCpuArch[] = "build_cpu_arch";
const char kBuildCpuArch_HelpShort[] =
    "build_cpu_arch: [string] The default value for the \"cpu_arch\" "
    "variable.";
const char kBuildCpuArch_Help[] =
    "build_cpu_arch: The default value for the \"cpu_arch\" variable.\n"
    "\n"
    "  This value has the same definition as \"cpu_arch\" (see\n"
    "  \"gn help cpu_arch\") but should be treated as read-only. This is so\n"
    "  the build can override the \"cpu_arch\" variable for doing\n"
    "  cross-compiles, but can still access the host build system's CPU\n"
    "  architecture.\n";

const char kBuildOs[] = "build_os";
const char kBuildOs_HelpShort[] =
    "build_os: [string] The default value for the \"os\" variable.";
const char kBuildOs_Help[] =
    "build_os: [string] The default value for the \"os\" variable.\n"
    "\n"
    "  This value has the same definition as \"os\" (see \"gn help os\") but\n"
    "  should be treated as read-only. This is so the build can override\n"
    "  the \"os\" variable for doing cross-compiles, but can still access\n"
    "  the host build system's operating system type.\n";

const char kDefaultToolchain[] = "default_toolchain";
const char kDefaultToolchain_HelpShort[] =
    "default_toolchain: [string] Label of the default toolchain.";
const char kDefaultToolchain_Help[] =
    "default_toolchain: [string] Label of the default toolchain.\n"
    "\n"
    "  A fully-qualified label representing the default toolchain, which may\n"
    "  not necessarily be the current one (see \"current_toolchain\").\n";

const char kOs[] = "os";
const char kOs_HelpShort[] =
    "os: [string] Indicates the operating system of the current build.";
const char kOs_Help[] =
    "os: Indicates the operating system of the current build."
    "\n"
    "  This value is set by default based on the current host operating\n"
    "  system. The build configuration can override the value to anything\n"
    "  it wants, or it can be set via the build arguments on the command\n"
    "  line.\n"
    "\n"
    "  If you want to know the default value without any overrides, you can\n"
    "  use \"default_os\" (see \"gn help default_os\").\n"
    "\n"
    "  Note that this returns the most specific value. So even though\n"
    "  Android and ChromeOS are both Linux, the more specific value will\n"
    "  be returned.\n"
    "\n"
    "Some possible values:\n"
    "  - \"amiga\"\n"
    "  - \"android\"\n"
    "  - \"chromeos\"\n"
    "  - \"ios\"\n"
    "  - \"linux\"\n"
    "  - \"mac\"\n"
    "  - \"win\"\n";

const char kPythonPath[] = "python_path";
const char kPythonPath_HelpShort[] =
    "python_path: [string] Absolute path of Python.";
const char kPythonPath_Help[] =
    "python_path: Absolute path of Python.\n"
    "\n"
    "  Normally used in toolchain definitions if running some command\n"
    "  requires Python. You will normally not need this when invoking scripts\n"
    "  since GN automatically finds it for you.\n";

const char kRootBuildDir[] = "root_build_dir";
const char kRootBuildDir_HelpShort[] =
  "root_build_dir: [string] Directory where build commands are run.";
const char kRootBuildDir_Help[] =
  "root_build_dir: [string] Directory where build commands are run.\n"
  "\n"
  "  This is the root build output directory which will be the current\n"
  "  directory when executing all compilers and scripts.\n"
  "\n"
  "  Most often this is used with rebase_path (see \"gn help rebase_path\")\n"
  "  to convert arguments to be relative to a script's current directory.\n";

const char kRootGenDir[] = "root_gen_dir";
const char kRootGenDir_HelpShort[] =
    "root_gen_dir: [string] Directory for the toolchain's generated files.";
const char kRootGenDir_Help[] =
    "root_gen_dir: Directory for the toolchain's generated files.\n"
    "\n"
    "  Absolute path to the root of the generated output directory tree for\n"
    "  the current toolchain. An example value might be \"//out/Debug/gen\".\n"
    "  It will not have a trailing slash.\n"
    "\n"
    "  This is primarily useful for setting up include paths for generated\n"
    "  files. If you are passing this to a script, you will want to pass it\n"
    "  through rebase_path() (see \"gn help rebase_path\") to convert it\n"
    "  to be relative to the build directory.\n"
    "\n"
    "  See also \"target_gen_dir\" which is usually a better location for\n"
    "  generated files. It will be inside the root generated dir.\n";

const char kRootOutDir[] = "root_out_dir";
const char kRootOutDir_HelpShort[] =
    "root_out_dir: [string] Root directory for toolchain output files.";
const char kRootOutDir_Help[] =
    "root_out_dir: [string] Root directory for toolchain output files.\n"
    "\n"
    "  Absolute path to the root of the output directory tree for the current\n"
    "  toolchain. An example value might be \"//out/Debug/gen\". It will not\n"
    "  have a trailing slash.\n"
    "\n"
    "  This is primarily useful for setting up script calls. If you are\n"
    "  passing this to a script, you will want to pass it through\n"
    "  rebase_path() (see \"gn help rebase_path\") to convert it\n"
    "  to be relative to the build directory.\n"
    "\n"
    "  See also \"target_out_dir\" which is usually a better location for\n"
    "  output files. It will be inside the root output dir.\n"
    "\n"
    "Example:\n"
    "\n"
    "  action(\"myscript\") {\n"
    "    # Pass the output dir to the script.\n"
    "    args = [ \"-o\", rebase_path(root_out_dir, root_build_dir) ]\n"
    "  }\n";

const char kTargetGenDir[] = "target_gen_dir";
const char kTargetGenDir_HelpShort[] =
    "target_gen_dir: [string] Directory for a target's generated files.";
const char kTargetGenDir_Help[] =
    "target_gen_dir: Directory for a target's generated files.\n"
    "\n"
    "  Absolute path to the target's generated file directory. If your\n"
    "  current target is in \"//tools/doom_melon\" then this value might be\n"
    "  \"//out/Debug/gen/tools/doom_melon\". It will not have a trailing\n"
    "  slash.\n"
    "\n"
    "  This is primarily useful for setting up include paths for generated\n"
    "  files. If you are passing this to a script, you will want to pass it\n"
    "  through rebase_path() (see \"gn help rebase_path\") to convert it\n"
    "  to be relative to the build directory.\n"
    "\n"
    "  See also \"gn help root_gen_dir\".\n"
    "\n"
    "Example:\n"
    "\n"
    "  action(\"myscript\") {\n"
    "    # Pass the generated output dir to the script.\n"
    "    args = [ \"-o\", rebase_path(target_gen_dir, root_build_dir) ]"
    "\n"
    "  }\n";

const char kTargetOutDir[] = "target_out_dir";
const char kTargetOutDir_HelpShort[] =
    "target_out_dir: [string] Directory for target output files.";
const char kTargetOutDir_Help[] =
    "target_out_dir: [string] Directory for target output files.\n"
    "\n"
    "  Absolute path to the target's generated file directory. If your\n"
    "  current target is in \"//tools/doom_melon\" then this value might be\n"
    "  \"//out/Debug/obj/tools/doom_melon\". It will not have a trailing\n"
    "  slash.\n"
    "\n"
    "  This is primarily useful for setting up arguments for calling\n"
    "  scripts. If you are passing this to a script, you will want to pass it\n"
    "  through rebase_path() (see \"gn help rebase_path\") to convert it\n"
    "  to be relative to the build directory.\n"
    "\n"
    "  See also \"gn help root_out_dir\".\n"
    "\n"
    "Example:\n"
    "\n"
    "  action(\"myscript\") {\n"
    "    # Pass the output dir to the script.\n"
    "    args = [ \"-o\", rebase_path(target_out_dir, root_build_dir) ]"
    "\n"
    "  }\n";

// Target variables ------------------------------------------------------------

#define COMMON_ORDERING_HELP \
    "\n" \
    "Ordering of flags and values:\n" \
    "\n" \
    "  1. Those set on the current target (not in a config).\n" \
    "  2. Those set on the \"configs\" on the target in order that the\n" \
    "     configs appear in the list.\n" \
    "  3. Those set on the \"all_dependent_configs\" on the target in order\n" \
    "     that the configs appear in the list.\n" \
    "  4. Those set on the \"direct_dependent_configs\" on the target in\n" \
    "     order that those configs appear in the list.\n" \
    "  5. all_dependent_configs pulled from dependencies, in the order of\n" \
    "     the \"deps\" list. This is done recursively. If a config appears\n" \
    "     more than once, only the first occurance will be used.\n" \
    "  6. direct_dependent_configs pulled from dependencies, in the order\n" \
    "     of the \"deps\" list. If a dependency has\n" \
    "     \"forward_dependent_configs_from\", they will be applied\n" \
    "     recursively.\n"

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
    "  added to them. These configs will also apply to the current target.\n"
    "\n"
    "  This addition happens in a second phase once a target and all of its\n"
    "  dependencies have been resolved. Therefore, a target will not see\n"
    "  these force-added configs in their \"configs\" variable while the\n"
    "  script is running, and then can not be removed. As a result, this\n"
    "  capability should generally only be used to add defines and include\n"
    "  directories necessary to compile a target's headers.\n"
    "\n"
    "  See also \"direct_dependent_configs\".\n"
    COMMON_ORDERING_HELP;

const char kArgs[] = "args";
const char kArgs_HelpShort[] =
    "args: [string list] Arguments passed to an action.";
const char kArgs_Help[] =
    "args: Arguments passed to an action.\n"
    "\n"
    "  For action and action_foreach targets, args is the list of arguments\n"
    "  to pass to the script. Typically you would use source expansion (see\n"
    "  \"gn help source_expansion\") to insert the source file names.\n"
    "\n"
    "  See also \"gn help action\" and \"gn help action_foreach\".\n";

const char kCflags[] = "cflags";
const char kCflags_HelpShort[] =
    "cflags: [string list] Flags passed to all C compiler variants.";
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
    "  These variant-specific versions will be appended to the \"cflags\".\n"
    COMMON_ORDERING_HELP;
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
    "  The include_dirs, defines, etc. in each config are appended in the\n"
    "  order they appear to the compile command for each file in the target.\n"
    "  They will appear after the include_dirs, defines, etc. that the target\n"
    "  sets directly.\n"
    "\n"
    "  The build configuration script will generally set up the default\n"
    "  configs applying to a given target type (see \"set_defaults\").\n"
    "  When a target is being defined, it can add to or remove from this\n"
    "  list.\n"
    COMMON_ORDERING_HELP
    "\n"
    "Example:\n"
    "  static_library(\"foo\") {\n"
    "    configs -= \"//build:no_rtti\"  # Don't use the default RTTI config.\n"
    "    configs += \":mysettings\"      # Add some of our own settings.\n"
    "  }\n";

const char kData[] = "data";
const char kData_HelpShort[] =
    "data: [file list] Runtime data file dependencies.";
const char kData_Help[] =
    "data: Runtime data file dependencies.\n"
    "\n"
    "  Lists files required to run the given target. These are typically\n"
    "  data files.\n"
    "\n"
    "  Appearing in the \"data\" section does not imply any special handling\n"
    "  such as copying them to the output directory. This is just used for\n"
    "  declaring runtime dependencies. There currently isn't a good use for\n"
    "  these but it is envisioned that test data can be listed here for use\n"
    "  running automated tests.\n"
    "\n"
    "  See also \"gn help inputs\" and \"gn help datadeps\", both of\n"
    "  which actually affect the build in concrete ways.\n";

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
    "  See also \"gn help deps\" and \"gn help data\".\n"
    "\n"
    "Example:\n"
    "  executable(\"foo\") {\n"
    "    deps = [ \"//base\" ]\n"
    "    datadeps = [ \"//plugins:my_runtime_plugin\" ]\n"
    "  }\n";

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
    COMMON_ORDERING_HELP
    "\n"
    "Example:\n"
    "  defines = [ \"AWESOME_FEATURE\", \"LOG_LEVEL=3\" ]\n";

const char kDepfile[] = "depfile";
const char kDepfile_HelpShort[] =
    "depfile: [string] File name for input dependencies for actions.";
const char kDepfile_Help[] =
    "depfile: [string] File name for input dependencies for actions.\n"
    "\n"
    "  If nonempty, this string specifies that the current action or\n"
    "  action_foreach target will generate the given \".d\" file containing\n"
    "  the dependencies of the input. Empty or unset means that the script\n"
    "  doesn't generate the files.\n"
    "\n"
    "  The .d file should go in the target output directory. If you have more\n"
    "  than one source file that the script is being run over, you can use\n"
    "  the output file expansions described in \"gn help action_foreach\" to\n"
    "  name the .d file according to the input."
    "\n"
    "  The format is that of a Makefile, and all of the paths should be\n"
    "  relative to the root build directory.\n"
    "\n"
    "Example:\n"
    "  action_foreach(\"myscript_target\") {\n"
    "    script = \"myscript.py\"\n"
    "    sources = [ ... ]\n"
    "\n"
    "    # Locate the depfile in the output directory named like the\n"
    "    # inputs but with a \".d\" appended.\n"
    "    depfile = \"$relative_target_output_dir/{{source_name}}.d\"\n"
    "\n"
    "    # Say our script uses \"-o <d file>\" to indicate the depfile.\n"
    "    args = [ \"{{source}}\", \"-o\", depfile ]\n"
    "  }\n";

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
    "  linked (like actions and groups) listed in \"deps\" will be treated\n"
    "  as \"datadeps\". Likewise, if the current target isn't linkable, then\n"
    "  all deps will be treated as \"datadeps\".\n"
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
    "  this variable added to them. These configs will also apply to the\n"
    "  current target.\n"
    "\n"
    "  This addition happens in a second phase once a target and all of its\n"
    "  dependencies have been resolved. Therefore, a target will not see\n"
    "  these force-added configs in their \"configs\" variable while the\n"
    "  script is running, and then can not be removed. As a result, this\n"
    "  capability should generally only be used to add defines and include\n"
    "  directories necessary to compile a target's headers.\n"
    "\n"
    "  See also \"all_dependent_configs\".\n"
    COMMON_ORDERING_HELP;

const char kForwardDependentConfigsFrom[] = "forward_dependent_configs_from";
const char kForwardDependentConfigsFrom_HelpShort[] =
    "forward_dependent_configs_from: [label list] Forward dependent's configs.";
const char kForwardDependentConfigsFrom_Help[] =
    "forward_dependent_configs_from\n"
    "\n"
    "  A list of target labels.\n"
    "\n"
    "  Exposes the direct_dependent_configs from a dependent target as\n"
    "  direct_dependent_configs of the current one. Each label in this list\n"
    "  must also be in the deps.\n"
    "\n"
    "  Sometimes you depend on a child library that exports some necessary\n"
    "  configuration via direct_dependent_configs. If your target in turn\n"
    "  exposes the child library's headers in its public headers, it might\n"
    "  mean that targets that depend on you won't work: they'll be seeing the\n"
    "  child library's code but not the necessary configuration. This list\n"
    "  specifies which of your deps' direct dependent configs to expose as\n"
    "  your own.\n"
    "\n"
    "Examples:\n"
    "\n"
    "  If we use a given library \"a\" from our public headers:\n"
    "\n"
    "    deps = [ \":a\", \":b\", ... ]\n"
    "    forward_dependent_configs_from = [ \":a\" ]\n"
    "\n"
    "  This example makes a \"transparent\" target that forwards a dependency\n"
    "  to another:\n"
    "\n"
    "    group(\"frob\") {\n"
    "      if (use_system_frob) {\n"
    "        deps = \":system_frob\"\n"
    "      } else {\n"
    "        deps = \"//third_party/fallback_frob\"\n"
    "      }\n"
    "      forward_dependent_configs_from = deps\n"
    "    }\n";

const char kIncludeDirs[] = "include_dirs";
const char kIncludeDirs_HelpShort[] =
    "include_dirs: [directory list] Additional include directories.";
const char kIncludeDirs_Help[] =
    "include_dirs: Additional include directories.\n"
    "\n"
    "  A list of source directories.\n"
    "\n"
    "  The directories in this list will be added to the include path for\n"
    "  the files in the affected target.\n"
    COMMON_ORDERING_HELP
    "\n"
    "Example:\n"
    "  include_dirs = [ \"src/include\", \"//third_party/foo\" ]\n";

const char kInputs[] = "inputs";
const char kInputs_HelpShort[] =
    "inputs: [file list] Additional compile-time dependencies.";
const char kInputs_Help[] =
    "inputs: Additional compile-time dependencies.\n"
    "\n"
    "  Inputs are compile-time dependencies of the current target. This means\n"
    "  that all inputs must be available before compiling any of the sources\n"
    "  or executing any actions.\n"
    "\n"
    "  Inputs are typically only used for action and action_foreach targets.\n"
    "\n"
    "Inputs for actions\n"
    "\n"
    "  For action and action_foreach targets, inputs should be the inputs to\n"
    "  script that don't vary. These should be all .py files that the script\n"
    "  uses via imports (the main script itself will be an implcit dependency\n"
    "  of the action so need not be listed).\n"
    "\n"
    "  For action targets, inputs should be the entire set of inputs the\n"
    "  script needs. For action_foreach targets, inputs should be the set of\n"
    "  dependencies that don't change. These will be applied to each script\n"
    "  invocation over the sources.\n"
    "\n"
    "  Note that another way to declare input dependencies from an action\n"
    "  is to have the action write a depfile (see \"gn help depfile\"). This\n"
    "  allows the script to dynamically write input dependencies, that might\n"
    "  not be known until actually executing the script. This is more\n"
    "  efficient than doing processing while running GN to determine the\n"
    "  inputs, and is easier to keep in-sync than hardcoding the list.\n"
    "\n"
    "Inputs for binary targets\n"
    "\n"
    "  Any input dependencies will be resolved before compiling any sources.\n"
    "  Normally, all actions that a target depends on will be run before any\n"
    "  files in a target are compiled. So if you depend on generated headers,\n"
    "  you do not typically need to list them in the inputs section.\n"
    "\n"
    "Example\n"
    "\n"
    "  action(\"myscript\") {\n"
    "    script = \"domything.py\"\n"
    "    inputs = [ \"input.data\" ]\n"
    "  }\n";

const char kLdflags[] = "ldflags";
const char kLdflags_HelpShort[] =
    "ldflags: [string list] Flags passed to the linker.";
const char kLdflags_Help[] =
    "ldflags: Flags passed to the linker.\n"
    "\n"
    "  A list of strings.\n"
    "\n"
    "  These flags are passed on the command-line to the linker and generally\n"
    "  specify various linking options. Most targets will not need these and\n"
    "  will use \"libs\" and \"lib_dirs\" instead.\n"
    "\n"
    "  ldflags are NOT pushed to dependents, so applying ldflags to source\n"
    "  sets or static libraries will be a no-op. If you want to apply ldflags\n"
    "  to dependent targets, put them in a config and set it in the\n"
    "  all_dependent_configs or direct_dependent_configs.\n";

#define COMMON_LIB_INHERITANCE_HELP \
    "\n" \
    "  libs and lib_dirs work differently than other flags in two respects.\n" \
    "  First, then are inherited across static library boundaries until a\n" \
    "  shared library or executable target is reached. Second, they are\n" \
    "  uniquified so each one is only passed once (the first instance of it\n" \
    "  will be the one used).\n"

const char kLibDirs[] = "lib_dirs";
const char kLibDirs_HelpShort[] =
    "lib_dirs: [directory list] Additional library directories.";
const char kLibDirs_Help[] =
    "lib_dirs: Additional library directories.\n"
    "\n"
    "  A list of directories.\n"
    "\n"
    "  Specifies additional directories passed to the linker for searching\n"
    "  for the required libraries. If an item is not an absolute path, it\n"
    "  will be treated as being relative to the current build file.\n"
    COMMON_LIB_INHERITANCE_HELP
    COMMON_ORDERING_HELP
    "\n"
    "Example:\n"
    "  lib_dirs = [ \"/usr/lib/foo\", \"lib/doom_melon\" ]\n";

const char kLibs[] = "libs";
const char kLibs_HelpShort[] =
    "libs: [string list] Additional libraries to link.";
const char kLibs_Help[] =
    "libs: Additional libraries to link.\n"
    "\n"
    "  A list of strings.\n"
    "\n"
    "  These files will be passed to the linker, which will generally search\n"
    "  the library include path. Unlike a normal list of files, they will be\n"
    "  passed to the linker unmodified rather than being treated as file\n"
    "  names relative to the current build file. Generally you would set\n"
    "  the \"lib_dirs\" so your library is found. If you need to specify\n"
    "  a path, you can use \"rebase_path\" to convert a path to be relative\n"
    "  to the build directory.\n"
    "\n"
    "  When constructing the linker command, the \"lib_prefix\" attribute of\n"
    "  the linker tool in the current toolchain will be prepended to each\n"
    "  library. So your BUILD file should not specify the switch prefix\n"
    "  (like \"-l\"). On Mac, libraries ending in \".framework\" will be\n"
    "  special-cased: the switch \"-framework\" will be prepended instead of\n"
    "  the lib_prefix, and the \".framework\" suffix will be trimmed.\n"
    COMMON_LIB_INHERITANCE_HELP
    COMMON_ORDERING_HELP
    "\n"
    "Examples:\n"
    "  On Windows:\n"
    "    libs = [ \"ctl3d.lib\" ]\n"
    "  On Linux:\n"
    "    libs = [ \"ld\" ]\n";

const char kOutputExtension[] = "output_extension";
const char kOutputExtension_HelpShort[] =
    "output_extension: [string] Value to use for the output's file extension.";
const char kOutputExtension_Help[] =
    "output_extension: Value to use for the output's file extension.\n"
    "\n"
    "  Normally the file extension for a target is based on the target\n"
    "  type and the operating system, but in rare cases you will need to\n"
    "  override the name (for example to use \"libfreetype.so.6\" instead\n"
    "  of libfreetype.so on Linux).";

const char kOutputName[] = "output_name";
const char kOutputName_HelpShort[] =
    "output_name: [string] Name for the output file other than the default.";
const char kOutputName_Help[] =
    "output_name: Define a name for the output file other than the default.\n"
    "\n"
    "  Normally the output name of a target will be based on the target name,\n"
    "  so the target \"//foo/bar:bar_unittests\" will generate an output\n"
    "  file such as \"bar_unittests.exe\" (using Windows as an example).\n"
    "\n"
    "  Sometimes you will want an alternate name to avoid collisions or\n"
    "  if the internal name isn't appropriate for public distribution.\n"
    "\n"
    "  The output name should have no extension or prefixes, these will be\n"
    "  added using the default system rules. For example, on Linux an output\n"
    "  name of \"foo\" will produce a shared library \"libfoo.so\".\n"
    "\n"
    "  This variable is valid for all binary output target types.\n"
    "\n"
    "Example:\n"
    "  static_library(\"doom_melon\") {\n"
    "    output_name = \"fluffy_bunny\"\n"
    "  }\n";

const char kOutputs[] = "outputs";
const char kOutputs_HelpShort[] =
    "outputs: [file list] Output files for actions and copy targets.";
const char kOutputs_Help[] =
    "outputs: Output files for actions and copy targets.\n"
    "\n"
    "  Outputs is valid for \"copy\", \"action\", and \"action_foreach\"\n"
    "  target types and indicates the resulting files. The values may contain\n"
    "  source expansions to generate the output names from the sources (see\n"
    "  \"gn help source_expansion\").\n"
    "\n"
    "  For copy targets, the outputs is the destination for the copied\n"
    "  file(s). For actions, the outputs should be the list of files\n"
    "  generated by the script.\n";

const char kPublic[] = "public";
const char kPublic_HelpShort[] =
    "public: [file list] Declare public header files for a target.";
const char kPublic_Help[] =
    "public: Declare public header files for a target.\n"
    "\n"
    "  A list of files that other targets can include. These permissions are\n"
    "  checked via the \"check\" command (see \"gn help check\").\n"
    "\n"
    "  If no public files are declared, other targets (assuming they have\n"
    "  visibility to depend on this target can include any file in the\n"
    "  sources list. If this variable is defined on a target, dependent\n"
    "  targets may only include files on this whitelist.\n"
    "\n"
    "  Header file permissions are also subject to visibility. A target\n"
    "  must be visible to another target to include any files from it at all\n"
    "  and the public headers indicate which subset of those files are\n"
    "  permitted. See \"gn help visibility\" for more.\n"
    "\n"
    "  Public files are inherited through the dependency tree. So if there is\n"
    "  a dependency A -> B -> C, then A can include C's public headers.\n"
    "  However, the same is NOT true of visibility, so unless A is in C's\n"
    "  visibility list, the include will be rejected.\n"
    "\n"
    "  GN only knows about files declared in the \"sources\" and \"public\"\n"
    "  sections of targets. If a file is included that is not known to the\n"
    "  build, it will be allowed.\n"
    "\n"
    "Examples:\n"
    "  These exact files are public:\n"
    "    public = [ \"foo.h\", \"bar.h\" ]\n"
    "\n"
    "  No files are public (no targets may include headers from this one):\n"
    "    public = []\n";

const char kScript[] = "script";
const char kScript_HelpShort[] =
    "script: [file name] Script file for actions.";
const char kScript_Help[] =
    "script: Script file for actions.\n"
    "\n"
    "  An absolute or buildfile-relative file name of a Python script to run\n"
    "  for a action and action_foreach targets (see \"gn help action\" and\n"
    "  \"gn help action_foreach\").\n";

const char kSources[] = "sources";
const char kSources_HelpShort[] =
    "sources: [file list] Source files for a target.";
const char kSources_Help[] =
    "sources: Source files for a target\n"
    "\n"
    "  A list of files relative to the current buildfile.\n";

const char kVisibility[] = "visibility";
const char kVisibility_HelpShort[] =
    "visibility: [label list] A list of labels that can depend on a target.";
const char kVisibility_Help[] =
    "visibility: A list of labels that can depend on a target.\n"
    "\n"
    "  A label or a list of labels and label patterns that define which\n"
    "  targets can depend on the current one. These permissions are checked\n"
    "  via then \"check\" command (see \"gn help check\").\n"
    "\n"
    "  If visibility is not defined, it defaults to public (\"*\").\n"
    "\n"
    "  If visibility is defined, only the targets with labels that match it\n"
    "  can depend on the current target. The empty list means no targets\n"
    "  can depend on the current target.\n"
    "\n"
    "  Tip: Often you will want the same visibility for all targets in a\n"
    "  BUILD file. In this case you can just put the definition at the top,\n"
    "  outside of any target, and the targets will inherit that scope and see\n"
    "  the definition.\n"
    "\n"
    "Matching:\n"
    "\n"
    "  You can specify \"*\" but the inputs aren't general patterns. The\n"
    "  following classes of patterns are supported:\n"
    "\n"
    "   - Explicit (no wildcard):\n"
    "       \"//foo/bar:baz\"\n"
    "       \":baz\"\n"
    "   - Wildcard target names:\n"
    "       \"//foo/bar:*\" (any target in the //foo/bar/BUILD.gn file)\n"
    "       \":*\"  (any target in the current build file)\n"
    "   - Wildcard directory names (\"*\" is only supported at the end)\n"
    "       \"*\"  (any target anywhere)\n"
    "       \"//foo/bar/*\"  (any target in any subdir of //foo/bar)\n"
    "       \"./*\"  (any target in the current build file or sub dirs)\n"
    "\n"
    "  The toolchain (normally an implicit part of a label) is ignored when\n"
    "  checking visibility.\n"
    "\n"
    "Examples:\n"
    "\n"
    "  Only targets in the current buildfile (\"private\", the default):\n"
    "    visibility = \":*\"\n"
    "\n"
    "  No targets (used for targets that should be leaf nodes):\n"
    "    visibility = []\n"
    "\n"
    "  Any target (\"public\"):\n"
    "    visibility = \"*\"\n"
    "\n"
    "  All targets in the current directory and any subdirectory:\n"
    "    visibility = \"./*\"\n"
    "\n"
    "  Any target in \"//bar/BUILD.gn\":\n"
    "    visibility = \"//bar:*\"\n"
    "\n"
    "  Any target in \"//bar/\" or any subdirectory thereof:\n"
    "    visibility = \"//bar/*\"\n"
    "\n"
    "  Just these specific targets:\n"
    "    visibility = [ \":mything\", \"//foo:something_else\" ]\n"
    "\n"
    "  Any target in the current directory and any subdirectory thereof, plus\n"
    "  any targets in \"//bar/\" and any subdirectory thereof.\n"
    "    visibility = [ \"./*\", \"//bar/*\" ]\n";

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
    INSERT_VARIABLE(BuildCpuArch)
    INSERT_VARIABLE(BuildOs)
    INSERT_VARIABLE(CpuArch)
    INSERT_VARIABLE(ComponentMode)
    INSERT_VARIABLE(CurrentToolchain)
    INSERT_VARIABLE(DefaultToolchain)
    INSERT_VARIABLE(Os)
    INSERT_VARIABLE(PythonPath)
    INSERT_VARIABLE(RootBuildDir)
    INSERT_VARIABLE(RootGenDir)
    INSERT_VARIABLE(RootOutDir)
    INSERT_VARIABLE(TargetGenDir)
    INSERT_VARIABLE(TargetOutDir)
  }
  return info_map;
}

const VariableInfoMap& GetTargetVariables() {
  static VariableInfoMap info_map;
  if (info_map.empty()) {
    INSERT_VARIABLE(AllDependentConfigs)
    INSERT_VARIABLE(Args)
    INSERT_VARIABLE(Cflags)
    INSERT_VARIABLE(CflagsC)
    INSERT_VARIABLE(CflagsCC)
    INSERT_VARIABLE(CflagsObjC)
    INSERT_VARIABLE(CflagsObjCC)
    INSERT_VARIABLE(Configs)
    INSERT_VARIABLE(Data)
    INSERT_VARIABLE(Datadeps)
    INSERT_VARIABLE(Defines)
    INSERT_VARIABLE(Depfile)
    INSERT_VARIABLE(Deps)
    INSERT_VARIABLE(DirectDependentConfigs)
    INSERT_VARIABLE(ForwardDependentConfigsFrom)
    INSERT_VARIABLE(IncludeDirs)
    INSERT_VARIABLE(Inputs)
    INSERT_VARIABLE(Ldflags)
    INSERT_VARIABLE(Libs)
    INSERT_VARIABLE(LibDirs)
    INSERT_VARIABLE(OutputExtension)
    INSERT_VARIABLE(OutputName)
    INSERT_VARIABLE(Outputs)
    INSERT_VARIABLE(Public)
    INSERT_VARIABLE(Script)
    INSERT_VARIABLE(Sources)
    INSERT_VARIABLE(Visibility)
  }
  return info_map;
}

#undef INSERT_VARIABLE

}  // namespace variables
