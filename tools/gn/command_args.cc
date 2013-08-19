// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "tools/gn/commands.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/tokenizer.h"

namespace commands {

namespace {

bool DoesLineBeginWithComment(const base::StringPiece& line) {
  // Skip whitespace.
  size_t i = 0;
  while (i < line.size() && IsAsciiWhitespace(line[i]))
    i++;

  return i < line.size() && line[i] == '#';
}

// Returns the offset of the beginning of the line identified by |offset|.
size_t BackUpToLineBegin(const std::string& data, size_t offset) {
  // Degenerate case of an empty line. Below we'll try to return the
  // character after the newline, but that will be incorrect in this case.
  if (offset == 0 || Tokenizer::IsNewline(data, offset))
    return offset;

  size_t cur = offset;
  do {
    cur --;
    if (Tokenizer::IsNewline(data, cur))
      return cur + 1;  // Want the first character *after* the newline.
  } while (cur > 0);
  return 0;
}

// Assumes DoesLineBeginWithComment().
std::string StripCommentFromLine(const base::StringPiece& line) {
  std::string ret = line.as_string();
  for (size_t i = 0; i < ret.size(); i++) {
    if (ret[i] == '#') {
      ret[i] = ' ';
      break;
    }
  }
  return ret;
}

// Tries to find the comment before the setting of the given value.
void GetContextForValue(const Value& value,
                        std::string* location_str,
                        std::string* comment) {
  Location location = value.origin()->GetRange().begin();
  const InputFile* file = location.file();
  if (!file)
    return;

  *location_str = file->name().value() + ":" +
      base::IntToString(location.line_number());

  const std::string& data = file->contents();
  size_t line_off =
      Tokenizer::ByteOffsetOfNthLine(data, location.line_number());

  while (line_off > 1) {
    line_off -= 2;  // Back up to end of previous line.
    size_t previous_line_offset = BackUpToLineBegin(data, line_off);

    base::StringPiece line(&data[previous_line_offset],
                           line_off - previous_line_offset + 1);
    if (!DoesLineBeginWithComment(line))
      break;

    comment->insert(0, StripCommentFromLine(line) + "\n");
    line_off = previous_line_offset;
  }
}

void PrintArgHelp(const base::StringPiece& name, const Value& value) {
  OutputString(name.as_string(), DECORATION_YELLOW);
  OutputString("  Default = " + value.ToString(true) + "\n");

  if (value.origin()) {
    std::string location, comment;
    GetContextForValue(value, &location, &comment);
    OutputString("    " + location + "\n" + comment);
  } else {
    OutputString("    (Internally set)\n");
  }
}

}  // namespace

extern const char kArgs[] = "args";
extern const char kArgs_HelpShort[] =
    "args: Display configurable arguments declared by the build.";
extern const char kArgs_Help[] =
    "gn args [arg name]\n"
    "  Displays all arguments declared by buildfiles along with their\n"
    "  description. Build arguments are anything in a declare_args() block\n"
    "  in any buildfile. The comment preceeding the declaration will be\n"
    "  displayed here (so comment well!).\n"
    "\n"
    "  These arguments can be overriden on the command-line:\n"
    "    --args=\"doom_melon_setting=5 component_build=1\"\n"
    "  or in a toolchain definition (see \"gn help buildargs\" for more on\n"
    "  how this all works).\n"
    "\n"
    "  If \"arg name\" is specified, only the information for that argument\n"
    "  will be displayed. Otherwise all arguments will be displayed.\n";

int RunArgs(const std::vector<std::string>& args) {
  Setup* setup = new Setup;
  setup->set_check_for_bad_items(false);
  if (!setup->DoSetup() || !setup->Run())
    return 1;

  const Scope::KeyValueMap& build_args =
      setup->build_settings().build_args().declared_arguments();

  if (args.size() == 1) {
    // Get help on a specific command.
    Scope::KeyValueMap::const_iterator found_arg = build_args.find(args[0]);
    if (found_arg == build_args.end()) {
      Err(Location(), "Unknown build arg.",
          "You asked for \"" + args[0] + "\" which I didn't find in any "
          "buildfile\nassociated with this build.");
      return 1;
    }
    PrintArgHelp(args[0], found_arg->second);
    return 0;
  } else if (args.size() > 1) {
    // Too many arguments.
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn args [arg name]\"").PrintToStdout();
    return 1;
  }

  // List all arguments. First put them in a regular map so they're sorted.
  std::map<base::StringPiece, Value> sorted_args;
  for (Scope::KeyValueMap::const_iterator i = build_args.begin();
       i != build_args.end(); ++i)
    sorted_args.insert(*i);

  for (std::map<base::StringPiece, Value>::iterator i = sorted_args.begin();
       i != sorted_args.end(); ++i) {
    PrintArgHelp(i->first, i->second);
    OutputString("\n");
  }

  return 0;
}

}  // namespace commands
