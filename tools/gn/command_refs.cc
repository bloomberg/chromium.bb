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

// Returns the file path generating this item.
base::FilePath FilePathForItem(const Item* item) {
  return item->defined_from()->GetRange().begin().file()->physical_name();
}

// Prints the targets which are the result of a query. This list is sorted
// and, if as_files is set, the unique filenames matching those targets will
// be used.
void OutputResultSet(const TargetSet& results, bool as_files) {
  if (results.empty())
    return;

  if (as_files) {
    // Output the set of unique source files.
    std::set<std::string> unique_files;
    for (const auto& cur : results)
      unique_files.insert(FilePathToUTF8(FilePathForItem(cur)));

    for (const auto& cur : unique_files)
      OutputString(cur + "\n");
  } else {
    // Output sorted and uniquified list of labels. The set will sort the
    // labels.
    std::set<Label> unique_labels;
    for (const auto& cur : results)
      unique_labels.insert(cur->label());

    // Grab the label of the default toolchain from a random target.
    Label default_tc_label =
        (*results.begin())->settings()->default_toolchain_label();

    for (const auto& cur : unique_labels) {
      // Print toolchain only for ones not in the default toolchain.
      OutputString(cur.GetUserVisibleName(
          cur.GetToolchainLabel() != default_tc_label));
      OutputString("\n");
    }
  }
}

// Prints refs of the given target (not the target itself). If the set is
// non-null, new targets encountered will be added to the set, and if a ref is
// in the set already, it will not be recused into. When the set is null, all
// refs will be printed.
void RecursivePrintTree(const DepMap& dep_map,
                        const Target* target,
                        TargetSet* seen_targets,
                        int indent_level) {
  std::string indent(indent_level * 2, ' ');

  DepMap::const_iterator dep_begin = dep_map.lower_bound(target);
  DepMap::const_iterator dep_end = dep_map.upper_bound(target);
  for (DepMap::const_iterator cur_dep = dep_begin;
       cur_dep != dep_end; cur_dep++) {
    const Target* cur_target = cur_dep->second;

    // Only print the toolchain for non-default-toolchain targets.
    OutputString(indent + cur_target->label().GetUserVisibleName(
        !cur_target->settings()->is_default()));

    bool print_children = true;
    if (seen_targets) {
      if (seen_targets->find(cur_target) == seen_targets->end()) {
        // New target, mark it visited.
        seen_targets->insert(cur_target);
      } else {
        // Already seen.
        print_children = false;
        // Only print "..." if something is actually elided, which means that
        // the current target has children.
        if (dep_map.lower_bound(cur_target) != dep_map.upper_bound(cur_target))
          OutputString("...");
      }
    }

    OutputString("\n");
    if (print_children)
      RecursivePrintTree(dep_map, cur_target, seen_targets, indent_level + 1);
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

}  // namespace

const char kRefs[] = "refs";
const char kRefs_HelpShort[] =
    "refs: Find stuff referencing a target, directory, or config.";
const char kRefs_Help[] =
    "gn refs <build_dir> <label_pattern> [--files] [--tree] [--all]\n"
    "        [--all-toolchains]\n"
    "\n"
    "  Finds which targets reference a given target or targets (reverse\n"
    "  dependencies).\n"
    "\n"
    "  The <label_pattern> can take exact labels or patterns that match more\n"
    "  than one (although not general regular expressions).\n"
    "  See \"gn help label_pattern\" for details.\n"
    "\n"
    "  --all\n"
    "      When used without --tree, will recurse and display all unique\n"
    "      dependencies of the given targets. When used with --tree, turns\n"
    "      off eliding to show a complete tree.\n"
    "\n"
    "  --all-toolchains\n"
    "      Make the label pattern matche all toolchains. If the label pattern\n"
    "      does not specify an explicit toolchain, labels from all toolchains\n"
    "      will be matched (normally only the default toolchain is matched\n"
    "      when no toolchain is specified).\n"
    "\n"
    "  --files\n"
    "      Output unique filenames referencing a matched target or config.\n"
    "      These will be relative to the source root directory such that they\n"
    "      are suitable for piping to other commands.\n"
    "\n"
    "  --tree\n"
    "      Outputs a reverse dependency tree from the given target. The label\n"
    "      pattern must match one target exactly. Duplicates will be elided.\n"
    "      Combine with --all to see a full dependency tree.\n"
    "\n"
    "Examples\n"
    "\n"
    "  gn refs out/Debug //tools/gn:gn\n"
    "      Find all targets depending on the given exact target name.\n"
    "\n"
    "  gn refs out/Debug //base:i18n --files | xargs gvim\n"
    "      Edit all files containing references to //base:i18n\n"
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
    "      Print a reverse dependency tree of //base:base\n";

int RunRefs(const std::vector<std::string>& args) {
  if (args.size() != 2) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn refs <build_dir> <label_pattern>\"").PrintToStdout();
    return 1;
  }

  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  bool tree = cmdline->HasSwitch("tree");
  bool all = cmdline->HasSwitch("all");
  bool all_toolchains = cmdline->HasSwitch("all-toolchains");
  bool files = cmdline->HasSwitch("files");

  Setup* setup = new Setup;
  setup->set_check_for_bad_items(false);
  if (!setup->DoSetup(args[0], false) || !setup->Run())
    return 1;

  // Figure out the target or targets that the user is querying.
  std::vector<const Target*> query;
  if (!ResolveTargetsFromCommandLinePattern(setup, args[1], all_toolchains,
                                            &query))
    return 1;
  if (query.empty()) {
    OutputString("\"" + args[1] + "\" matches no targets.\n");
    return 0;
  }

  // Construct the reverse dependency tree.
  DepMap dep_map;
  FillDepMap(setup, &dep_map);

  if (tree) {
    // Output dependency tree.
    if (files) {
      Err(NULL, "--files option can't be used with --tree option.")
          .PrintToStdout();
      return 1;
    }
    if (query.size() != 1) {
      Err(NULL, "Query matches more than one target.",
          "--tree only supports a single target as input.").PrintToStdout();
      return 1;
    }
    if (all) {
      // Recursively print all targets.
      RecursivePrintTree(dep_map, query[0], NULL, 0);
    } else {
      // Recursively print unique targets.
      TargetSet seen_targets;
      RecursivePrintTree(dep_map, query[0], &seen_targets, 0);
    }
  } else if (all) {
    // Output recursive dependencies, uniquified and flattened.
    TargetSet results;
    for (const auto& cur_query : query)
      RecursiveCollectChildRefs(dep_map, cur_query, &results);
    OutputResultSet(results, files);
  } else {
    // Output direct references of everything in the query.
    TargetSet results;
    for (const auto& cur_query : query) {
      DepMap::const_iterator dep_begin = dep_map.lower_bound(cur_query);
      DepMap::const_iterator dep_end = dep_map.upper_bound(cur_query);
      for (DepMap::const_iterator cur_dep = dep_begin;
           cur_dep != dep_end; cur_dep++)
        results.insert(cur_dep->second);
    }
    OutputResultSet(results, files);
  }

  return 0;
}

}  // namespace commands
