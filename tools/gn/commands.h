// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_COMMANDS_H_
#define TOOLS_GN_COMMANDS_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"

// Each "Run" command returns the value we should return from main().

namespace commands {

typedef int (*CommandRunner)(const std::vector<std::string>&);

extern const char kDeps[];
extern const char kDeps_HelpShort[];
extern const char kDeps_Help[];
int RunDeps(const std::vector<std::string>& args);

extern const char kDesc[];
extern const char kDesc_HelpShort[];
extern const char kDesc_Help[];
int RunDesc(const std::vector<std::string>& args);

extern const char kGen[];
extern const char kGen_HelpShort[];
extern const char kGen_Help[];
int RunGen(const std::vector<std::string>& args);

extern const char kHelp[];
extern const char kHelp_HelpShort[];
extern const char kHelp_Help[];
int RunHelp(const std::vector<std::string>& args);

extern const char kTree[];
extern const char kTree_HelpShort[];
extern const char kTree_Help[];
int RunTree(const std::vector<std::string>& args);

// -----------------------------------------------------------------------------

struct CommandInfo {
  CommandInfo();
  CommandInfo(const char* hs, const char* h, CommandRunner r);

  const char* help_short;
  const char* help;
  CommandRunner runner;
};

typedef std::map<base::StringPiece, CommandInfo> CommandInfoMap;

const CommandInfoMap& GetCommands();

}  // namespace commands

#endif  // TOOLS_GN_COMMANDS_H_
