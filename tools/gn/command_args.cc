// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "tools/gn/commands.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/tokenizer.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <stdlib.h>
#endif

namespace commands {

namespace {

const char kSwitchList[] = "list";
const char kSwitchShort[] = "short";

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

// Assumes DoesLineBeginWithComment(), this strips the # character from the
// beginning and normalizes preceeding whitespace.
std::string StripHashFromLine(const base::StringPiece& line) {
  // Replace the # sign and everything before it with 3 spaces, so that a
  // normal comment that has a space after the # will be indented 4 spaces
  // (which makes our formatting come out nicely). If the comment is indented
  // from there, we want to preserve that indenting.
  return "   " + line.substr(line.find('#') + 1).as_string();
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

    comment->insert(0, StripHashFromLine(line) + "\n");
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

int ListArgs(const std::string& build_dir) {
  Setup* setup = new Setup;
  setup->set_check_for_bad_items(false);
  if (!setup->DoSetup(build_dir) || !setup->Run())
    return 1;

  Scope::KeyValueMap build_args;
  setup->build_settings().build_args().MergeDeclaredArguments(&build_args);

  // Find all of the arguments we care about. Use a regular map so they're
  // sorted nicely when we write them out.
  std::map<base::StringPiece, Value> sorted_args;
  std::string list_value =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(kSwitchList);
  if (list_value.empty()) {
    // List all values.
    for (Scope::KeyValueMap::const_iterator i = build_args.begin();
         i != build_args.end(); ++i)
      sorted_args.insert(*i);
  } else {
    // List just the one specified as the parameter to --list.
    Scope::KeyValueMap::const_iterator found_arg = build_args.find(list_value);
    if (found_arg == build_args.end()) {
      Err(Location(), "Unknown build argument.",
          "You asked for \"" + list_value + "\" which I didn't find in any "
          "build file\nassociated with this build.").PrintToStdout();
      return 1;
    }
    sorted_args.insert(*found_arg);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchShort)) {
    // Short key=value output.
    for (std::map<base::StringPiece, Value>::iterator i = sorted_args.begin();
         i != sorted_args.end(); ++i) {
      OutputString(i->first.as_string());
      OutputString(" = ");
      OutputString(i->second.ToString(true));
      OutputString("\n");
    }
    return 0;
  }

  // Long output.
  for (std::map<base::StringPiece, Value>::iterator i = sorted_args.begin();
       i != sorted_args.end(); ++i) {
    PrintArgHelp(i->first, i->second);
    OutputString("\n");
  }

  return 0;
}

int EditArgsFile(const std::string& build_dir) {
  {
    // Scope the setup. We only use it for some basic state. We'll do the
    // "real" build below in the gen command.
    Setup setup;
    setup.set_check_for_bad_items(false);
    // Don't fill build arguments. We're about to edit the file which supplies
    // these in the first place.
    setup.set_fill_arguments(false);
    if (!setup.DoSetup(build_dir) || !setup.Run())
      return 1;

    base::FilePath build_arg_file =
        setup.build_settings().GetFullPath(setup.GetBuildArgFile());

    // Get the path to the user's editor.
    base::FilePath editor;
#if defined(OS_WIN)
    // We want to use 16-bit chars on Windows, so dont' go through the getenv
    // wrappers.
    base::char16 env_buf[MAX_PATH]
    if (::GetEnvironmentVariableW(L"EDITOR", env_buf, MAX_PATH) == 0)
      editor = base::FilePath(base::string16("notepad.exe"));
    else
      editor = base::FilePath(base::string16(env_buf));

    base::CommandLine editor_cmdline(editor);
    editor_cmdline.AppendArgPath(build_arg_file);

    // Use GetAppOutput as a simple way to wait for the process to exit.
    std::string output;
    base::GetAppOutput(editor_cmdline, &output);
#else
    const char* editor_ptr = getenv("VISUAL");
    if (!editor_ptr)
      editor_ptr = getenv("EDITOR");
    if (!editor_ptr)
      editor_ptr = "vi";

    std::string cmd(editor_ptr);
    cmd.append(" \"");

    std::string escaped_build_arg_file = build_arg_file.value();
    ReplaceSubstringsAfterOffset(&escaped_build_arg_file, 0, "\"", "\\\"");
    cmd.append(escaped_build_arg_file);

    cmd.push_back('"');
    system(cmd.c_str());
#endif
  }

  // Now do a normal "gen" command.
  std::vector<std::string> gen_commands;
  gen_commands.push_back(build_dir);
  return RunGen(gen_commands);
}

}  // namespace

extern const char kArgs[] = "args";
extern const char kArgs_HelpShort[] =
    "args: Display or configure arguments declared by the build.";
extern const char kArgs_Help[] =
    "gn args [arg name]\n"
    "\n"
    "  See also \"gn help buildargs\" for a more high-level overview of how\n"
    "  build arguments work.\n"
    "\n"
    "Usage\n"
    "  gn args <dir_name>\n"
    "      Open the arguments for the given build directory in an editor\n"
    "      (as specified by the EDITOR environment variable). If the given\n"
    "      build directory doesn't exist, it will be created and an empty\n"
    "      args file will be opened in the editor. You would type something\n"
    "      like this into that file:\n"
    "          enable_doom_melon=false\n"
    "          os=\"android\"\n"
    "\n"
    "      Note: you can edit the build args manually by editing the file\n"
    "      \"gn.args\" in the build directory and then running\n"
    "      \"gn gen <build_dir>\".\n"
    "\n"
    "  gn args <dir_name> --list[=<exact_arg>] [--short]\n"
    "      Lists all build arguments available in the current configuration,\n"
    "      or, if an exact_arg is specified for the list flag, just that one\n"
    "      build argument.\n"
    "\n"
    "      The output will list the declaration location, default value, and\n"
    "      comment preceeding the declaration. If --short is specified,\n"
    "      only the names and values will be printed.\n"
    "\n"
    "      If the dir_name is specified, the build configuration will be\n"
    "      taken from that build directory. The reason this is needed is that\n"
    "      the definition of some arguments is dependent on the build\n"
    "      configuration, so setting some values might add, remove, or change\n"
    "      the default values for other arguments. Specifying your exact\n"
    "      configuration allows the proper arguments to be displayed.\n"
    "\n"
    "      Instead of specifying the dir_name, you can also use the\n"
    "      command-line flag to specify the build configuration:\n"
    "        --args=<exact list of args to use>\n"
    "\n"
    "Examples\n"
    "  gn args out/Debug\n"
    "    Opens an editor with the args for out/Debug.\n"
    "\n"
    "  gn args out/Debug --list --short\n"
    "    Prints all arguments with their default values for the out/Debug\n"
    "    build.\n"
    "\n"
    "  gn args out/Debug --list=cpu_arch\n"
    "    Prints information about the \"cpu_arch\" argument for the out/Debug\n"
    "    build.\n"
    "\n"
    "  gn args --list --args=\"os=\\\"android\\\" enable_doom_melon=true\"\n"
    "    Prints all arguments with the default values for a build with the\n"
    "    given arguments set (which may affect the values of other\n"
    "    arguments).\n";

int RunArgs(const std::vector<std::string>& args) {
  if (args.size() != 1) {
    Err(Location(), "Exactly one build dir needed.",
        "Usage: \"gn args <build_dir>\"\n"
        "Or see \"gn help args\" for more variants.").PrintToStdout();
    return 1;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kSwitchList))
    return ListArgs(args[0]);
  return EditArgsFile(args[0]);
}

}  // namespace commands
