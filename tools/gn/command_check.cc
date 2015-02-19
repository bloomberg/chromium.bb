// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "tools/gn/commands.h"
#include "tools/gn/header_checker.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/switches.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

namespace commands {

const char kCheck[] = "check";
const char kCheck_HelpShort[] =
    "check: Check header dependencies.";
const char kCheck_Help[] =
    "gn check <out_dir> [<label_pattern>] [--force]\n"
    "\n"
    "  \"gn check\" is the same thing as \"gn gen\" with the \"--check\" flag\n"
    "  except that this command does not write out any build files. It's\n"
    "  intended to be an easy way to manually trigger include file checking.\n"
    "\n"
    "  The <label_pattern> can take exact labels or patterns that match more\n"
    "  than one (although not general regular expressions). If specified,\n"
    "  only those matching targets will be checked. See\n"
    "  \"gn help label_pattern\" for details.\n"
    "\n"
    "  The .gn file may specify a list of targets to be checked. Only these\n"
    "  targets will be checked if no label_pattern is specified on the\n"
    "  command line. Otherwise, the command-line list is used instead. See\n"
    "  \"gn help dotfile\".\n"
    "\n"
    "Command-specific switches\n"
    "\n"
    "  --force\n"
    "      Ignores specifications of \"check_includes = false\" and checks\n"
    "      all target's files that match the target label.\n"
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

  std::vector<const Target*> all_targets =
      setup->builder()->GetAllResolvedTargets();

  bool filtered_by_build_config = false;
  std::vector<const Target*> targets_to_check;
  if (args.size() > 1) {
    // Compute the targets to check.
    std::vector<std::string> inputs(args.begin() + 1, args.end());
    UniqueVector<const Target*> target_matches;
    UniqueVector<const Config*> config_matches;
    UniqueVector<const Toolchain*> toolchain_matches;
    UniqueVector<SourceFile> file_matches;
    if (!ResolveFromCommandLineInput(setup, inputs, false,
                                     &target_matches, &config_matches,
                                     &toolchain_matches, &file_matches))
      return 1;

    if (target_matches.size() == 0) {
      OutputString("No matching targets.\n");
      return 1;
    }
    targets_to_check.insert(targets_to_check.begin(),
                            target_matches.begin(), target_matches.end());
  } else {
    // No argument means to check everything allowed by the filter in
    // the build config file.
    if (setup->check_patterns()) {
      FilterTargetsByPatterns(all_targets, *setup->check_patterns(),
                              &targets_to_check);
      filtered_by_build_config = targets_to_check.size() != all_targets.size();
    } else {
      // No global filter, check everything.
      targets_to_check = all_targets;
    }
  }

  const base::CommandLine* cmdline = base::CommandLine::ForCurrentProcess();
  bool force = cmdline->HasSwitch("force");

  if (!CheckPublicHeaders(&setup->build_settings(), all_targets,
                          targets_to_check, force))
    return 1;

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kQuiet)) {
    if (filtered_by_build_config) {
      // Tell the user about the implicit filtering since this is obscure.
      OutputString(base::StringPrintf(
          "%d targets out of %d checked based on the check_targets defined in"
          " \".gn\".\n",
          static_cast<int>(targets_to_check.size()),
          static_cast<int>(all_targets.size())));
    }
    OutputString("Header dependency check OK\n", DECORATION_GREEN);
  }
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
