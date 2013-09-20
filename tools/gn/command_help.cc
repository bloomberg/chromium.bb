// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <iostream>

#include "tools/gn/args.h"
#include "tools/gn/commands.h"
#include "tools/gn/err.h"
#include "tools/gn/file_template.h"
#include "tools/gn/functions.h"
#include "tools/gn/input_conversion.h"
#include "tools/gn/pattern.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/variables.h"

namespace commands {

namespace {

// Prints a line for a command, assuming there is a colon. Everything before
// the colon is the command (and is highlighted). After the colon if there is
// a square bracket, the contents of the bracket is dimmed.
void PrintShortHelp(const std::string& line) {
  size_t colon_offset = line.find(':');
  size_t first_normal = 0;
  if (colon_offset != std::string::npos) {
    OutputString("  " + line.substr(0, colon_offset), DECORATION_YELLOW);
    first_normal = colon_offset;
  }

  // See if the colon is followed by a " [" and if so, dim the contents of [ ].
  if (first_normal > 0 &&
      line.size() > first_normal + 2 &&
      line[first_normal + 1] == ' ' && line[first_normal + 2] == '[') {
    size_t begin_bracket = first_normal + 2;
    OutputString(": ");
    first_normal = line.find(']', begin_bracket);
    if (first_normal == std::string::npos)
      first_normal = line.size();
    else
      first_normal++;
    OutputString(line.substr(begin_bracket, first_normal - begin_bracket),
                 DECORATION_DIM);
  }

  OutputString(line.substr(first_normal) + "\n");
}

void PrintToplevelHelp() {
  OutputString("Commands (type \"gn help <command>\" for more details):\n");

  const commands::CommandInfoMap& command_map = commands::GetCommands();
  for (commands::CommandInfoMap::const_iterator i = command_map.begin();
       i != command_map.end(); ++i)
    PrintShortHelp(i->second.help_short);

  OutputString(
      "\n"
      "  When run with no arguments \"gn gen\" is assumed.\n"
      "\n"
      "Common switches:\n");
  PrintShortHelp(
      "--args: Specifies build args overrides. See \"gn help buildargs\".");
  PrintShortHelp(
      "-q: Quiet mode, don't print anything on success.");
  PrintShortHelp(
      "--output: Directory for build output (relative to source root).");
  PrintShortHelp(
      "--root: Specifies source root (overrides .gn file).");
  PrintShortHelp(
      "--secondary: Specifies secondary source root (overrides .gn file).");
  PrintShortHelp(
      "-v: Verbose mode, print lots of logging.");

  // Functions.
  OutputString("\nBuildfile functions (type \"gn help <function>\" for more "
               "details):\n");
  const functions::FunctionInfoMap& function_map = functions::GetFunctions();
  std::vector<std::string> sorted_functions;
  for (functions::FunctionInfoMap::const_iterator i = function_map.begin();
       i != function_map.end(); ++i)
    sorted_functions.push_back(i->first.as_string());
  std::sort(sorted_functions.begin(), sorted_functions.end());
  for (size_t i = 0; i < sorted_functions.size(); i++)
    OutputString("  " + sorted_functions[i] + "\n", DECORATION_YELLOW);

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
    OutputString(found_command->second.help);
    return 0;
  }

  // Check functions.
  const functions::FunctionInfoMap& function_map = functions::GetFunctions();
  functions::FunctionInfoMap::const_iterator found_function =
      function_map.find(args[0]);
  if (found_function != function_map.end()) {
    OutputString(found_function->second.help);
    return 0;
  }

  // Builtin variables.
  const variables::VariableInfoMap& builtin_vars =
      variables::GetBuiltinVariables();
  variables::VariableInfoMap::const_iterator found_builtin_var =
      builtin_vars.find(args[0]);
  if (found_builtin_var != builtin_vars.end()) {
    OutputString(found_builtin_var->second.help);
    return 0;
  }

  // Target variables.
  const variables::VariableInfoMap& target_vars =
      variables::GetTargetVariables();
  variables::VariableInfoMap::const_iterator found_target_var =
      target_vars.find(args[0]);
  if (found_target_var != target_vars.end()) {
    OutputString(found_target_var->second.help);
    return 0;
  }

  // Random other topics.
  if (args[0] == "buildargs") {
    OutputString(kBuildArgs_Help);
    return 0;
  }
  if (args[0] == "dotfile") {
    OutputString(kDotfile_Help);
    return 0;
  }
  if (args[0] == "input_conversion") {
    OutputString(kInputConversion_Help);
    return 0;
  }
  if (args[0] == "patterns") {
    OutputString(kPattern_Help);
    return 0;
  }
  if (args[0] == "source_expansion") {
    OutputString(kSourceExpansion_Help);
    return 0;
  }

  // No help on this.
  Err(Location(), "No help on \"" + args[0] + "\".").PrintToStdout();
  RunHelp(std::vector<std::string>());
  return 1;
}

}  // namespace commands
