// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/command_line.h"
#include "tools/gn/commands.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file.h"
#include "tools/gn/item.h"
#include "tools/gn/pattern.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"

namespace commands {

namespace {

// Returns the file path generating this record.
base::FilePath FilePathForRecord(const BuilderRecord* record) {
  if (!record->item())
    return base::FilePath(FILE_PATH_LITERAL("=UNRESOLVED DEPENDENCY="));
  return record->item()->defined_from()->GetRange().begin().file()
      ->physical_name();
}

}  // namespace

const char kRefs[] = "refs";
const char kRefs_HelpShort[] =
    "refs: Find stuff referencing a target, directory, or config.";
const char kRefs_Help[] =
    "gn refs <label_pattern> [--files]\n"
    "\n"
    "  Finds code referencing a given label. The label can be a\n"
    "  target or config name. Unlike most other commands, unresolved\n"
    "  dependencies will be tolerated. This allows you to use this command\n"
    "  to find references to targets you're in the process of moving.\n"
    "\n"
    "  By default, the mapping from source item to dest item (where the\n"
    "  pattern matches the dest item). See \"gn help pattern\" for\n"
    "  information on pattern-matching rules.\n"
    "\n"
    "Option:\n"
    "  --files\n"
    "      Output unique filenames referencing a matched target or config.\n"
    "\n"
    "Examples:\n"
    "  gn refs \"//tools/gn/*\"\n"
    "      Find all targets depending on any target or config in the\n"
    "      \"tools/gn\" directory.\n"
    "\n"
    "  gn refs //tools/gn:gn\n"
    "      Find all targets depending on the given exact target name.\n"
    "\n"
    "  gn refs \"*gtk*\" --files\n"
    "      Find all unique buildfiles with a dependency on a target that has\n"
    "      the substring \"gtk\" in the name.\n";

int RunRefs(const std::vector<std::string>& args) {
  if (args.size() != 1 && args.size() != 2) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn refs <label_pattern>\"").PrintToStdout();
    return 1;
  }

  Pattern pattern(args[0]);
  // Check for common errors on input.
  if (args[0].find('*') == std::string::npos) {
    // We need to begin with a "//" and have a colon if there's no "*" or it
    // will be impossible to match anything.
    if (args[0].size() < 2 ||
        (args[0][0] != '/' && args[0][1] != '/') ||
        args[0].find(':') == std::string::npos) {
      OutputString("Assuming \"*" + args[0] +
                       "*\". See \"gn help refs\" for more information.\n",
                   DECORATION_YELLOW);
      pattern = Pattern("*" + args[0] + "*");
    }
  }

  Setup* setup = new Setup;
  setup->set_check_for_bad_items(false);
  // TODO(brettw) bug 343726: Use a temporary directory instead of this
  // default one to avoid messing up any build that's in there.
  if (!setup->DoSetup("//out/Default/") || !setup->Run())
    return 1;

  std::vector<const BuilderRecord*> records = setup->builder()->GetAllRecords();

  const CommandLine* cmdline = CommandLine::ForCurrentProcess();

  bool file_output = cmdline->HasSwitch("files");
  std::set<std::string> unique_output;

  for (size_t record_index = 0; record_index < records.size(); record_index++) {
    const BuilderRecord* record = records[record_index];
    const BuilderRecord::BuilderRecordSet& deps = record->all_deps();
    for (BuilderRecord::BuilderRecordSet::const_iterator d = deps.begin();
         d != deps.end(); ++d) {
      std::string label = (*d)->label().GetUserVisibleName(false);
      if (pattern.MatchesString(label)) {
        // Got a match.
        if (file_output) {
          unique_output.insert(FilePathToUTF8(FilePathForRecord(record)));
          break;  // Found a match for this target's file, don't need more.
        } else {
          // We can get dupes when there are differnet toolchains involved,
          // so we want to send all output through the de-duper.
          unique_output.insert(
              record->item()->label().GetUserVisibleName(false) + " -> " +
              label);
        }
      }
    }
  }

  for (std::set<std::string>::iterator i = unique_output.begin();
       i != unique_output.end(); ++i)
    OutputString(*i + "\n");

  return 0;
}

}  // namespace commands
