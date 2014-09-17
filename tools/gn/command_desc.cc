// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>
#include <sstream>

#include "base/command_line.h"
#include "tools/gn/commands.h"
#include "tools/gn/config.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/item.h"
#include "tools/gn/label.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/target.h"
#include "tools/gn/variables.h"

namespace commands {

namespace {

// Prints the given directory in a nice way for the user to view.
std::string FormatSourceDir(const SourceDir& dir) {
#if defined(OS_WIN)
  // On Windows we fix up system absolute paths to look like native ones.
  // Internally, they'll look like "/C:\foo\bar/"
  if (dir.is_system_absolute()) {
    std::string buf = dir.value();
    if (buf.size() > 3 && buf[2] == ':') {
      buf.erase(buf.begin());  // Erase beginning slash.
      return buf;
    }
  }
#endif
  return dir.value();
}

void RecursiveCollectChildDeps(const Target* target, std::set<Label>* result);

void RecursiveCollectDeps(const Target* target, std::set<Label>* result) {
  if (result->find(target->label()) != result->end())
    return;  // Already did this target.
  result->insert(target->label());

  RecursiveCollectChildDeps(target, result);
}

void RecursiveCollectChildDeps(const Target* target, std::set<Label>* result) {
  for (DepsIterator iter(target); !iter.done(); iter.Advance())
    RecursiveCollectDeps(iter.target(), result);
}

// Prints dependencies of the given target (not the target itself). If the
// set is non-null, new targets encountered will be added to the set, and if
// a dependency is in the set already, it will not be recused into. When the
// set is null, all dependencies will be printed.
void RecursivePrintDeps(const Target* target,
                        const Label& default_toolchain,
                        std::set<const Target*>* seen_targets,
                        int indent_level) {
  // Combine all deps into one sorted list.
  std::vector<LabelTargetPair> sorted_deps;
  for (DepsIterator iter(target); !iter.done(); iter.Advance())
    sorted_deps.push_back(iter.pair());
  std::sort(sorted_deps.begin(), sorted_deps.end(),
            LabelPtrLabelLess<Target>());

  std::string indent(indent_level * 2, ' ');
  for (size_t i = 0; i < sorted_deps.size(); i++) {
    const Target* cur_dep = sorted_deps[i].ptr;

    OutputString(indent +
        cur_dep->label().GetUserVisibleName(default_toolchain));
    bool print_children = true;
    if (seen_targets) {
      if (seen_targets->find(cur_dep) == seen_targets->end()) {
        // New target, mark it visited.
        seen_targets->insert(cur_dep);
      } else {
        // Already seen.
        print_children = false;
        // Only print "..." if something is actually elided, which means that
        // the current target has children.
        if (!cur_dep->public_deps().empty() ||
            !cur_dep->private_deps().empty() ||
            !cur_dep->data_deps().empty())
          OutputString("...");
      }
    }

    OutputString("\n");
    if (print_children) {
      RecursivePrintDeps(cur_dep, default_toolchain, seen_targets,
                         indent_level + 1);
    }
  }
}

void PrintDeps(const Target* target, bool display_header) {
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  Label toolchain_label = target->label().GetToolchainLabel();

  // Tree mode is separate.
  if (cmdline->HasSwitch("tree")) {
    if (display_header)
      OutputString("\nDependency tree:\n");

    if (cmdline->HasSwitch("all")) {
      // Show all tree deps with no eliding.
      RecursivePrintDeps(target, toolchain_label, NULL, 1);
    } else {
      // Don't recurse into duplicates.
      std::set<const Target*> seen_targets;
      RecursivePrintDeps(target, toolchain_label, &seen_targets, 1);
    }
    return;
  }

  // Collect the deps to display.
  std::vector<Label> deps;
  if (cmdline->HasSwitch("all")) {
    // Show all dependencies.
    if (display_header)
      OutputString("\nAll recursive dependencies:\n");

    std::set<Label> all_deps;
    RecursiveCollectChildDeps(target, &all_deps);
    for (std::set<Label>::iterator i = all_deps.begin();
         i != all_deps.end(); ++i)
      deps.push_back(*i);
  } else {
    // Show direct dependencies only.
    if (display_header) {
      OutputString(
          "\nDirect dependencies "
          "(try also \"--all\", \"--tree\", or even \"--all --tree\"):\n");
    }
    for (DepsIterator iter(target); !iter.done(); iter.Advance())
      deps.push_back(iter.label());
  }

  std::sort(deps.begin(), deps.end());
  for (size_t i = 0; i < deps.size(); i++)
    OutputString("  " + deps[i].GetUserVisibleName(toolchain_label) + "\n");
}

void PrintForwardDependentConfigsFrom(const Target* target,
                                      bool display_header) {
  if (target->forward_dependent_configs().empty())
    return;

  if (display_header)
    OutputString("\nforward_dependent_configs_from:\n");

  // Collect the sorted list of deps.
  std::vector<Label> forward;
  for (size_t i = 0; i < target->forward_dependent_configs().size(); i++)
    forward.push_back(target->forward_dependent_configs()[i].label);
  std::sort(forward.begin(), forward.end());

  Label toolchain_label = target->label().GetToolchainLabel();
  for (size_t i = 0; i < forward.size(); i++)
    OutputString("  " + forward[i].GetUserVisibleName(toolchain_label) + "\n");
}

// libs and lib_dirs are special in that they're inherited. We don't currently
// implement a blame feature for this since the bottom-up inheritance makes
// this difficult.
void PrintLibDirs(const Target* target, bool display_header) {
  const OrderedSet<SourceDir>& lib_dirs = target->all_lib_dirs();
  if (lib_dirs.empty())
    return;

  if (display_header)
    OutputString("\nlib_dirs\n");

  for (size_t i = 0; i < lib_dirs.size(); i++)
    OutputString("    " + FormatSourceDir(lib_dirs[i]) + "\n");
}

void PrintLibs(const Target* target, bool display_header) {
  const OrderedSet<std::string>& libs = target->all_libs();
  if (libs.empty())
    return;

  if (display_header)
    OutputString("\nlibs\n");

  for (size_t i = 0; i < libs.size(); i++)
    OutputString("    " + libs[i] + "\n");
}

void PrintPublic(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\npublic:\n");

  if (target->all_headers_public()) {
    OutputString("  [All headers listed in the sources are public.]\n");
    return;
  }

  Target::FileList public_headers = target->public_headers();
  std::sort(public_headers.begin(), public_headers.end());
  for (size_t i = 0; i < public_headers.size(); i++)
    OutputString("  " + public_headers[i].value() + "\n");
}

void PrintCheckIncludes(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\ncheck_includes:\n");

  if (target->check_includes())
    OutputString("  true\n");
  else
    OutputString("  false\n");
}

void PrintAllowCircularIncludesFrom(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\nallow_circular_includes_from:\n");

  Label toolchain_label = target->label().GetToolchainLabel();
  const std::set<Label>& allow = target->allow_circular_includes_from();
  for (std::set<Label>::const_iterator iter = allow.begin();
       iter != allow.end(); ++iter)
    OutputString("  " + iter->GetUserVisibleName(toolchain_label) + "\n");
}

void PrintVisibility(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\nvisibility:\n");

  OutputString(target->visibility().Describe(2, false));
}

void PrintTestonly(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\ntestonly:\n");

  if (target->testonly())
    OutputString("  true\n");
  else
    OutputString("  false\n");
}

void PrintConfigsVector(const Target* target,
                        const LabelConfigVector& configs,
                        const std::string& heading,
                        bool display_header) {
  if (configs.empty())
    return;

  // Don't sort since the order determines how things are processed.
  if (display_header)
    OutputString("\n" + heading + " (in order applying):\n");

  Label toolchain_label = target->label().GetToolchainLabel();
  for (size_t i = 0; i < configs.size(); i++) {
    OutputString("  " +
        configs[i].label.GetUserVisibleName(toolchain_label) + "\n");
  }
}

void PrintConfigsVector(const Target* target,
                        const UniqueVector<LabelConfigPair>& configs,
                        const std::string& heading,
                        bool display_header) {
  if (configs.empty())
    return;

  // Don't sort since the order determines how things are processed.
  if (display_header)
    OutputString("\n" + heading + " (in order applying):\n");

  Label toolchain_label = target->label().GetToolchainLabel();
  for (size_t i = 0; i < configs.size(); i++) {
    OutputString("  " +
        configs[i].label.GetUserVisibleName(toolchain_label) + "\n");
  }
}

void PrintConfigs(const Target* target, bool display_header) {
  PrintConfigsVector(target, target->configs().vector(), "configs",
                     display_header);
}

void PrintPublicConfigs(const Target* target, bool display_header) {
  PrintConfigsVector(target, target->public_configs(),
                     "public_configs", display_header);
}

void PrintAllDependentConfigs(const Target* target, bool display_header) {
  PrintConfigsVector(target, target->all_dependent_configs(),
                     "all_dependent_configs", display_header);
}

void PrintFileList(const Target::FileList& files,
                   const std::string& header,
                   bool indent_extra,
                   bool display_header) {
  if (files.empty())
    return;

  if (display_header)
    OutputString("\n" + header + ":\n");

  std::string indent = indent_extra ? "    " : "  ";

  Target::FileList sorted = files;
  std::sort(sorted.begin(), sorted.end());
  for (size_t i = 0; i < sorted.size(); i++)
    OutputString(indent + sorted[i].value() + "\n");
}

void PrintSources(const Target* target, bool display_header) {
  PrintFileList(target->sources(), "sources", false, display_header);
}

void PrintInputs(const Target* target, bool display_header) {
  PrintFileList(target->inputs(), "inputs", false, display_header);
}

void PrintOutputs(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\noutputs:\n");

  if (target->output_type() == Target::ACTION) {
    // Action, print out outputs, don't apply sources to it.
    for (size_t i = 0; i < target->action_values().outputs().list().size();
         i++) {
      OutputString("  " +
                   target->action_values().outputs().list()[i].AsString() +
                   "\n");
    }
  } else {
    const SubstitutionList& outputs = target->action_values().outputs();
    if (!outputs.required_types().empty()) {
      // Display the pattern and resolved pattern separately, since there are
      // subtitutions used.
      OutputString("  Output pattern:\n");
      for (size_t i = 0; i < outputs.list().size(); i++)
        OutputString("    " + outputs.list()[i].AsString() + "\n");

      // Now display what that resolves to given the sources.
      OutputString("\n  Resolved output file list:\n");
    }

    // Resolved output list.
    std::vector<SourceFile> output_files;
    SubstitutionWriter::ApplyListToSources(target->settings(), outputs,
                                           target->sources(), &output_files);
    PrintFileList(output_files, "", true, false);
  }
}

void PrintScript(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\nscript:\n");
  OutputString("  " + target->action_values().script().value() + "\n");
}

void PrintArgs(const Target* target, bool display_header) {
  if (display_header)
    OutputString("\nargs:\n");
  for (size_t i = 0; i < target->action_values().args().list().size(); i++) {
    OutputString("  " +
                 target->action_values().args().list()[i].AsString() + "\n");
  }
}

void PrintDepfile(const Target* target, bool display_header) {
  if (target->action_values().depfile().empty())
    return;
  if (display_header)
    OutputString("\ndepfile:\n");
  OutputString("  " + target->action_values().depfile().AsString() + "\n");
}

// Attribute the origin for attributing from where a target came from. Does
// nothing if the input is null or it does not have a location.
void OutputSourceOfDep(const ParseNode* origin, std::ostream& out) {
  if (!origin)
    return;
  Location location = origin->GetRange().begin();
  out << "       (Added by " + location.file()->name().value() << ":"
      << location.line_number() << ")\n";
}

// Templatized writer for writing out different config value types.
template<typename T> struct DescValueWriter {};
template<> struct DescValueWriter<std::string> {
  void operator()(const std::string& str, std::ostream& out) const {
    out << "    " << str << "\n";
  }
};
template<> struct DescValueWriter<SourceDir> {
  void operator()(const SourceDir& dir, std::ostream& out) const {
    out << "    " << FormatSourceDir(dir) << "\n";
  }
};

// Writes a given config value type to the string, optionally with attribution.
// This should match RecursiveTargetConfigToStream in the order it traverses.
template<typename T> void OutputRecursiveTargetConfig(
    const Target* target,
    const char* header_name,
    const std::vector<T>& (ConfigValues::* getter)() const) {
  bool display_blame = CommandLine::ForCurrentProcess()->HasSwitch("blame");

  DescValueWriter<T> writer;
  std::ostringstream out;

  for (ConfigValuesIterator iter(target); !iter.done(); iter.Next()) {
    if ((iter.cur().*getter)().empty())
      continue;

    // Optional blame sub-head.
    if (display_blame) {
      const Config* config = iter.GetCurrentConfig();
      if (config) {
        // Source of this value is a config.
        out << "  From " << config->label().GetUserVisibleName(false) << "\n";
        OutputSourceOfDep(iter.origin(), out);
      } else {
        // Source of this value is the target itself.
        out << "  From " << target->label().GetUserVisibleName(false) << "\n";
      }
    }

    // Actual values.
    ConfigValuesToStream(iter.cur(), getter, writer, out);
  }

  std::string out_str = out.str();
  if (!out_str.empty()) {
    OutputString("\n" + std::string(header_name) + "\n");
    OutputString(out_str);
  }
}

}  // namespace

// desc ------------------------------------------------------------------------

const char kDesc[] = "desc";
const char kDesc_HelpShort[] =
    "desc: Show lots of insightful information about a target.";
const char kDesc_Help[] =
    "gn desc <out_dir> <target label> [<what to show>]\n"
    "        [--blame] [--all | --tree]\n"
    "\n"
    "  Displays information about a given labeled target for the given build.\n"
    "  The build parameters will be taken for the build in the given\n"
    "  <out_dir>.\n"
    "\n"
    "Possibilities for <what to show>:\n"
    "  (If unspecified an overall summary will be displayed.)\n"
    "\n"
    "  sources\n"
    "      Source files.\n"
    "\n"
    "  inputs\n"
    "      Additional input dependencies.\n"
    "\n"
    "  public\n"
    "      Public header files.\n"
    "\n"
    "  check_includes\n"
    "      Whether \"gn check\" checks this target for include usage.\n"
    "\n"
    "  allow_circular_includes_from\n"
    "      Permit includes from these targets.\n"
    "\n"
    "  visibility\n"
    "      Prints which targets can depend on this one.\n"
    "\n"
    "  testonly\n"
    "      Whether this target may only be used in tests.\n"
    "\n"
    "  configs\n"
    "      Shows configs applied to the given target, sorted in the order\n"
    "      they're specified. This includes both configs specified in the\n"
    "      \"configs\" variable, as well as configs pushed onto this target\n"
    "      via dependencies specifying \"all\" or \"direct\" dependent\n"
    "      configs.\n"
    "\n"
    "  deps [--all | --tree]\n"
    "      Show immediate (or, when \"--all\" or \"--tree\" is specified,\n"
    "      recursive) dependencies of the given target. \"--tree\" shows them\n"
    "      in a tree format with duplicates elided (noted by \"...\").\n"
    "      \"--all\" shows them sorted alphabetically. Using both flags will\n"
    "      print a tree with no omissions. The \"deps\", \"public_deps\", and\n"
    "      \"data_deps\" will all be included.\n"
    "\n"
    "  public_configs\n"
    "  all_dependent_configs\n"
    "      Shows the labels of configs applied to targets that depend on this\n"
    "      one (either directly or all of them).\n"
    "\n"
    "  forward_dependent_configs_from\n"
    "      Shows the labels of dependencies for which dependent configs will\n"
    "      be pushed to targets depending on the current one.\n"
    "\n"
    "  script\n"
    "  args\n"
    "  depfile\n"
    "      Actions only. The script and related values.\n"
    "\n"
    "  outputs\n"
    "      Outputs for script and copy target types.\n"
    "\n"
    "  defines       [--blame]\n"
    "  include_dirs  [--blame]\n"
    "  cflags        [--blame]\n"
    "  cflags_cc     [--blame]\n"
    "  cflags_cxx    [--blame]\n"
    "  ldflags       [--blame]\n"
    "  lib_dirs\n"
    "  libs\n"
    "      Shows the given values taken from the target and all configs\n"
    "      applying. See \"--blame\" below.\n"
    "\n"
    "  --blame\n"
    "      Used with any value specified by a config, this will name\n"
    "      the config that specified the value. This doesn't currently work\n"
    "      for libs and lib_dirs because those are inherited and are more\n"
    "      complicated to figure out the blame (patches welcome).\n"
    "\n"
    "Note:\n"
    "  This command will show the full name of directories and source files,\n"
    "  but when directories and source paths are written to the build file,\n"
    "  they will be adjusted to be relative to the build directory. So the\n"
    "  values for paths displayed by this command won't match (but should\n"
    "  mean the same thing).\n"
    "\n"
    "Examples:\n"
    "  gn desc out/Debug //base:base\n"
    "      Summarizes the given target.\n"
    "\n"
    "  gn desc out/Foo :base_unittests deps --tree\n"
    "      Shows a dependency tree of the \"base_unittests\" project in\n"
    "      the current directory.\n"
    "\n"
    "  gn desc out/Debug //base defines --blame\n"
    "      Shows defines set for the //base:base target, annotated by where\n"
    "      each one was set from.\n";

#define OUTPUT_CONFIG_VALUE(name, type) \
    OutputRecursiveTargetConfig<type>(target, #name, &ConfigValues::name);

int RunDesc(const std::vector<std::string>& args) {
  if (args.size() != 2 && args.size() != 3) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn desc <out_dir> <target_name> [<what to display>]\"")
        .PrintToStdout();
    return 1;
  }

  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  if (!setup->DoSetup(args[0], false))
    return 1;
  if (!setup->Run())
    return 1;

  const Target* target = ResolveTargetFromCommandLineString(setup, args[1]);
  if (!target)
    return 1;

#define CONFIG_VALUE_HANDLER(name, type) \
    } else if (what == #name) { OUTPUT_CONFIG_VALUE(name, type)

  if (args.size() == 3) {
    // User specified one thing to display.
    const std::string& what = args[2];
    if (what == variables::kConfigs) {
      PrintConfigs(target, false);
    } else if (what == variables::kPublicConfigs) {
      PrintPublicConfigs(target, false);
    } else if (what == variables::kAllDependentConfigs) {
      PrintAllDependentConfigs(target, false);
    } else if (what == variables::kForwardDependentConfigsFrom) {
      PrintForwardDependentConfigsFrom(target, false);
    } else if (what == variables::kSources) {
      PrintSources(target, false);
    } else if (what == variables::kPublic) {
      PrintPublic(target, false);
    } else if (what == variables::kCheckIncludes) {
      PrintCheckIncludes(target, false);
    } else if (what == variables::kAllowCircularIncludesFrom) {
      PrintAllowCircularIncludesFrom(target, false);
    } else if (what == variables::kVisibility) {
      PrintVisibility(target, false);
    } else if (what == variables::kTestonly) {
      PrintTestonly(target, false);
    } else if (what == variables::kInputs) {
      PrintInputs(target, false);
    } else if (what == variables::kScript) {
      PrintScript(target, false);
    } else if (what == variables::kArgs) {
      PrintArgs(target, false);
    } else if (what == variables::kDepfile) {
      PrintDepfile(target, false);
    } else if (what == variables::kOutputs) {
      PrintOutputs(target, false);
    } else if (what == variables::kDeps) {
      PrintDeps(target, false);
    } else if (what == variables::kLibDirs) {
      PrintLibDirs(target, false);
    } else if (what == variables::kLibs) {
      PrintLibs(target, false);

    CONFIG_VALUE_HANDLER(defines, std::string)
    CONFIG_VALUE_HANDLER(include_dirs, SourceDir)
    CONFIG_VALUE_HANDLER(cflags, std::string)
    CONFIG_VALUE_HANDLER(cflags_c, std::string)
    CONFIG_VALUE_HANDLER(cflags_cc, std::string)
    CONFIG_VALUE_HANDLER(cflags_objc, std::string)
    CONFIG_VALUE_HANDLER(cflags_objcc, std::string)
    CONFIG_VALUE_HANDLER(ldflags, std::string)

    } else {
      OutputString("Don't know how to display \"" + what + "\".\n");
      return 1;
    }

#undef CONFIG_VALUE_HANDLER
    return 0;
  }

  // Display summary.

  // Display this only applicable to binary targets.
  bool is_binary_output =
    target->output_type() != Target::GROUP &&
    target->output_type() != Target::COPY_FILES &&
    target->output_type() != Target::ACTION &&
    target->output_type() != Target::ACTION_FOREACH;

  // Generally we only want to display toolchains on labels when the toolchain
  // is different than the default one for this target (which we always print
  // in the header).
  Label target_toolchain = target->label().GetToolchainLabel();

  // Header.
  OutputString("Target: ", DECORATION_YELLOW);
  OutputString(target->label().GetUserVisibleName(false) + "\n");
  OutputString("Type: ", DECORATION_YELLOW);
  OutputString(std::string(
      Target::GetStringForOutputType(target->output_type())) + "\n");
  OutputString("Toolchain: ", DECORATION_YELLOW);
  OutputString(target_toolchain.GetUserVisibleName(false) + "\n");

  PrintSources(target, true);
  if (is_binary_output) {
    PrintPublic(target, true);
    PrintCheckIncludes(target, true);
    PrintAllowCircularIncludesFrom(target, true);
  }
  PrintVisibility(target, true);
  if (is_binary_output) {
    PrintTestonly(target, true);
    PrintConfigs(target, true);
  }

  PrintPublicConfigs(target, true);
  PrintAllDependentConfigs(target, true);
  PrintForwardDependentConfigsFrom(target, true);

  PrintInputs(target, true);

  if (is_binary_output) {
    OUTPUT_CONFIG_VALUE(defines, std::string)
    OUTPUT_CONFIG_VALUE(include_dirs, SourceDir)
    OUTPUT_CONFIG_VALUE(cflags, std::string)
    OUTPUT_CONFIG_VALUE(cflags_c, std::string)
    OUTPUT_CONFIG_VALUE(cflags_cc, std::string)
    OUTPUT_CONFIG_VALUE(cflags_objc, std::string)
    OUTPUT_CONFIG_VALUE(cflags_objcc, std::string)
    OUTPUT_CONFIG_VALUE(ldflags, std::string)
  }

  if (target->output_type() == Target::ACTION ||
      target->output_type() == Target::ACTION_FOREACH) {
    PrintScript(target, true);
    PrintArgs(target, true);
    PrintDepfile(target, true);
  }

  if (target->output_type() == Target::ACTION ||
      target->output_type() == Target::ACTION_FOREACH ||
      target->output_type() == Target::COPY_FILES) {
    PrintOutputs(target, true);
  }

  // Libs can be part of any target and get recursively pushed up the chain,
  // so always display them, even for groups and such.
  PrintLibs(target, true);
  PrintLibDirs(target, true);

  PrintDeps(target, true);

  return 0;
}

}  // namespace commands
