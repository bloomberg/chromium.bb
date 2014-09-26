// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_COMMANDS_H_
#define TOOLS_GN_COMMANDS_H_

#include <map>
#include <string>
#include <vector>

#include "base/strings/string_piece.h"

class BuildSettings;
class Setup;
class Target;

// Each "Run" command returns the value we should return from main().

namespace commands {

typedef int (*CommandRunner)(const std::vector<std::string>&);

extern const char kArgs[];
extern const char kArgs_HelpShort[];
extern const char kArgs_Help[];
int RunArgs(const std::vector<std::string>& args);

extern const char kCheck[];
extern const char kCheck_HelpShort[];
extern const char kCheck_Help[];
int RunCheck(const std::vector<std::string>& args);

extern const char kDesc[];
extern const char kDesc_HelpShort[];
extern const char kDesc_Help[];
int RunDesc(const std::vector<std::string>& args);

extern const char kGen[];
extern const char kGen_HelpShort[];
extern const char kGen_Help[];
int RunGen(const std::vector<std::string>& args);

extern const char kFormat[];
extern const char kFormat_HelpShort[];
extern const char kFormat_Help[];
int RunFormat(const std::vector<std::string>& args);

extern const char kHelp[];
extern const char kHelp_HelpShort[];
extern const char kHelp_Help[];
int RunHelp(const std::vector<std::string>& args);

extern const char kLs[];
extern const char kLs_HelpShort[];
extern const char kLs_Help[];
int RunLs(const std::vector<std::string>& args);

extern const char kRefs[];
extern const char kRefs_HelpShort[];
extern const char kRefs_Help[];
int RunRefs(const std::vector<std::string>& args);

// -----------------------------------------------------------------------------

struct CommandInfo {
  CommandInfo();
  CommandInfo(const char* in_help_short,
              const char* in_help,
              CommandRunner in_runner);

  const char* help_short;
  const char* help;
  CommandRunner runner;
};

typedef std::map<base::StringPiece, CommandInfo> CommandInfoMap;

const CommandInfoMap& GetCommands();

// Helper functions for some commands ------------------------------------------

// Given a setup that has already been run and some command-line input,
// resolves that input as a target label and returns the corresponding target.
// On failure, returns null and prints the error to the standard output.
const Target* ResolveTargetFromCommandLineString(
    Setup* setup,
    const std::string& label_string);

// Like above but the input string can be a pattern that matches multiple
// targets. If the input does not parse as a pattern, prints and error and
// returns false. If the pattern is valid, fills the vector (which might be
// empty if there are no matches) and returns true.
//
// If all_tolchains is false, a pattern with an unspecified toolchain will
// match the default toolchain only. If true, all toolchains will be matched.
bool ResolveTargetsFromCommandLinePattern(
    Setup* setup,
    const std::string& label_pattern,
    bool all_toolchains,
    std::vector<const Target*>* matches);

// Runs the header checker. All targets in the build should be given in
// all_targets, and the specific targets to check should be in to_check. If
// to_check is empty, all targets will be checked.
//
// force_check, if true, will override targets opting out of header checking
// with "check_includes = false" and will check them anyway.
//
// On success, returns true. If the check fails, the error(s) will be printed
// to stdout and false will be returned.
bool CheckPublicHeaders(const BuildSettings* build_settings,
                        const std::vector<const Target*>& all_targets,
                        const std::vector<const Target*>& to_check,
                        bool force_check);

}  // namespace commands

#endif  // TOOLS_GN_COMMANDS_H_
