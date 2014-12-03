// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/switches.h"

namespace switches {

const char kArgs[] = "args";
const char kArgs_HelpShort[] =
    "--args: Specifies build arguments overrides.";
const char kArgs_Help[] =
    "--args: Specifies build arguments overrides.\n"
    "\n"
    "  See \"gn help buildargs\" for an overview of how build arguments work.\n"
    "\n"
    "  Most operations take a build directory. The build arguments are taken\n"
    "  from the previous build done in that directory. If a command specifies\n"
    "  --args, it will override the previous arguments stored in the build\n"
    "  directory, and use the specified ones.\n"
    "\n"
    "  The args specified will be saved to the build directory for subsequent\n"
    "  commands. Specifying --args=\"\" will clear all build arguments.\n"
    "\n"
    "Formatting\n"
    "\n"
    "  The value of the switch is interpreted in GN syntax. For typical usage\n"
    "  of string arguments, you will need to be careful about escaping of\n"
    "  quotes.\n"
    "\n"
    "Examples\n"
    "\n"
    "  gn gen out/Default --args=\"foo=\\\"bar\\\"\"\n"
    "\n"
    "  gn gen out/Default --args='foo=\"bar\" enable=true blah=7'\n"
    "\n"
    "  gn check out/Default --args=\"\"\n"
    "    Clears existing build args from the directory.\n"
    "\n"
    "  gn desc out/Default --args=\"some_list=[1, false, \\\"foo\\\"]\"\n";

#define COLOR_HELP_LONG \
    "--[no]color: Forces colored output on or off.\n"\
    "\n"\
    "  Normally GN will try to detect whether it is outputting to a terminal\n"\
    "  and will enable or disable color accordingly. Use of these switches\n"\
    "  will override the default.\n"\
    "\n"\
    "Examples\n"\
    "\n"\
    "  gn gen out/Default --color\n"\
    "\n"\
    "  gn gen out/Default --nocolor\n"
const char kColor[] = "color";
const char kColor_HelpShort[] =
    "--color: Force colored output.";
const char kColor_Help[] = COLOR_HELP_LONG;

const char kDotfile[] = "dotfile";
const char kDotfile_HelpShort[] =
    "--dotfile: override the name of the \".gn\" file.";
const char kDotfile_Help[] =
    "--dotfile: override the name of the \".gn\" file.\n"
    "\n"
    "  Normally GN loads the \".gn\"file  from the source root for some basic\n"
    "  configuration (see \"gn help dotfile\"). This flag allows you to\n"
    "  use a different file.\n"
    "\n"
    "  Note that this interacts with \"--root\" in a possibly incorrect way.\n"
    "  It would be nice to test the edge cases and document or fix.\n";

const char kNoColor[] = "nocolor";
const char kNoColor_HelpShort[] =
    "--nocolor: Force non-colored output.";
const char kNoColor_Help[] = COLOR_HELP_LONG;

const char kQuiet[] = "q";
const char kQuiet_HelpShort[] =
    "-q: Quiet mode. Don't print output on success.";
const char kQuiet_Help[] =
    "-q: Quiet mode. Don't print output on success.\n"
    "\n"
    "  This is useful when running as a part of another script.\n";

const char kRoot[] = "root";
const char kRoot_HelpShort[] =
    "--root: Explicitly specify source root.";
const char kRoot_Help[] =
    "--root: Explicitly specify source root.\n"
    "\n"
    "  Normally GN will look up in the directory tree from the current\n"
    "  directory to find a \".gn\" file. The source root directory specifies\n"
    "  the meaning of \"//\" beginning with paths, and the BUILD.gn file\n"
    "  in that directory will be the first thing loaded.\n"
    "\n"
    "  Specifying --root allows GN to do builds in a specific directory\n"
    "  regardless of the current directory.\n"
    "\n"
    "Examples\n"
    "\n"
    "  gn gen //out/Default --root=/home/baracko/src\n"
    "\n"
    "  gn desc //out/Default --root=\"C:\\Users\\BObama\\My Documents\\foo\"\n";

const char kThreads[] = "threads";
const char kThreads_HelpShort[] =
    "--threads: Specify number of worker threads.";
const char kThreads_Help[] =
    "--threads: Specify number of worker threads.\n"
    "\n"
    "  GN runs many threads to load and run build files. This can make\n"
    "  debugging challenging. Or you may want to experiment with different\n"
    "  values to see how it affects performance.\n"
    "\n"
    "  The parameter is the number of worker threads. This does not count the\n"
    "  main thread (so there are always at least two).\n"
    "\n"
    "Examples\n"
    "\n"
    "  gen gen out/Default --threads=1\n";

const char kTime[] = "time";
const char kTime_HelpShort[] =
    "--time: Outputs a summary of how long everything took.";
const char kTime_Help[] =
    "--time: Outputs a summary of how long everything took.\n"
    "\n"
    "  Hopefully self-explanatory.\n"
    "\n"
    "Examples\n"
    "\n"
    "  gn gen out/Default --time\n";

const char kTracelog[] = "tracelog";
const char kTracelog_HelpShort[] =
    "--tracelog: Writes a Chrome-compatible trace log to the given file.";
const char kTracelog_Help[] =
    "--tracelog: Writes a Chrome-compatible trace log to the given file.\n"
    "\n"
    "  The trace log will show file loads, executions, scripts, and writes.\n"
    "  This allows performance analysis of the generation step.\n"
    "\n"
    "  To view the trace, open Chrome and navigate to \"chrome://tracing/\",\n"
    "  then press \"Load\" and specify the file you passed to this parameter.\n"
    "\n"
    "Examples\n"
    "\n"
    "  gn gen out/Default --tracelog=mytrace.trace\n";

const char kVerbose[] = "v";
const char kVerbose_HelpShort[] =
    "-v: Verbose logging.";
const char kVerbose_Help[] =
    "-v: Verbose logging.\n"
    "\n"
    "  This will spew logging events to the console for debugging issues.\n"
    "  Good luck!\n";

const char kVersion[] = "version";
const char kVersion_HelpShort[] =
    "--version: Prints the GN version number and exits.";
// It's impossible to see this since gn_main prints the version and exits
// immediately if this switch is used.
const char kVersion_Help[] = "";

// -----------------------------------------------------------------------------

SwitchInfo::SwitchInfo()
    : short_help(""),
      long_help("") {
}

SwitchInfo::SwitchInfo(const char* short_help, const char* long_help)
    : short_help(short_help),
      long_help(long_help) {
}

#define INSERT_VARIABLE(var) \
    info_map[k##var] = SwitchInfo(k##var##_HelpShort, k##var##_Help);

const SwitchInfoMap& GetSwitches() {
  static SwitchInfoMap info_map;
  if (info_map.empty()) {
    INSERT_VARIABLE(Args)
    INSERT_VARIABLE(Color)
    INSERT_VARIABLE(Dotfile)
    INSERT_VARIABLE(NoColor)
    INSERT_VARIABLE(Root)
    INSERT_VARIABLE(Quiet)
    INSERT_VARIABLE(Time)
    INSERT_VARIABLE(Tracelog)
    INSERT_VARIABLE(Verbose)
    INSERT_VARIABLE(Version)
  }
  return info_map;
}

#undef INSERT_VARIABLE

}  // namespace switches
