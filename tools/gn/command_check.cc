// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/commands.h"
#include "tools/gn/header_checker.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/trace.h"

namespace commands {

const char kCheck[] = "check";
const char kCheck_HelpShort[] =
    "check: Check header dependencies.";
const char kCheck_Help[] =
    "gn check <out_dir> [<target label>] [--force]\n"
    "\n"
    "  \"gn check\" is the same thing as \"gn gen\" with the \"--check\" flag\n"
    "  except that this command does not write out any build files. It's\n"
    "  intended to be an easy way to manually trigger include file checking.\n"
    "\n"
    "  The <label_pattern> can take exact labels or patterns that match more\n"
    "  than one (although not general regular expressions). If specified,\n"
    "  only those matching targets will be checked.\n"
    "  See \"gn help label_pattern\" for details.\n"
    "\n"
    "  --force\n"
    "      Ignores specifications of \"check_includes = false\" and checks\n"
    "      all target's files that match the target label.\n"
    "\n"
    "  See \"gn help\" for the common command-line switches.\n"
    "\n"
    "Examples\n"
    "\n"
    "  gn check out/Debug\n"
    "      Check everything.\n"
    "\n"
    "  gn check out/Default //foo:bar\n"
    "      Check only the files in the //foo:bar target.\n"
    "\n"
    "  gn check out/Default \"//foo/*\n"
    "      Check only the files in targets in the //foo directory tree.\n";

int RunCheck(const std::vector<std::string>& args) {
  if (args.size() != 1 && args.size() != 2) {
    Err(Location(), "You're holding it wrong.",
        "Usage: \"gn check <out_dir> [<target_label>]\"").PrintToStdout();
    return 1;
  }

  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup();
  if (!setup->DoSetup(args[0], false))
    return 1;
  if (!setup->Run())
    return 1;

  std::vector<const Target*> targets_to_check;
  if (args.size() == 2) {
    // Compute the target to check (empty means everything).
    if (!ResolveTargetsFromCommandLinePattern(setup, args[1], false,
                                              &targets_to_check))
      return 1;
    if (targets_to_check.size() == 0) {
      OutputString("No matching targets.\n");
      return 1;
    }
  }

  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  bool force = cmdline->HasSwitch("force");

  if (!CheckPublicHeaders(&setup->build_settings(),
                          setup->builder()->GetAllResolvedTargets(),
                          targets_to_check,
                          force))
    return 1;

  OutputString("Header dependency check OK\n", DECORATION_GREEN);
  return 0;
}

bool CheckPublicHeaders(const BuildSettings* build_settings,
                        const std::vector<const Target*>& all_targets,
                        const std::vector<const Target*>& to_check,
                        bool force_check) {
  ScopedTrace trace(TraceItem::TRACE_CHECK_HEADERS, "Check headers");

  scoped_refptr<HeaderChecker> header_checker(
      new HeaderChecker(build_settings, all_targets));

  std::vector<Err> header_errors;
  header_checker->Run(to_check, force_check, &header_errors);
  for (size_t i = 0; i < header_errors.size(); i++) {
    if (i > 0)
      OutputString("___________________\n", DECORATION_YELLOW);
    header_errors[i].PrintToStdout();
  }
  return header_errors.empty();
}

}  // namespace commands
