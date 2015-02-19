// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <set>

#include "base/command_line.h"
#include "tools/gn/commands.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file.h"
#include "tools/gn/item.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"

namespace commands {

namespace {

typedef std::set<const Target*> TargetSet;
typedef std::vector<const Target*> TargetVector;

// Maps targets to the list of targets that depend on them.
typedef std::multimap<const Target*, const Target*> DepMap;

// Populates the reverse dependency map for the targets in the Setup.
void FillDepMap(Setup* setup, DepMap* dep_map) {
  for (const auto& target : setup->builder()->GetAllResolvedTargets()) {
    for (const auto& dep_pair : target->GetDeps(Target::DEPS_ALL))
      dep_map->insert(std::make_pair(dep_pair.ptr, target));
  }
}

// Forward declaration for function below.
void RecursivePrintTargetDeps(const DepMap& dep_map,
                              const Target* target,
                              TargetSet* seen_targets,
                              int indent_level);

// Prints the target and its dependencies in tree form. If the set is non-null,
// new targets encountered will be added to the set, and if a ref is in the set
// already, it will not be recused into. When the set is null, all refs will be
// printed.
void RecursivePrintTarget(const DepMap& dep_map,
                          const Target* target,
                          TargetSet* seen_targets,
                          int indent_level) {
  std::string indent(indent_level * 2, ' ');

  // Only print the toolchain for non-default-toolchain targets.
  OutputString(indent + target->label().GetUserVisibleName(
      !target->settings()->is_default()));

  bool print_children = true;
  if (seen_targets) {
    if (seen_targets->find(target) == seen_targets->end()) {
      // New target, mark it visited.
      seen_targets->insert(target);
    } else {
      // Already seen.
      print_children = false;
      // Only print "..." if something is actually elided, which means that
      // the current target has children.
      if (dep_map.lower_bound(target) != dep_map.upper_bound(target))
        OutputString("...");
    }
  }

  OutputString("\n");
  if (print_children)
    RecursivePrintTargetDeps(dep_map, target, seen_targets, indent_level + 1);
}

// Prints refs of the given target (not the target itself). See
// RecursivePrintTarget.
void RecursivePrintTargetDeps(const DepMap& dep_map,
                              const Target* target,
                              TargetSet* seen_targets,
                              int indent_level) {
  DepMap::const_iterator dep_begin = dep_map.lower_bound(target);
  DepMap::const_iterator dep_end = dep_map.upper_bound(target);
  for (DepMap::const_iterator cur_dep = dep_begin;
       cur_dep != dep_end; cur_dep++) {
    RecursivePrintTarget(dep_map, cur_dep->second, seen_targets, indent_level);
  }
}

void RecursiveCollectChildRefs(const DepMap& dep_map,
                               const Target* target,
                               TargetSet* results);

// Recursively finds all targets that reference the given one, and additionally
// adds the current one to the list.
void RecursiveCollectRefs(const DepMap& dep_map,
                          const Target* target,
                          TargetSet* results) {
  if (results->find(target) != results->end())
    return;  // Already found this target.
  results->insert(target);
  RecursiveCollectChildRefs(dep_map, target, results);
}

// Recursively finds all targets that reference the given one.
void RecursiveCollectChildRefs(const DepMap& dep_map,
                               const Target* target,
                               TargetSet* results) {
  DepMap::const_iterator dep_begin = dep_map.lower_bound(target);
  DepMap::const_iterator dep_end = dep_map.upper_bound(target);
  for (DepMap::const_iterator cur_dep = dep_begin;
       cur_dep != dep_end; cur_dep++)
    RecursiveCollectRefs(dep_map, cur_dep->second, results);
}

bool TargetContainsFile(const Target* target, const SourceFile& file) {
  for (const auto& cur_file : target->sources()) {
    if (cur_file == file)
      return true;
  }
  for (const auto& cur_file : target->public_headers()) {
    if (cur_file == file)
      return true;
  }
  for (const auto& cur_file : target->inputs()) {
    if (cur_file == file)
      return true;
  }
  for (const auto& cur_file : target->data()) {
    if (cur_file == file)
      return true;
  }
  return false;
}

void GetTargetsContainingFile(Setup* setup,
                              const std::vector<const Target*>& all_targets,
                              const SourceFile& file,
                              bool all_toolchains,
                              UniqueVector<const Target*>* matches) {
  Label default_toolchain = setup->loader()->default_toolchain_label();
  for (const auto& target : all_targets) {
    if (!all_toolchains) {
      // Only check targets in the default toolchain.
      if (target->label().GetToolchainLabel() != default_toolchain)
        continue;
    }
    if (TargetContainsFile(target, file))
      matches->push_back(target);
  }
}

bool TargetReferencesConfig(const Target* target, const Config* config) {
  for (const LabelConfigPair& cur : target->configs()) {
    if (cur.ptr == config)
      return true;
  }
  for (const LabelConfigPair& cur : target->public_configs()) {
    if (cur.ptr == config)
      return true;
  }
  return false;
}

void GetTargetsReferencingConfig(Setup* setup,
                                 const std::vector<const Target*>& all_targets,
                                 const Config* config,
                                 bool all_toolchains,
                                 UniqueVector<const Target*>* matches) {
  Label default_toolchain = setup->loader()->default_toolchain_label();
  for (const auto& target : all_targets) {
    if (!all_toolchains) {
      // Only check targets in the default toolchain.
      if (target->label().GetToolchainLabel() != default_toolchain)
        continue;
    }
    if (TargetReferencesConfig(target, config))
      matches->push_back(target);
  }
}

void DoTreeOutput(const DepMap& dep_map,
                  const UniqueVector<const Target*>& implicit_target_matches,
                  const UniqueVector<const Target*>& explicit_target_matches,
                  bool all) {
  TargetSet seen_targets;

  // Implicit targets don't get printed themselves.
  for (const Target* target : implicit_target_matches) {
    if (all)
      RecursivePrintTargetDeps(dep_map, target, nullptr, 0);
    else
      RecursivePrintTargetDeps(dep_map, target, &seen_targets, 0);
  }

  // Explicit targets appear in the output.
  for (const Target* target : implicit_target_matches) {
    if (all)
      RecursivePrintTarget(dep_map, target, nullptr, 0);
    else
      RecursivePrintTarget(dep_map, target, &seen_targets, 0);
  }
}

void DoAllListOutput(
    const DepMap& dep_map,
    const UniqueVector<const Target*>& implicit_target_matches,
    const UniqueVector<const Target*>& explicit_target_matches) {
  // Output recursive dependencies, uniquified and flattened.
  TargetSet results;

  for (const Target* target : implicit_target_matches)
    RecursiveCollectChildRefs(dep_map, target, &results);
  for (const Target* target : explicit_target_matches) {
    // Explicit targets also get added to the output themselves.
    results.insert(target);
    RecursiveCollectChildRefs(dep_map, target, &results);
  }

  FilterAndPrintTargetSet(false, results);
}

void DoDirectListOutput(
    const DepMap& dep_map,
    const UniqueVector<const Target*>& implicit_target_matches,
    const UniqueVector<const Target*>& explicit_target_matches) {
  TargetSet results;

  // Output everything that refers to the implicit ones.
  for (const Target* target : implicit_target_matches) {
    DepMap::const_iterator dep_begin = dep_map.lower_bound(target);
    DepMap::const_iterator dep_end = dep_map.upper_bound(target);
    for (DepMap::const_iterator cur_dep = dep_begin;
         cur_dep != dep_end; cur_dep++)
      results.insert(cur_dep->second);
  }

  // And just output the explicit ones directly (these are the target matches
  // when referring to what references a file or config).
  for (const Target* target : explicit_target_matches)
    results.insert(target);

  FilterAndPrintTargetSet(false, results);
}

}  // namespace

const char kRefs[] = "refs";
const char kRefs_HelpShort[] =
    "refs: Find stuff referencing a target or file.";
const char kRefs_Help[] =
    "gn refs <out_dir> (<label_pattern>|<label>|<file>)* [--all]\n"
    "        [--all-toolchains] [--as=...] [--testonly=...] [--type=...]\n"
    "\n"
    "  Finds reverse dependencies (which targets reference something). The\n"
    "  input is a list containing:\n"
    "\n"
    "   - Target label: The result will be which targets depend on it.\n"
    "\n"
    "   - Config label: The result will be which targets list the given\n"
    "     config in its \"configs\" or \"public_configs\" list.\n"
    "\n"
    "   - Label pattern: The result will be which targets depend on any\n"
    "     target matching the given pattern. Patterns will not match\n"
    "     configs. These are not general regular expressions, see\n"
    "     \"gn help label_pattern\" for details.\n"
    "\n"
    "   - File name: The result will be which targets list the given file in\n"
    "     its \"inputs\", \"sources\", \"public\", or \"data\". Any input\n"
    "     that does not contain wildcards and does not match a target or a\n"
    "     config will be treated as a file.\n"
    "\n"
    "Options\n"
    "\n"
    "  --all\n"
    "      When used without --tree, will recurse and display all unique\n"
    "      dependencies of the given targets. For example, if the input is\n"
    "      a target, this will output all targets that depend directly or\n"
    "      indirectly on the input. If the input is a file, this will output\n"
    "      all targets that depend directly or indirectly on that file.\n"
    "\n"
    "      When used with --tree, turns off eliding to show a complete tree.\n"
    "\n"
    "  --all-toolchains\n"
    "      Normally only inputs in the default toolchain will be included.\n"
    "      This switch will turn on matching all toolchains.\n"
    "\n"
    "      For example, a file is in a target might be compiled twice:\n"
    "      once in the default toolchain and once in a secondary one. Without\n"
    "      this flag, only the default toolchain one will be matched and\n"
    "      printed (potentially with its recursive dependencies, depending on\n"
    "      the other options). With this flag, both will be printed\n"
    "      (potentially with both of their recursive dependencies).\n"
    "\n"
    TARGET_PRINTING_MODE_COMMAND_LINE_HELP
    "\n"
    TARGET_TESTONLY_FILTER_COMMAND_LINE_HELP
    "\n"
    "  --tree\n"
    "      Outputs a reverse dependency tree from the given target.\n"
    "      Duplicates will be elided. Combine with --all to see a full\n"
    "      dependency tree.\n"
    "\n"
    "      Tree output can not be used with the filtering or output flags:\n"
    "      --as, --type, --testonly.\n"
    "\n"
    TARGET_TYPE_FILTER_COMMAND_LINE_HELP
    "\n"
    "Examples (target input)\n"
    "\n"
    "  gn refs out/Debug //tools/gn:gn\n"
    "      Find all targets depending on the given exact target name.\n"
    "\n"
    "  gn refs out/Debug //base:i18n --as=buildfiles | xargs gvim\n"
    "      Edit all .gn files containing references to //base:i18n\n"
    "\n"
    "  gn refs out/Debug //base --all\n"
    "      List all targets depending directly or indirectly on //base:base.\n"
    "\n"
    "  gn refs out/Debug \"//base/*\"\n"
    "      List all targets depending directly on any target in //base or\n"
    "      its subdirectories.\n"
    "\n"
    "  gn refs out/Debug \"//base:*\"\n"
    "      List all targets depending directly on any target in\n"
    "      //base/BUILD.gn.\n"
    "\n"
    "  gn refs out/Debug //base --tree\n"
    "      Print a reverse dependency tree of //base:base\n"
    "\n"
    "Examples (file input)\n"
    "\n"
    "  gn refs out/Debug //base/macros.h\n"
    "      Print target(s) listing //base/macros.h as a source.\n"
    "\n"
    "  gn refs out/Debug //base/macros.h --tree\n"
    "      Display a reverse dependency tree to get to the given file. This\n"
    "      will show how dependencies will reference that file.\n"
    "\n"
    "  gn refs out/Debug //base/macros.h //base/basictypes.h --all\n"
    "      Display all unique targets with some dependency path to a target\n"
    "      containing either of the given files as a source.\n"
    "\n"
    "  gn refs out/Debug //base/macros.h --testonly=true --type=executable\n"
    "          --all --as=output\n"
    "      Display the executable file names of all test executables\n"
    "      potentially affected by a change to the given file.\n";

int RunRefs(const std::vector<std::string>& args) {
  if (args.size() != 2) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn refs <out_dir> (<label_pattern>|<file>)\"")
        .PrintToStdout();
    return 1;
  }

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  bool tree = cmdline->HasSwitch("tree");
  bool all = cmdline->HasSwitch("all");
  bool all_toolchains = cmdline->HasSwitch("all-toolchains");

  Setup* setup = new Setup;
  setup->set_check_for_bad_items(false);
  if (!setup->DoSetup(args[0], false) || !setup->Run())
    return 1;

  // The inputs are everything but the first arg (which is the build dir).
  std::vector<std::string> inputs(args.begin() + 1, args.end());

  // Get the matches for the command-line input.
  UniqueVector<const Target*> target_matches;
  UniqueVector<const Config*> config_matches;
  UniqueVector<const Toolchain*> toolchain_matches;
  UniqueVector<SourceFile> file_matches;
  if (!ResolveFromCommandLineInput(setup, inputs, all_toolchains,
                                   &target_matches, &config_matches,
                                   &toolchain_matches, &file_matches))
    return 1;

  // When you give a file or config as an input, you want the targets that are
  // associated with it. We don't want to just append this to the
  // target_matches, however, since these targets should actually be listed in
  // the output, while for normal targets you don't want to see the inputs,
  // only what refers to them.
  std::vector<const Target*> all_targets =
      setup->builder()->GetAllResolvedTargets();
  UniqueVector<const Target*> explicit_target_matches;
  for (const auto& file : file_matches) {
    GetTargetsContainingFile(setup, all_targets, file, all_toolchains,
                             &explicit_target_matches);
  }
  for (const auto& config : config_matches) {
    GetTargetsReferencingConfig(setup, all_targets, config, all_toolchains,
                                &explicit_target_matches);
  }

  // Construct the reverse dependency tree.
  DepMap dep_map;
  FillDepMap(setup, &dep_map);

  if (tree)
    DoTreeOutput(dep_map, target_matches, explicit_target_matches, all);
  else if (all)
    DoAllListOutput(dep_map, target_matches, explicit_target_matches);
  else
    DoDirectListOutput(dep_map, target_matches, explicit_target_matches);

  return 0;
}

}  // namespace commands
