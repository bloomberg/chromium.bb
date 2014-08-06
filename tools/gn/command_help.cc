// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>

#include "tools/gn/args.h"
#include "tools/gn/commands.h"
#include "tools/gn/err.h"
#include "tools/gn/functions.h"
#include "tools/gn/input_conversion.h"
#include "tools/gn/pattern.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/variables.h"

namespace commands {

namespace {

void PrintToplevelHelp() {
  OutputString("Commands (type \"gn help <command>\" for more details):\n");

  const commands::CommandInfoMap& command_map = commands::GetCommands();
  for (commands::CommandInfoMap::const_iterator i = command_map.begin();
       i != command_map.end(); ++i)
    PrintShortHelp(i->second.help_short);

  OutputString(
      "\n"
      "Common switches:\n");
  PrintShortHelp(
      "--args: Specifies build arguments overrides. "
      "See \"gn help buildargs\".");
  PrintShortHelp(
      "--[no]color: Forces colored output on or off (rather than autodetect).");
  PrintShortHelp(
      "--dotfile: Specifies an alternate .gn file. See \"gn help dotfile\".");
  PrintShortHelp(
      "--no-exec: Skips exec_script calls (for performance testing).");
  PrintShortHelp(
      "-q: Quiet mode, don't print anything on success.");
  PrintShortHelp(
      "--root: Specifies source root (overrides .gn file).");
  PrintShortHelp(
      "--time: Outputs a summary of how long everything took.");
  PrintShortHelp(
      "--tracelog: Writes a Chrome-compatible trace log to the given file.");
  PrintShortHelp(
      "-v: Verbose mode, print lots of logging.");
  PrintShortHelp(
      "--version: Print the GN binary's version and exit.");

  // Target declarations.
  OutputString("\nTarget declarations (type \"gn help <function>\" for more "
               "details):\n");
  const functions::FunctionInfoMap& function_map = functions::GetFunctions();
  for (functions::FunctionInfoMap::const_iterator i = function_map.begin();
       i != function_map.end(); ++i) {
    if (i->second.is_target)
      PrintShortHelp(i->second.help_short);
  }

  // Functions.
  OutputString("\nBuildfile functions (type \"gn help <function>\" for more "
               "details):\n");
  for (functions::FunctionInfoMap::const_iterator i = function_map.begin();
       i != function_map.end(); ++i) {
    if (!i->second.is_target)
      PrintShortHelp(i->second.help_short);
  }

  // Built-in variables.
  OutputString("\nBuilt-in predefined variables (type \"gn help <variable>\" "
               "for more details):\n");
  const variables::VariableInfoMap& builtin_vars =
      variables::GetBuiltinVariables();
  for (variables::VariableInfoMap::const_iterator i = builtin_vars.begin();
       i != builtin_vars.end(); ++i)
    PrintShortHelp(i->second.help_short);

  // Target variables.
  OutputString("\nVariables you set in targets (type \"gn help <variable>\" "
               "for more details):\n");
  const variables::VariableInfoMap& target_vars =
      variables::GetTargetVariables();
  for (variables::VariableInfoMap::const_iterator i = target_vars.begin();
       i != target_vars.end(); ++i)
    PrintShortHelp(i->second.help_short);

  OutputString("\nOther help topics:\n");
  PrintShortHelp("buildargs: How build arguments work.");
  PrintShortHelp("dotfile: Info about the toplevel .gn file.");
  PrintShortHelp(
      "input_conversion: Processing input from exec_script and read_file.");
  PrintShortHelp("patterns: How to use patterns.");
  PrintShortHelp("source_expansion: Map sources to outputs for scripts.");
}

}  // namespace

const char kHelp[] = "help";
const char kHelp_HelpShort[] =
    "help: Does what you think.";
const char kHelp_Help[] =
    "gn help <anything>\n"
    "  Yo dawg, I heard you like help on your help so I put help on the help\n"
    "  in the help.\n";

int RunHelp(const std::vector<std::string>& args) {
  if (args.size() == 0) {
    PrintToplevelHelp();
    return 0;
  }

  // Check commands.
  const commands::CommandInfoMap& command_map = commands::GetCommands();
  commands::CommandInfoMap::const_iterator found_command =
      command_map.find(args[0]);
  if (found_command != command_map.end()) {
    PrintLongHelp(found_command->second.help);
    return 0;
  }

  // Check functions.
  const functions::FunctionInfoMap& function_map = functions::GetFunctions();
  functions::FunctionInfoMap::const_iterator found_function =
      function_map.find(args[0]);
  if (found_function != function_map.end()) {
    PrintLongHelp(found_function->second.help);
    return 0;
  }

  // Builtin variables.
  const variables::VariableInfoMap& builtin_vars =
      variables::GetBuiltinVariables();
  variables::VariableInfoMap::const_iterator found_builtin_var =
      builtin_vars.find(args[0]);
  if (found_builtin_var != builtin_vars.end()) {
    PrintLongHelp(found_builtin_var->second.help);
    return 0;
  }

  // Target variables.
  const variables::VariableInfoMap& target_vars =
      variables::GetTargetVariables();
  variables::VariableInfoMap::const_iterator found_target_var =
      target_vars.find(args[0]);
  if (found_target_var != target_vars.end()) {
    PrintLongHelp(found_target_var->second.help);
    return 0;
  }

  // Random other topics.
  if (args[0] == "buildargs") {
    PrintLongHelp(kBuildArgs_Help);
    return 0;
  }
  if (args[0] == "dotfile") {
    PrintLongHelp(kDotfile_Help);
    return 0;
  }
  if (args[0] == "input_conversion") {
    PrintLongHelp(kInputConversion_Help);
    return 0;
  }
  if (args[0] == "patterns") {
    PrintLongHelp(kPattern_Help);
    return 0;
  }
  if (args[0] == "source_expansion") {
    PrintLongHelp(kSourceExpansion_Help);
    return 0;
  }

  // No help on this.
  Err(Location(), "No help on \"" + args[0] + "\".").PrintToStdout();
  RunHelp(std::vector<std::string>());
  return 1;
}

}  // namespace commands
