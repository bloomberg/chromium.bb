// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>

#include "base/command_line.h"
#include "base/containers/hash_tables.h"
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

// Note that this uses raw pointers. These need to be manually deleted (which
// we won't normally bother with). This allows the vector to be resized
// more quickly.
using DepStackVector = std::vector<DepStack*>;

using DepSet = base::hash_set<const Target*>;

struct Options {
  Options()
      : all(false),
        public_only(false),
        with_data(false) {
  }

  bool all;
  bool public_only;
  bool with_data;
};

struct State {
  State() : found_count(0) {
    // Reserve fairly large buffers for the found vectors.
    const size_t kReserveSize = 32768;
    found_public.reserve(kReserveSize);
    found_other.reserve(kReserveSize);
  }

  // Stores targets that do not have any paths to the destination. This is
  // an optimization to avoid revisiting useless paths.
  DepSet rejected;

  // Total number of paths found.
  int found_count;

  // The pointers in these vectors are owned by this object, but are
  // deliberately leaked. There can be a lot of them which can take a long time
  // to free, and GN will just exit after this is used anyway.
  DepStackVector found_public;
  DepStackVector found_other;
};

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

bool AreAllPublic(const DepStack& stack) {
  // Don't check the type of the last one since that doesn't point to anything.
  for (size_t i = 0; i < stack.size() - 1; i++) {
    if (stack[i].second != DEP_PUBLIC)
      return false;
  }
  return true;
}

// Increments *found_count to reflect how many results are found. If print_all
// is not set, only the first result will be printed.
//
// As an optimization, targets that do not have any paths are added to
// *reject so this function doesn't waste time revisiting them.
void RecursiveFindPath(const Options& options,
                       State* state,
                       const Target* current,
                       const Target* desired,
                       DepStack* stack) {
  if (state->rejected.find(current) != state->rejected.end())
    return;
  int initial_found_count = state->found_count;

  if (current == desired) {
    // Found a path.
    state->found_count++;
    stack->push_back(TargetDep(current, DEP_NONE));
    if (AreAllPublic(*stack))
      state->found_public.push_back(new DepStack(*stack));
    else
      state->found_other.push_back(new DepStack(*stack));
    stack->pop_back();
    return;
  }

  stack->push_back(TargetDep(current, DEP_PUBLIC));
  for (const auto& pair : current->public_deps())
    RecursiveFindPath(options, state, pair.ptr, desired, stack);

  if (!options.public_only) {
    stack->back().second = DEP_PRIVATE;
    for (const auto& pair : current->private_deps())
      RecursiveFindPath(options, state, pair.ptr, desired, stack);
  }

  if (options.with_data) {
    stack->back().second = DEP_DATA;
    for (const auto& pair : current->data_deps())
      RecursiveFindPath(options, state, pair.ptr, desired, stack);
  }

  stack->pop_back();

  if (state->found_count == initial_found_count)
    state->rejected.insert(current);  // Eliminated this target.
}

bool StackLengthLess(const DepStack* a, const DepStack* b) {
  return a->size() < b->size();
}

// Prints one result vector. The vector will be modified.
void PrintResultVector(const Options& options, DepStackVector* result) {
  if (!options.all && !result->empty()) {
    // Just print the smallest one.
    PrintDepStack(**std::min_element(result->begin(), result->end(),
                                     &StackLengthLess));
    return;
  }

  // Print all in order of increasing length.
  std::sort(result->begin(), result->end(), &StackLengthLess);
  for (const auto& stack : *result)
    PrintDepStack(*stack);
}

void PrintResults(const Options& options, State* state) {
  PrintResultVector(options, &state->found_public);

  // Consider non-public paths only if all paths are requested or there were
  // no public paths.
  if (state->found_public.empty() || options.all)
    PrintResultVector(options, &state->found_other);
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
    "  By default, a single path will be printed. If there is a path with\n"
    "  only public dependencies, the shortest public path will be printed.\n"
    "  Otherwise, the shortest path using either public or private\n"
    "  dependencies will be printed. If --with-data is specified, data deps\n"
    "  will also be considered. If there are multiple shortest paths, an\n"
    "  arbitrary one will be selected.\n"
    "\n"
    "Options\n"
    "\n"
    "  --all\n"
    "     Prints all paths found rather than just the first one. Public paths\n"
    "     will be printed first in order of increasing length, followed by\n"
    "     non-public paths in order of increasing length.\n"
    "\n"
    "  --public\n"
    "     Considers only public paths. Can't be used with --with-data.\n"
    "\n"
    "  --with-data\n"
    "     Additionally follows data deps. Without this flag, only public and\n"
    "     private linked deps will be followed. Can't be used with --public.\n"
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

  Options options;
  options.all = base::CommandLine::ForCurrentProcess()->HasSwitch("all");
  options.public_only =
      base::CommandLine::ForCurrentProcess()->HasSwitch("public");
  options.with_data =
      base::CommandLine::ForCurrentProcess()->HasSwitch("with-data");
  if (options.public_only && options.with_data) {
    Err(Location(), "Can't use --public with --with-data for 'gn path'.",
        "Your zealous over-use of arguments has inevitably resulted in an "
        "invalid\ncombination of flags.").PrintToStdout();
    return 1;
  }

  // If we don't find a path going "forwards", try the reverse direction. Deps
  // can only go in one direction without having a cycle, which will have
  // caused a run failure above.
  State state;
  DepStack stack;
  RecursiveFindPath(options, &state, target1, target2, &stack);
  if (state.found_count == 0) {
    // Need to reset the rejected set for a new invocation since the reverse
    // search will revisit the same targets looking for something else.
    state.rejected.clear();
    RecursiveFindPath(options, &state, target2, target1, &stack);
  }

  PrintResults(options, &state);

  // This string is inserted in the results to annotate whether the result
  // is only public or includes data deps or not.
  const char* path_annotation = "";
  if (options.public_only)
    path_annotation = "public ";
  else if (!options.with_data)
    path_annotation = "non-data ";

  if (state.found_count == 0) {
    // No results.
    OutputString(base::StringPrintf(
        "No %spaths found between these two targets.\n", path_annotation),
        DECORATION_YELLOW);
  } else if (state.found_count == 1) {
    // Exactly one result.
    OutputString(base::StringPrintf("1 %spath found.", path_annotation),
                 DECORATION_YELLOW);
    if (!options.public_only) {
      if (state.found_public.empty())
        OutputString(" It is not public.");
      else
        OutputString(" It is public.");
    }
    OutputString("\n");
  } else {
    if (options.all) {
      // Showing all paths when there are many.
      OutputString(base::StringPrintf("%d unique %spaths found.",
                                      state.found_count, path_annotation),
                   DECORATION_YELLOW);
      if (!options.public_only) {
        OutputString(base::StringPrintf(" %d of them are public.",
            static_cast<int>(state.found_public.size())));
      }
      OutputString("\n");
    } else {
      // Showing one path when there are many.
      OutputString(
          base::StringPrintf("Showing one of %d unique %spaths.",
                             state.found_count, path_annotation),
          DECORATION_YELLOW);
      if (!options.public_only) {
        OutputString(base::StringPrintf(" %d of them are public.\n",
            static_cast<int>(state.found_public.size())));
      }
      OutputString("Use --all to print all paths.\n");
    }
  }
  return 0;
}

}  // namespace commands
