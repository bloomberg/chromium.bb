// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/commands.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/ninja_writer.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"

namespace {

// Suppress output on success.
const char kSwitchQuiet[] = "q";

}  // namespace

int RunGenCommand(const std::vector<std::string>& args) {
  base::TimeTicks begin_time = base::TimeTicks::Now();

  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  if (!setup->DoSetup())
    return 1;

  // Cause the load to also generate the ninja files for each target.
  setup->build_settings().set_target_resolved_callback(
      base::Bind(&NinjaTargetWriter::RunAndWriteFile));

  // Do the actual load. This will also write out the target ninja files.
  if (!setup->Run())
    return 1;

  // Write the root ninja files.
  if (!NinjaWriter::RunAndWriteFiles(&setup->build_settings())) {
    Err(Location(),
        "Couldn't open root buildfile(s) for writing").PrintToStdout();
    return 1;
  }

  base::TimeTicks end_time = base::TimeTicks::Now();

  if (!CommandLine::ForCurrentProcess()->HasSwitch(kSwitchQuiet)) {
    OutputString("Done. ", DECORATION_GREEN);

    // TODO(brettw) get the number of targets without getting the entire list.
    std::vector<const Target*> all_targets;
    setup->build_settings().target_manager().GetAllTargets(&all_targets);
    std::string stats = "Generated " +
        base::IntToString(static_cast<int>(all_targets.size())) +
        " targets from " +
        base::IntToString(
            setup->scheduler().input_file_manager()->GetInputFileCount()) +
        " files in " +
        base::IntToString((end_time - begin_time).InMilliseconds()) + "ms\n";
    OutputString(stats);
  }

  return 0;
}
