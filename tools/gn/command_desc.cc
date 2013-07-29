// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <set>

#include "tools/gn/commands.h"
#include "tools/gn/config.h"
#include "tools/gn/item.h"
#include "tools/gn/item_node.h"
#include "tools/gn/label.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"

namespace {

struct CompareTargetLabel {
  bool operator()(const Target* a, const Target* b) const {
    return a->label() < b->label();
  }
};

const Target* GetTargetForDesc(const std::vector<std::string>& args) {
  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  if (!setup->DoSetup())
    return NULL;

  // FIXME(brettw): set the output dir to be a sandbox one to avoid polluting
  // the real output dir with files written by the build scripts.

  // Do the actual load. This will also write out the target ninja files.
  if (!setup->Run())
    return NULL;

  // Need to resolve the label after we know the default toolchain.
  // TODO(brettw) find the current directory and resolve the input label
  // relative to that.
  Label default_toolchain = setup->build_settings().toolchain_manager()
      .GetDefaultToolchainUnlocked();
  Value arg_value(NULL, args[0]);
  Err err;
  Label label = Label::Resolve(SourceDir(), default_toolchain, arg_value, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return NULL;
  }

  ItemNode* node;
  {
    base::AutoLock lock(setup->build_settings().item_tree().lock());
    node = setup->build_settings().item_tree().GetExistingNodeLocked(label);
  }
  if (!node) {
    Err(Location(), "",
        "I don't know about this \"" + label.GetUserVisibleName(false) +
        "\"").PrintToStdout();
    return NULL;
  }

  const Target* target = node->item()->AsTarget();
  if (!target) {
    Err(Location(), "Not a target.",
        "The \"" + label.GetUserVisibleName(false) + "\" thing\n"
        "is not a target. Somebody should probably implement this command for "
        "other\nitem types.");
    return NULL;
  }

  return target;
}

void RecursiveCollectDeps(const Target* target, std::set<Label>* result) {
  if (result->find(target->label()) != result->end())
    return;  // Already did this target.
  result->insert(target->label());

  const std::vector<const Target*>& deps = target->deps();
  for (size_t i = 0; i < deps.size(); i++)
    RecursiveCollectDeps(deps[i], result);
}

// Prints dependencies of the given target (not the target itself).
void RecursivePrintDeps(const Target* target,
                        const Label& default_toolchain,
                        int indent_level) {
  std::vector<const Target*> sorted_deps = target->deps();
  std::sort(sorted_deps.begin(), sorted_deps.end(), CompareTargetLabel());

  std::string indent(indent_level * 2, ' ');
  for (size_t i = 0; i < sorted_deps.size(); i++) {
    OutputString(indent +
        sorted_deps[i]->label().GetUserVisibleName(default_toolchain) + "\n");
    RecursivePrintDeps(sorted_deps[i], default_toolchain, indent_level + 1);
  }
}

}  // namespace

int RunDescCommand(const std::vector<std::string>& args) {
  if (args.size() != 1) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn desc <target_name>\"").PrintToStdout();
    return NULL;
  }

  const Target* target = GetTargetForDesc(args);
  if (!target)
    return 1;

  // Generally we only want to display toolchains on labels when the toolchain
  // is different than the default one for this target (which we always print
  // in the header).
  Label target_toolchain = target->label().GetToolchainLabel();

  // Header.
  std::string title_target =
      "Target: " + target->label().GetUserVisibleName(false);
  std::string title_toolchain =
      "Toolchain: " + target_toolchain.GetUserVisibleName(false);
  OutputString(title_target + "\n", DECORATION_YELLOW);
  OutputString(title_toolchain + "\n", DECORATION_YELLOW);
  OutputString(std::string(
      std::max(title_target.size(), title_toolchain.size()), '=') + "\n");

  OutputString("Sources:\n");
  const Target::FileList& sources = target->sources();
  for (size_t i = 0; i < sources.size(); i++)
    OutputString("  " + sources[i].value() + "\n");

  // Configs (don't sort since the order determines how things are processed).
  OutputString("\nConfigs:\n");
  const std::vector<const Config*>& configs = target->configs();
  for (size_t i = 0; i < configs.size(); i++) {
    OutputString("  " +
        configs[i]->label().GetUserVisibleName(target_toolchain) + "\n");
  }

  // Deps. Sorted for convenience. Sort the labels rather than the strings so
  // that "//foo:bar" comes before "//foo/third_party:bar".
  OutputString("\nDirect dependencies:\n"
               "(Use \"gn deps\" or \"gn tree\" to display recursive deps.)\n");
  const std::vector<const Target*>& deps = target->deps();
  std::vector<Label> sorted_deps;
  for (size_t i = 0; i < deps.size(); i++)
    sorted_deps.push_back(deps[i]->label());
  std::sort(sorted_deps.begin(), sorted_deps.end());
  for (size_t i = 0; i < sorted_deps.size(); i++) {
    OutputString("  " + sorted_deps[i].GetUserVisibleName(target_toolchain) +
                 "\n");
  }
  return 0;
}

int RunDepsCommand(const std::vector<std::string>& args) {
  if (args.size() != 1) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn deps <target_name>\"").PrintToStdout();
    return NULL;
  }

  const Target* target = GetTargetForDesc(args);
  if (!target)
    return 1;

  // Generally we only want to display toolchains on labels when the toolchain
  // is different than the default one for this target (which we always print
  // in the header).
  Label target_toolchain = target->label().GetToolchainLabel();

  std::set<Label> all_deps;
  RecursiveCollectDeps(target, &all_deps);

  OutputString("Recursive dependencies of " +
               target->label().GetUserVisibleName(true) + "\n",
               DECORATION_YELLOW);

  for (std::set<Label>::iterator i = all_deps.begin();
       i != all_deps.end(); ++i)
    OutputString("  " + i->GetUserVisibleName(target_toolchain) + "\n");
  return 0;
}

int RunTreeCommand(const std::vector<std::string>& args) {
  if (args.size() != 1) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn tree <target_name>\"").PrintToStdout();
    return NULL;
  }

  const Target* target = GetTargetForDesc(args);
  if (!target)
    return 1;

  OutputString(target->label().GetUserVisibleName(false) + "\n");
  RecursivePrintDeps(target, target->label().GetToolchainLabel(), 1);

  return 0;
}
