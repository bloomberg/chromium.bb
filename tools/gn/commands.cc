// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/commands.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/item.h"
#include "tools/gn/label.h"
#include "tools/gn/label_pattern.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"

namespace commands {

CommandInfo::CommandInfo()
    : help_short(NULL),
      help(NULL),
      runner(NULL) {
}

CommandInfo::CommandInfo(const char* in_help_short,
                         const char* in_help,
                         CommandRunner in_runner)
    : help_short(in_help_short),
      help(in_help),
      runner(in_runner) {
}

const CommandInfoMap& GetCommands() {
  static CommandInfoMap info_map;
  if (info_map.empty()) {
    #define INSERT_COMMAND(cmd) \
        info_map[k##cmd] = CommandInfo(k##cmd##_HelpShort, \
                                       k##cmd##_Help, \
                                       &Run##cmd);

    INSERT_COMMAND(Args)
    INSERT_COMMAND(Check)
    INSERT_COMMAND(Desc)
    INSERT_COMMAND(Gen)
    INSERT_COMMAND(Format)
    INSERT_COMMAND(Help)
    INSERT_COMMAND(Ls)
    INSERT_COMMAND(Refs)

    #undef INSERT_COMMAND
  }
  return info_map;
}

const Target* ResolveTargetFromCommandLineString(
    Setup* setup,
    const std::string& label_string) {
  // Need to resolve the label after we know the default toolchain.
  Label default_toolchain = setup->loader()->default_toolchain_label();
  Value arg_value(NULL, label_string);
  Err err;
  Label label = Label::Resolve(SourceDirForCurrentDirectory(
                                   setup->build_settings().root_path()),
                               default_toolchain, arg_value, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return NULL;
  }

  const Item* item = setup->builder()->GetItem(label);
  if (!item) {
    Err(Location(), "Label not found.",
        label.GetUserVisibleName(false) + " not found.").PrintToStdout();
    return NULL;
  }

  const Target* target = item->AsTarget();
  if (!target) {
    Err(Location(), "Not a target.",
        "The \"" + label.GetUserVisibleName(false) + "\" thing\n"
        "is not a target. Somebody should probably implement this command for "
        "other\nitem types.");
    return NULL;
  }

  return target;
}

bool ResolveTargetsFromCommandLinePattern(
    Setup* setup,
    const std::string& label_pattern,
    bool all_toolchains,
    std::vector<const Target*>* matches) {
  Value pattern_value(NULL, label_pattern);

  Err err;
  LabelPattern pattern = LabelPattern::GetPattern(
      SourceDirForCurrentDirectory(setup->build_settings().root_path()),
      pattern_value,
      &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  if (!all_toolchains) {
    // By default a pattern with an empty toolchain will match all toolchains.
    // IF the caller wants to default to the main toolchain only, set it
    // explicitly.
    if (pattern.toolchain().is_null()) {
      // No explicit toolchain set.
      pattern.set_toolchain(setup->loader()->default_toolchain_label());
    }
  }

  std::vector<const Target*> all_targets =
      setup->builder()->GetAllResolvedTargets();

  for (size_t i = 0; i < all_targets.size(); i++) {
    if (pattern.Matches(all_targets[i]->label()))
      matches->push_back(all_targets[i]);
  }
  return true;
}

}  // namespace commands
