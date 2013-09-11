// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/variables.h"

namespace variables {

// Built-in variables ----------------------------------------------------------

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

const char kDefaultToolchain[] = "default_toolchain";
const char kDefaultToolchain_HelpShort[] =
    "default_toolchain: [string] Label of the default toolchain.";
const char kDefaultToolchain_Help[] =
    "default_toolchain: [string] Label of the default toolchain.\n"
    "\n"
    "  A fully-qualified label representing the default toolchain, which may\n"
    "  not necessarily be the current one (see \"current_toolchain\").\n";

const char kIsLinux[] = "is_linux";
const char kIsLinux_HelpShort[] =
    "is_linux: Indicates the current build is for Linux.";
const char kIsLinux_Help[] =
    "is_linux: Indicates the current build is for Linux.\n"
    "\n"
    "  Set by default when running on Linux. Can be overridden by command-\n"
    "  line arguments or by toolchain arguments.\n";

const char kIsMac[] = "is_mac";
const char kIsMac_HelpShort[] =
    "is_mac: Indicates the current build is for Mac.";
const char kIsMac_Help[] =
    "is_mac: Indicates the current build is for Mac.\n"
    "\n"
    "  Set by default when running on Mac. Can be overridden by command-\n"
    "  line arguments or by toolchain arguments.\n";

const char kIsPosix[] = "is_posix";
const char kIsPosix_HelpShort[] =
    "is_posix: Indicates the current build is for Posix.";
const char kIsPosix_Help[] =
    "is_posix: Indicates the current build is for Posix.\n"
    "\n"
    "  Set by default when running Linux or Mac. Can be overridden by\n"
    "  command-line arguments or by toolchain arguments.\n";

const char kIsWin[] = "is_win";
const char kIsWin_HelpShort[] =
    "is_win: Indicates the current build is for Windows.";
const char kIsWin_Help[] =
    "is_win: Indicates the current build is for Windows.\n"
    "\n"
    "  Set by default when running on Windows. Can be overridden by command-\n"
    "  line arguments or by toolchain arguments.\n";

const char kPythonPath[] = "python_path";
const char kPythonPath_HelpShort[] =
    "python_path: [string] Absolute path of Python.";
const char kPythonPath_Help[] =
    "python_path: Absolute path of Python.\n"
    "\n"
    "  Normally used in toolchain definitions if running some command\n"
    "  requires Python. You will normally not need this when invoking scripts\n"
    "  since GN automatically finds it for you.\n";

const char kRelativeBuildToSourceRootDir[] =
    "relative_build_to_source_root_dir";
const char kRelativeBuildToSourceRootDir_HelpShort[] =
    "relative_build_to_source_root_dir: [string] Relative location of root";
const char kRelativeBuildToSourceRootDir_Help[] =
    "relative_build_to_source_root_dir: Relative location of root.\n"
    "\n"
    "  A relative path from the build directory to the root of the source\n"
    "  tree, with no terminating slash. The build directory will be the\n"
    "  directory when executing normal build commands (except for scripts).\n"
    "\n"
    "  If the checkout is in \"/foo/src/\" and the build directory is in\n"
    "  \"/foo/src/out/Debug/\" then the relative build to source root dir\n"
    "  will be \"../..\".\n"
    "\n"
    "  See also relative_source_root_dir.\n"
    "\n"
    "Example:\n"
    "  This is typically used to invoke tools checked out in the tree:\n"
    "\n"
    "  tool(\"link\") {\n"
    "    command = \"$relative_build_to_source_root_dir/third_party/gold/ld\n"
    "  }\n";

const char kRelativeRootGenDir[] = "relative_root_gen_dir";
const char kRelativeRootGenDir_HelpShort[] =
    "relative_root_gen_dir: [string] Relative root dir for generated files.";
const char kRelativeRootGenDir_Help[] =
    "relative_root_gen_dir: Relative root for generated files.\n"
    "\n"
    "  Relative path from the directory of the current build file to the\n"
    "  root of the generated file directory hierarchy for the current\n"
    "  toolchain.\n"
    "\n"
    "  Generally scripts should use \"relative_target_output_dir\" instead.\n"
    "\n"
    "  See also \"root_gen_dir\".\n"
    "\n"
    "Example:\n"
    "\n"
    "  If your current build file is in \"//tools\", you might write\n"
    "  args = [ \"$relative_root_gen_dir/output.txt\" ]\n";

const char kRelativeRootOutputDir[] = "relative_root_output_dir";
const char kRelativeRootOutputDir_HelpShort[] =
    "relative_root_output_dir: [string] Relative dir for output files.";
const char kRelativeRootOutputDir_Help[] =
    "relative_root_output_dir: Relative dir for output files.\n"
    "\n"
    "  Relative path from the directory of the current build file to the\n"
    "  current toolchain's root build output directory.\n"
    "\n"
    "  Generally scripts should use \"relative_target_output_dir\" instead.\n";

extern const char kRelativeSourceRootDir[] = "relative_source_root_dir";
extern const char kRelativeSourceRootDir_HelpShort[] =
    "relative_source_root_dir: [string] Relative location of root.";
extern const char kRelativeSourceRootDir_Help[] =
    "relative_source_root_dir: Relative location of root.\n"
    "\n"
    "  A relative path from the current source directory to the root of the\n"
    "  source tree, with no terminating slash. The build directory will be\n"
    "  the directory when executing scripts.\n"
    "\n"
    "  If the checkout is in \"/foo/src/\" and the build directory is in\n"
    "  \"/foo/src/out/Debug/\" then the relative build to source root dir\n"
    "  will be \"../..\".\n"
    "\n"
    "  See also relative_build_to_source_root_dir.\n"
    "\n"
    "Example:\n"
    "  This is typically used to invoke tools checked out in the tree:\n"
    "\n"
    "  tool(\"link\") {\n"
    "    command = \"$relative_build_to_source_root_dir/third_party/gold/ld\n"
    "  }\n";

const char kRelativeTargetGenDir[] = "relative_target_gen_dir";
const char kRelativeTargetGenDir_HelpShort[] =
    "relative_target_gen_dir: [string] Relative dir for generated files.";
const char kRelativeTargetGenDir_Help[] =
    "relative_target_gen_dir: Relative dir for generated files.\n"
    "\n"
    "  Relative path from the directory of the current build file to the\n"
    "  current target's generated file directory.\n"
    "\n"
    "  Normally used when invoking scripts (the current directory of which is\n"
    "  that of the invoking buildfile) that need to write files.\n"
    "\n"
    "  Scripts generating final rather than intermediate files should use the\n"
    "  \"relative_target_output_dir\" instead.\n"
    "\n"
    "  See also \"target_gen_dir\".\n"
    "\n"
    "Example:\n"
    "\n"
    "  If your current build file is in \"//tools\", you might write\n"
    "  args = [ \"$relative_root_gen_dir/output.txt\" ]\n";

const char kRelativeTargetOutputDir[] = "relative_target_output_dir";
const char kRelativeTargetOutputDir_HelpShort[] =
    "relative_target_output_dir: [string] Relative dir for build results.";
const char kRelativeTargetOutputDir_Help[] =
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

const char kRootGenDir[] = "root_gen_dir";
const char kRootGenDir_HelpShort[] =
    "root_gen_dir: [string] Absolute root dir for generated files.";
const char kRootGenDir_Help[] =
    "root_gen_dir: Absolute root dir for generated files.\n"
    "\n"
    "  Absolute path to the root of the generated output directory tree for\n"
    "  the current toolchain. An example value might be \"//out/Debug/gen\".\n"
    "  It will not have a trailing slash.\n"
    "\n"
    "  This is primarily useful for setting up include paths for generated\n"
    "  files. Scripts will want the \"relative_root_gen_dir\" instead\n"
    "  which will be relative to the scripts' current directory.\n"
    "\n"
    "  See also \"relative_root_gen_dir\" and \"target_gen_dir\".\n";

const char kTargetGenDir[] = "target_gen_dir";
const char kTargetGenDir_HelpShort[] =
    "target_gen_dir: [string] Absolute dir for generated files.";
const char kTargetGenDir_Help[] =
    "target_gen_dir: Absolute dir for generated files.\n"
    "\n"
    "  Absolute path to the target's generated file directory. If your\n"
    "  current target is in \"//tools/doom_melon\" then this value might be\n"
    "  \"//out/Debug/gen/tools/doom_melon\". It will not have a trailing\n"
    "  slash.\n"
    "\n"
    "  Scripts generating files will generally want the relative version of\n"
    "  this instead: \"relative_target_gen_dir\".\n"
    "\n"
    "Example:\n"
    "\n"
    "  # Add the gen directory to the include path.\n"
    "  args = [ target_gen_dir ]\n";

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

const char kLdflags[] = "ldflags";
const char kLdflags_HelpShort[] =
    "ldflags: [string list] Flags passed to the linker.";
const char kLdflags_Help[] =
    "ldflags: Flags passed to the linker.\n"
    "\n"
    "  A list of strings.\n"
    "\n"
    "  These flags are passed on the command-line to the linker and generally\n"
    "  specify additional system libraries to link or the library search\n"
    "  path.\n"
    "\n"
    "  Ldflags work differently than other flags in several respects. First,\n"
    "  then are inherited across static library boundaries until a shared\n"
    "  library or executable target is reached. Second, they are uniquified\n"
    "  so each flag is only passed once (the first instance of any specific\n"
    "  flag will be the one used).\n"
    "\n"
    "  The order that ldflags apply is:\n"
    "    1. Flags set on the target itself.\n"
    "    2. Flags from the configs applying to the target.\n"
    "    3. Flags from deps of the target, in order (recursively following\n"
    "       these rules).\n"
    COMMON_FLAGS_HELP;

extern const char kOutputName[] = "output_name";
extern const char kOutputName_HelpShort[] =
    "output_name: [string] Name for the output file other than the default.";
extern const char kOutputName_Help[] =
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
    "  efficiently anwyay.\n"
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
    INSERT_VARIABLE(CurrentToolchain)
    INSERT_VARIABLE(DefaultToolchain)
    INSERT_VARIABLE(IsLinux)
    INSERT_VARIABLE(IsMac)
    INSERT_VARIABLE(IsPosix)
    INSERT_VARIABLE(IsWin)
    INSERT_VARIABLE(PythonPath)
    INSERT_VARIABLE(RelativeBuildToSourceRootDir)
    INSERT_VARIABLE(RelativeRootGenDir)
    INSERT_VARIABLE(RelativeRootOutputDir)
    INSERT_VARIABLE(RelativeSourceRootDir)
    INSERT_VARIABLE(RelativeTargetGenDir)
    INSERT_VARIABLE(RelativeTargetOutputDir)
    INSERT_VARIABLE(RootGenDir)
    INSERT_VARIABLE(TargetGenDir)
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
    INSERT_VARIABLE(Data)
    INSERT_VARIABLE(Datadeps)
    INSERT_VARIABLE(Deps)
    INSERT_VARIABLE(DirectDependentConfigs)
    INSERT_VARIABLE(External)
    INSERT_VARIABLE(ForwardDependentConfigsFrom)
    INSERT_VARIABLE(HardDep)
    INSERT_VARIABLE(Ldflags)
    INSERT_VARIABLE(OutputName)
    INSERT_VARIABLE(SourcePrereqs)
    INSERT_VARIABLE(Sources)
  }
  return info_map;
}

#undef INSERT_VARIABLE

}  // namespace variables
