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
    "  custom(\"myscript\") {\n"
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
    "  custom(\"myscript\") {\n"
    "    # Pass the generated output dir to the script.\n"
    "    args = [ \"-o\", rebase_path(target_gen_dir, root_build_dir) ]"
    "\n"
    "  }\n";

const char kTargetOutDir[] = "target_out_dir";
const char kTargetOutDir_HelpShort[] =
    "target_out_dir: [string] Directory for target output files.";
const char kTargetOutDir_Help[] =
    "target_out_dir: [string] Directory for target output files."
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
    "  custom(\"myscript\") {\n"
    "    # Pass the output dir to the script.\n"
    "    args = [ \"-o\", rebase_path(target_out_dir, root_build_dir) ]"
    "\n"
    "  }\n";

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
    "  added to them. These configs will also apply to the current target.\n"
    "\n"
    "  This addition happens in a second phase once a target and all of its\n"
    "  dependencies have been resolved. Therefore, a target will not see\n"
    "  these force-added configs in their \"configs\" variable while the\n"
    "  script is running, and then can not be removed. As a result, this\n"
    "  capability should generally only be used to add defines and include\n"
    "  directories necessary to compile a target's headers.\n"
    "\n"
    "  See also \"direct_dependent_configs\".\n";

const char kArgs[] = "args";
const char kArgs_HelpShort[] =
    "args: [string list] Arguments passed to a custom script.";
const char kArgs_Help[] =
    "args: Arguments passed to a custom script.\n"
    "\n"
    "  For custom script targets, args is the list of arguments to pass\n"
    "  to the script. Typically you would use source expansion (see\n"
    "  \"gn help source_expansion\") to insert the source file names.\n"
    "\n"
    "  See also \"gn help custom\".\n";

const char kCflags[] = "cflags";
const char kCflags_HelpShort[] =
    "cflags: [string list] Flags passed to all C compiler variants.";
// Avoid writing long help for each variant.
#define COMMON_FLAGS_HELP \
    "\n"\
    "  Flags are never quoted. If your flag includes a string that must be\n"\
    "  quoted, you must do it yourself. This also means that you can\n"\
    "  specify more than one flag in a string if necessary (\"--foo --bar\")\n"\
    "  and have them be seen as separate by the tool.\n"
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
    COMMON_FLAGS_HELP;
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
    "  See also \"gn help source_prereqs\" and \"gn help datadeps\", both of\n"
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
    "\n"
    "Example:\n"
    "  defines = [ \"AWESOME_FEATURE\", \"LOG_LEVEL=3\" ]\n";

const char kDepfile[] = "depfile";
const char kDepfile_HelpShort[] =
    "depfile: [string] File name for input dependencies for custom targets.";
const char kDepfile_Help[] =
    "depfile: [string] File name for input dependencies for custom targets.\n"
    "\n"
    "  If nonempty, this string specifies that the current \"custom\" target\n"
    "  will generate the given \".d\" file containing the dependencies of the\n"
    "  input. Empty or unset means that the script doesn't generate the\n"
    "  files.\n"
    "\n"
    "  The .d file should go in the target output directory. If you have more\n"
    "  than one source file that the script is being run over, you can use\n"
    "  the output file expansions described in \"gn help custom\" to name the\n"
    "  .d file according to the input."
    "\n"
    "  The format is that of a Makefile, and all of the paths should be\n"
    "  relative to the root build directory.\n"
    "\n"
    "Example:\n"
    "  custom(\"myscript_target\") {\n"
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
    "  See also \"all_dependent_configs\".\n";

const char kExternal[] = "external";
const char kExternal_HelpShort[] =
    "external: [boolean] Declares a target as externally generated.";
const char kExternal_Help[] =
    "external: Declares a target as externally generated.\n"
    "\n"
    "  External targets are treated like normal targets as far as dependent\n"
    "  targets are concerned, but do not actually have their .ninja file\n"
    "  written to disk. This allows them to be generated by an external\n"
    "  program (e.g. GYP).\n"
    "\n"
    "  See also \"gn help gyp\".\n"
    "\n"
    "Example:\n"
    "  static_library(\"foo\") {\n"
    "    external = true\n"
    "  }\n";

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

const char kGypFile[] = "gyp_file";
const char kGypFile_HelpShort[] =
    "gyp_file: [file name] Name of GYP file to write to in GYP mode.";
const char kGypFile_Help[] =
    "gyp_file: Name of GYP file to write to in GYP mode.\n"
    "\n"
    "  See \"gn help gyp\" for an overview of how this works.\n"
    "\n"
    "  Tip: If all targets in a given BUILD.gn file should go in the same\n"
    "  GYP file, just put gyp_file = \"foo\" at the top of the file and\n"
    "  the variable will be in scope for all targets.\n";

const char kGypHeader[] = "gyp_header";
const char kGypHeader_HelpShort[] =
    "gyp_header: [string] Extra stuff to prepend to GYP files.";
const char kGypHeader_Help[] =
    "gyp_header: Extra stuff to prepend to GYP files.\n"
    "\n"
    "  A Python dictionary string. This will be inserted after the initial\n"
    "  \"{\" in the GYP file. It is expected this is used to define the\n"
    "  make_global_settings.\n"
    "\n"
    "  This string should end in a comma to keep the python dictionary syntax\n"
    "  valid when everything is concatenated.\n";

const char kHardDep[] = "hard_dep";
const char kHardDep_HelpShort[] =
    "hard_dep: [boolean] Indicates a target should be built before dependees.";
const char kHardDep_Help[] =
    "hard_dep: Indicates a target should be built before dependees.\n"
    "\n"
    "  Ninja's default is to assume that targets can be compiled\n"
    "  independently. This breaks down for generated files that are included\n"
    "  in other targets because Ninja doesn't know to run the generator\n"
    "  before compiling the source file.\n"
    "\n"
    "  Setting \"hard_dep\" to true on a target means that no sources in\n"
    "  targets depending directly on this one will be compiled until this\n"
    "  target is complete. It will introduce a Ninja implicit dependency\n"
    "  from those sources to this target. This flag is not transitive so\n"
    "  it will only affect direct dependents, which will cause problems if\n"
    "  a direct dependent uses this generated file in a public header that a\n"
    "  third target consumes. Try not to do this.\n"
    "\n"
    "  See also \"gn help source_prereqs\" which allows you to specify the\n"
    "  exact generated file dependency on the target consuming it.\n"
    "\n"
    "Example:\n"
    "  executable(\"foo\") {\n"
    "    # myresource will be run before any of the sources in this target\n"
    "    # are compiled.\n"
    "    deps = [ \":myresource\" ]\n"
    "    ...\n"
    "  }\n"
    "\n"
    "  custom(\"myresource\") {\n"
    "    hard_dep = true\n"
    "    script = \"my_generator.py\"\n"
    "    outputs = \"$target_gen_dir/myresource.h\"\n"
    "  }\n";

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
    "\n"
    "Example:\n"
    "  include_dirs = [ \"src/include\", \"//third_party/foo\" ]\n";

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
    COMMON_FLAGS_HELP;

#define COMMON_LIB_INHERITANCE_HELP \
    "\n" \
    "  libs and lib_dirs work differently than other flags in two respects.\n" \
    "  First, then are inherited across static library boundaries until a\n" \
    "  shared library or executable target is reached. Second, they are\n" \
    "  uniquified so each one is only passed once (the first instance of it\n" \
    "  will be the one used).\n" \
    "\n" \
    "  The order that libs/lib_dirs apply is:\n" \
    "    1. Ones set on the target itself.\n" \
    "    2. Ones from the configs applying to the target.\n" \
    "    3. Ones from deps of the target, in order (recursively following\n" \
    "       these rules).\n"

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
    "outputs: [file list] Output files for custom script and copy targets.";
const char kOutputs_Help[] =
    "outputs: Output files for custom script and copy targets.\n"
    "\n"
    "  Outputs is valid for \"copy\" and \"custom\" target types and\n"
    "  indicates the resulting files. The values may contain source\n"
    "  expansions to generate the output names from the sources (see\n"
    "  \"gn help source_expansion\").\n"
    "\n"
    "  For copy targets, the outputs is the destination for the copied\n"
    "  file(s). For custom script targets, the outputs should be the list of\n"
    "  files generated by the script.\n";

const char kScript[] = "script";
const char kScript_HelpShort[] =
    "script: [file name] Script file for custom script targets.";
const char kScript_Help[] =
    "script: Script file for custom script targets.\n"
    "\n"
    "  An absolute or buildfile-relative file name of a Python script to run\n"
    "  for a custom script target (see \"gn help custom\").\n";

const char kSourcePrereqs[] = "source_prereqs";
const char kSourcePrereqs_HelpShort[] =
    "source_prereqs: [file list] Additional compile-time dependencies.";
const char kSourcePrereqs_Help[] =
    "source_prereqs: Additional compile-time dependencies.\n"
    "\n"
    "  Inputs are compile-time dependencies of the current target. This means\n"
    "  that all source prerequisites must be available before compiling any\n"
    "  of the sources.\n"
    "\n"
    "  If one of your sources #includes a generated file, that file must be\n"
    "  available before that source file is compiled. For subsequent builds,\n"
    "  the \".d\" files will list the include dependencies of each source\n"
    "  and Ninja can know about that dependency to make sure it's generated\n"
    "  before compiling your source file. However, for the first run it's\n"
    "  not possible for Ninja to know about this dependency.\n"
    "\n"
    "  Source prerequisites solves this problem by declaring such\n"
    "  dependencies. It will introduce a Ninja \"implicit\" dependency for\n"
    "  each source file in the target on the listed files.\n"
    "\n"
    "  For binary targets, the files in the \"source_prereqs\" should all be\n"
    "  listed in the \"outputs\" section of another target. There is no\n"
    "  reason to declare static source files as source prerequisites since\n"
    "  the normal include file dependency management will handle them more\n"
    "  efficiently anyway.\n"
    "\n"
    "  For custom script targets that don't generate \".d\" files, the\n"
    "  \"source_prereqs\" section is how you can list known compile-time\n"
    "  dependencies your script may have.\n"
    "\n"
    "  See also \"gn help data\" and \"gn help datadeps\" (which declare\n"
    "  run-time rather than compile-time dependencies), and\n"
    "  \"gn help hard_dep\" which allows you to declare the source dependency\n"
    "  on the target generating a file rather than the target consuming it.\n"
    "\n"
    "Examples:\n"
    "  executable(\"foo\") {\n"
    "    sources = [ \"foo.cc\" ]\n"
    "    source_prereqs = [ \"$root_gen_dir/something/generated_data.h\" ]\n"
    "  }\n"
    "\n"
    "  custom(\"myscript\") {\n"
    "    script = \"domything.py\"\n"
    "    source_prereqs = [ \"input.data\" ]\n"
    "  }\n";

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
    INSERT_VARIABLE(Depfile)
    INSERT_VARIABLE(Deps)
    INSERT_VARIABLE(DirectDependentConfigs)
    INSERT_VARIABLE(External)
    INSERT_VARIABLE(ForwardDependentConfigsFrom)
    INSERT_VARIABLE(GypFile)
    INSERT_VARIABLE(GypHeader)
    INSERT_VARIABLE(HardDep)
    INSERT_VARIABLE(IncludeDirs)
    INSERT_VARIABLE(Ldflags)
    INSERT_VARIABLE(Libs)
    INSERT_VARIABLE(LibDirs)
    INSERT_VARIABLE(OutputExtension)
    INSERT_VARIABLE(OutputName)
    INSERT_VARIABLE(Outputs)
    INSERT_VARIABLE(Script)
    INSERT_VARIABLE(SourcePrereqs)
    INSERT_VARIABLE(Sources)
  }
  return info_map;
}

#undef INSERT_VARIABLE

}  // namespace variables
