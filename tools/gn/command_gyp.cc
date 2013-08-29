// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fstream>

#include "base/atomicops.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/commands.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/ninja_target_writer.h"
#include "tools/gn/ninja_writer.h"
#include "tools/gn/path_output.h"
#include "tools/gn/setup.h"
#include "tools/gn/standard_out.h"

namespace commands {

namespace {

// Suppress output on success.
const char kSwitchQuiet[] = "q";

// Skip actually executing GYP. This is for when you're working on the GN
// build and don't want to wait for GYP to regenerate. All GN files are
// regenerated, but the GYP ones are not.
const char kSwitchNoGyp[] = "no-gyp";

void TargetResolvedCallback(base::subtle::Atomic32* write_counter,
                            const Target* target) {
  base::subtle::NoBarrier_AtomicIncrement(write_counter, 1);
  NinjaTargetWriter::RunAndWriteFile(target);
}

bool SimpleNinjaParse(const std::string& data,
                      std::set<std::string>* subninjas,
                      size_t* first_subninja_offset) {
  const size_t kSubninjaPrefixLen = 10;
  const char kSubninjaPrefix[kSubninjaPrefixLen + 1] = "\nsubninja ";

  *first_subninja_offset = std::string::npos;
  size_t next_subninja = 0;
  while ((next_subninja = data.find(kSubninjaPrefix, next_subninja)) !=
         std::string::npos) {
    if (*first_subninja_offset == std::string::npos)
      *first_subninja_offset = next_subninja;

    size_t line_end = data.find('\n', next_subninja + 1);
    if (line_end == std::string::npos)
      return false;

    std::string filename = data.substr(
        next_subninja + kSubninjaPrefixLen,
        line_end - next_subninja - kSubninjaPrefixLen);
    subninjas->insert(filename);

    next_subninja = line_end;
  }
  return *first_subninja_offset != std::string::npos;
}

bool FixupBuildNinja(const BuildSettings* build_settings,
                     const base::FilePath& buildfile) {
  std::string contents;
  if (!file_util::ReadFileToString(buildfile, &contents)) {
    Err(Location(), "Could not load " + FilePathToUTF8(buildfile))
        .PrintToStdout();
    return false;
  }

  std::set<std::string> subninjas;
  size_t first_subninja_offset = 0;
  if (!SimpleNinjaParse(contents, &subninjas, &first_subninja_offset)) {
    Err(Location(), "Could not parse " + FilePathToUTF8(buildfile))
        .PrintToStdout();
    return false;
  }

  // Write toolchain files.
  std::vector<const Settings*> all_settings;
  if (!NinjaWriter::RunAndWriteToolchainFiles(
          build_settings, subninjas, &all_settings))
    return false;

  // Copy first part of buildfile to the output.
  std::ofstream file;
  file.open(FilePathToUTF8(buildfile).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail()) {
    Err(Location(), "Could not write " + FilePathToUTF8(buildfile))
        .PrintToStdout();
    return false;
  }
  file.write(contents.data(), first_subninja_offset);

  // Add refs for our toolchains to the original build.ninja.
  NinjaHelper helper(build_settings);
  PathOutput path_output(build_settings->build_dir(), ESCAPE_NINJA, true);
  file << "\n# GN-added toolchain files.\n";
  for (size_t i = 0; i < all_settings.size(); i++) {
    file << "subninja ";
    path_output.WriteFile(file,
                          helper.GetNinjaFileForToolchain(all_settings[i]));
    file << std::endl;
  }
  file << "\n# GYP-written subninjas.";

  // Write remaining old subninjas from original file.
  file.write(&contents[first_subninja_offset],
             contents.size() - first_subninja_offset);
  return true;
}

bool RunGyp(const BuildSettings* build_settings) {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(kSwitchQuiet))
    OutputString("Running GYP...\n");

  const base::FilePath& python_path = build_settings->python_path();

  // Construct the command line. Note that AppendArgPath and AppendSwitchPath
  // don't preserve the relative ordering, and we need the python file to be
  // first, so we have to convert switch values to strings before appending.
  //
  // Note that GYP will get confused if this path is quoted, so don't quote it
  // and hope that there are no spaces!
  CommandLine cmdline(python_path);
  cmdline.AppendArgPath(
      build_settings->GetFullPath(SourceFile("//build/gyp_chromium.py")));
  cmdline.AppendArg("-G");
  cmdline.AppendArg("output_dir=out.gn");
  cmdline.AppendArg("-f");
  cmdline.AppendArg("ninja");

  std::string output;
  if (!base::GetAppOutput(cmdline, &output)) {
    Err(Location(), "GYP execution failed.", output).PrintToStdout();
    return false;
  }
  return true;
}

}  // namespace

const char kGyp[] = "gyp";
const char kGyp_HelpShort[] =
    "gyp: Run gyp and then GN.";
const char kGyp_Help[] =
    "TODO(brettw) write this.\n";

// Note: partially duplicated from command_gen.cc.
int RunGyp(const std::vector<std::string>& args) {
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  bool no_gyp = cmdline->HasSwitch(kSwitchNoGyp);

  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup = new Setup;
  if (!setup->DoSetup())
    return 1;

  setup->build_settings().SetBuildDir(SourceDir("//out.gn/Debug/"));
  setup->build_settings().set_using_external_generator(true);

  // Provide a way for buildfiles to know we're doing a GYP build.
  /*
  Scope::KeyValueMap variable_overrides;
  variable_overrides["is_gyp"] = Value(NULL, true);
  setup->build_settings().build_args().AddArgOverrides(variable_overrides);
  */

  base::TimeTicks begin_time = base::TimeTicks::Now();
  if (!no_gyp) {
    if (!RunGyp(&setup->build_settings()))
      return 1;
  }
  base::TimeTicks end_gyp_time = base::TimeTicks::Now();

  if (!cmdline->HasSwitch(kSwitchQuiet))
    OutputString("Running GN...\n");

  // Cause the load to also generate the ninja files for each target. We wrap
  // the writing to maintain a counter.
  base::subtle::Atomic32 write_counter = 0;
  setup->build_settings().set_target_resolved_callback(
      base::Bind(&TargetResolvedCallback, &write_counter));

  // Do the actual load. This will also write out the target ninja files.
  if (!setup->Run())
    return 1;

  // Integrate with the GYP build.
  if (!no_gyp) {
    base::FilePath ninja_buildfile(setup->build_settings().GetFullPath(
        SourceFile(setup->build_settings().build_dir().value() +
                   "build.ninja")));
    if (!FixupBuildNinja(&setup->build_settings(), ninja_buildfile))
      return 1;
  }

  // Timing info.
  base::TimeTicks end_time = base::TimeTicks::Now();
  if (!cmdline->HasSwitch(kSwitchQuiet)) {
    OutputString("Done. ", DECORATION_GREEN);

    std::string stats = "Wrote " +
        base::IntToString(static_cast<int>(write_counter)) +
        " targets from " +
        base::IntToString(
            setup->scheduler().input_file_manager()->GetInputFileCount()) +
        " files in " +
        base::IntToString((end_time - end_gyp_time).InMilliseconds()) + "ms " +
        "(GYP took " +
        base::IntToString((end_gyp_time - begin_time).InMilliseconds()) +
        "ms)\n";

    OutputString(stats);
  }

  return 0;
}

}  // namespace commands
