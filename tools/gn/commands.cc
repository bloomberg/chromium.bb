// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/commands.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/item.h"
#include "tools/gn/label.h"
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
    INSERT_COMMAND(Help)
    INSERT_COMMAND(Refs)

    #undef INSERT_COMMAND
  }
  return info_map;
}

const Target* GetTargetForDesc(const std::vector<std::string>& args) {
  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  // TODO(brettw) bug 343726: Use a temporary directory instead of this
  // default one to avoid messing up any build that's in there.
  if (!setup->DoSetup("//out/Default/"))
    return NULL;

  // Do the actual load. This will also write out the target ninja files.
  if (!setup->Run())
    return NULL;

  // Need to resolve the label after we know the default toolchain.
  Label default_toolchain = setup->loader()->default_toolchain_label();
  Value arg_value(NULL, args[0]);
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

}  // namespace commands
