// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "tools/gn/commands.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"

namespace commands {

namespace {

enum DepType {
  DEP_NONE,
  DEP_PUBLIC,
  DEP_PRIVATE,
  DEP_DATA
};

// As we do a depth-first search, this vector will store the current path
// the current target for printing when a match is found.
using TargetDep = std::pair<const Target*, DepType>;
using DepStack = std::vector<TargetDep>;

void PrintDepStack(const DepStack& stack) {
  // Don't print toolchains unless they differ from the first target.
  const Label& default_toolchain = stack[0].first->label().GetToolchainLabel();

  for (const auto& pair : stack) {
    OutputString(pair.first->label().GetUserVisibleName(default_toolchain));
    switch (pair.second) {
      case DEP_NONE:
        break;
      case DEP_PUBLIC:
        OutputString(" --[public]-->", DECORATION_DIM);
        break;
      case DEP_PRIVATE:
        OutputString(" --[private]-->", DECORATION_DIM);
        break;
      case DEP_DATA:
        OutputString(" --[data]-->", DECORATION_DIM);
        break;
    }
    OutputString("\n");
  }
  OutputString("\n");
}

// Increments *found_count to reflect how many results are found. If print_all
// is not set, only the first result will be printed.
void RecursiveFindPath(const Target* current,
                       const Target* desired,
                       DepStack* stack,
                       int* found_count,
                       bool print_all) {
  if (current == desired) {
    (*found_count)++;
    if (print_all || *found_count == 1) {
      stack->push_back(TargetDep(current, DEP_NONE));
      PrintDepStack(*stack);
      stack->pop_back();
    }
    return;
  }

  stack->push_back(TargetDep(current, DEP_PUBLIC));
  for (const auto& pair : current->public_deps())
    RecursiveFindPath(pair.ptr, desired, stack, found_count, print_all);

  stack->back().second = DEP_PRIVATE;
  for (const auto& pair : current->private_deps())
    RecursiveFindPath(pair.ptr, desired, stack, found_count, print_all);

  stack->back().second = DEP_DATA;
  for (const auto& pair : current->data_deps())
    RecursiveFindPath(pair.ptr, desired, stack, found_count, print_all);
  stack->pop_back();
}

}  // namespace

const char kPath[] = "path";
const char kPath_HelpShort[] =
    "path: Find paths between two targets.";
const char kPath_Help[] =
    "gn path <out_dir> <target_one> <target_two>\n"
    "\n"
    "  Finds paths of dependencies between two targets. Each unique path\n"
    "  will be printed in one group, and groups will be separate by newlines.\n"
    "  The two targets can appear in either order: paths will be found going\n"
    "  in either direction.\n"
    "\n"
    "  Each dependency will be annotated with its type. By default, only the\n"
    "  first path encountered will be printed, which is not necessarily the\n"
    "  shortest path.\n"
    "\n"
    "Options\n"
    "\n"
    "  --all\n"
    "     Prints all paths found rather than just the first one.\n"
    "\n"
    "Example\n"
    "\n"
    "  gn path out/Default //base //tools/gn\n";

int RunPath(const std::vector<std::string>& args) {
  if (args.size() != 3) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn path <out_dir> <target_one> <target_two>\"")
        .PrintToStdout();
    return 1;
  }

  Setup* setup = new Setup;
  if (!setup->DoSetup(args[0], false))
    return 1;
  if (!setup->Run())
    return 1;

  const Target* target1 = ResolveTargetFromCommandLineString(setup, args[1]);
  if (!target1)
    return 1;
  const Target* target2 = ResolveTargetFromCommandLineString(setup, args[2]);
  if (!target2)
    return 1;

  bool print_all = base::CommandLine::ForCurrentProcess()->HasSwitch("all");

  // If we don't find a path going "forwards", try the reverse direction. Deps
  // can only go in one direction without having a cycle, which will have
  // caused a run failure above.
  DepStack stack;
  int found = 0;
  RecursiveFindPath(target1, target2, &stack, &found, print_all);
  if (found == 0)
    RecursiveFindPath(target2, target1, &stack, &found, print_all);

  if (found == 0) {
    OutputString("No paths found between these two targets.\n",
                 DECORATION_YELLOW);
  } else if (found == 1) {
    OutputString("1 path found.\n", DECORATION_YELLOW);
  } else {
    if (print_all) {
      OutputString(base::StringPrintf("%d unique paths found.\n", found),
                   DECORATION_YELLOW);
    } else {
      OutputString(
          base::StringPrintf("Showing the first of %d unique paths. ", found),
          DECORATION_YELLOW);
      OutputString("Use --all to print all paths.\n");
    }
  }
  return 0;
}

}  // namespace commands
