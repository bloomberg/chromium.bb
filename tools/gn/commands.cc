// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/commands.h"

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

    INSERT_COMMAND(Deps)
    INSERT_COMMAND(Desc)
    INSERT_COMMAND(Gen)
    INSERT_COMMAND(Help)
    INSERT_COMMAND(Tree)

    #undef INSERT_COMMAND
  }
  return info_map;
}

}  // namespace commands
