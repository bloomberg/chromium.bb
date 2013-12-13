// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <map>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/commands.h"
#include "tools/gn/err.h"
#include "tools/gn/gyp_helper.h"
#include "tools/gn/gyp_target_writer.h"
#include "tools/gn/location.h"
#include "tools/gn/parser.h"
#include "tools/gn/setup.h"
#include "tools/gn/source_file.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"
#include "tools/gn/tokenizer.h"

namespace commands {

namespace {

typedef GypTargetWriter::TargetGroup TargetGroup;
typedef std::map<Label, TargetGroup> CorrelatedTargetsMap;
typedef std::map<SourceFile, std::vector<TargetGroup> > GroupedTargetsMap;
typedef std::map<std::string, std::string> StringStringMap;
typedef std::vector<const BuilderRecord*> RecordVector;

std::vector<const BuilderRecord*> GetAllResolvedTargetRecords(
    const Builder* builder) {
  std::vector<const BuilderRecord*> all = builder->GetAllRecords();
  std::vector<const BuilderRecord*> result;
  result.reserve(all.size());
  for (size_t i = 0; i < all.size(); i++) {
    if (all[i]->type() == BuilderRecord::ITEM_TARGET &&
        all[i]->should_generate() &&
        all[i]->item())
      result.push_back(all[i]);
  }
  return result;
}

// Groups targets sharing the same label between debug and release.
//
// We strip the toolchain label because the 64-bit and 32-bit builds, for
// example, will have different toolchains but we want to correlate them.
//
// TODO(brettw) this assumes that everything in the build has the same
// toolchain. To support cross-compiling and nacl, we'll need to differentiate
// the 32-vs-64-bit case and the default-toolchain-vs-not case. When we find
// a target not using hte default toolchain, we should probably just shell
// out to ninja.
void CorrelateTargets(const RecordVector& debug_targets,
                      const RecordVector& release_targets,
                      const RecordVector& host_debug_targets,
                      const RecordVector& host_release_targets,
                      const RecordVector& debug64_targets,
                      const RecordVector& release64_targets,
                      CorrelatedTargetsMap* correlated) {
  // Normal.
  for (size_t i = 0; i < debug_targets.size(); i++) {
    const BuilderRecord* record = debug_targets[i];
    (*correlated)[record->label().GetWithNoToolchain()].debug = record;
  }
  for (size_t i = 0; i < release_targets.size(); i++) {
    const BuilderRecord* record = release_targets[i];
    (*correlated)[record->label().GetWithNoToolchain()].release = record;
  }

  // Host build.
  for (size_t i = 0; i < host_debug_targets.size(); i++) {
    const BuilderRecord* record = host_debug_targets[i];
    (*correlated)[record->label().GetWithNoToolchain()].host_debug = record;
  }
  for (size_t i = 0; i < host_release_targets.size(); i++) {
    const BuilderRecord* record = host_release_targets[i];
    (*correlated)[record->label().GetWithNoToolchain()].host_release = record;
  }

  // Host build.
  for (size_t i = 0; i < debug64_targets.size(); i++) {
    const BuilderRecord* record = debug64_targets[i];
    (*correlated)[record->label().GetWithNoToolchain()].debug64 = record;
  }
  for (size_t i = 0; i < release64_targets.size(); i++) {
    const BuilderRecord* record = release64_targets[i];
    (*correlated)[record->label().GetWithNoToolchain()].release64 = record;
  }
}

// Verifies that both debug and release variants match. They can differ only
// by flags.
bool EnsureTargetsMatch(const TargetGroup& group, Err* err) {
  // Check that both debug and release made this target.
  if (!group.debug || !group.release) {
    const BuilderRecord* non_null_one =
        group.debug ? group.debug : group.release;
    *err = Err(Location(), "The debug and release builds did not both generate "
        "a target with the name\n" +
        non_null_one->label().GetUserVisibleName(true));
    return false;
  }

  const Target* debug_target = group.debug->item()->AsTarget();
  const Target* release_target = group.release->item()->AsTarget();

  // Check the flags that determine if and where we write the GYP file.
  if (group.debug->should_generate() != group.release->should_generate() ||
      debug_target->external() != release_target->external() ||
      debug_target->gyp_file() != release_target->gyp_file()) {
    *err = Err(Location(), "The metadata for the target\n" +
        group.debug->label().GetUserVisibleName(true) +
        "\ndoesn't match between the debug and release builds.");
    return false;
  }

  // Check that the sources match.
  if (debug_target->sources().size() != release_target->sources().size()) {
    *err = Err(Location(), "The source file count for the target\n" +
        group.debug->label().GetUserVisibleName(true) +
        "\ndoesn't have the same number of files between the debug and "
        "release builds.");
    return false;
  }
  for (size_t i = 0; i < debug_target->sources().size(); i++) {
    if (debug_target->sources()[i] != release_target->sources()[i]) {
      *err = Err(Location(), "The debug and release version of the target \n" +
          group.debug->label().GetUserVisibleName(true) +
          "\ndon't agree on the file\n" +
          debug_target->sources()[i].value());
      return false;
    }
  }

  // Check that the deps match.
  if (debug_target->deps().size() != release_target->deps().size()) {
    *err = Err(Location(), "The source file count for the target\n" +
        group.debug->label().GetUserVisibleName(true) +
        "\ndoesn't have the same number of deps between the debug and "
        "release builds.");
    return false;
  }
  for (size_t i = 0; i < debug_target->deps().size(); i++) {
    if (debug_target->deps()[i].label != release_target->deps()[i].label) {
      *err = Err(Location(), "The debug and release version of the target \n" +
          group.debug->label().GetUserVisibleName(true) +
          "\ndon't agree on the dep\n" +
          debug_target->deps()[i].label.GetUserVisibleName(true));
      return false;
    }
  }
  return true;
}

// Returns the (number of targets, number of GYP files).
std::pair<int, int> WriteGypFiles(CommonSetup* debug_setup,
                                  CommonSetup* release_setup,
                                  CommonSetup* host_debug_setup,
                                  CommonSetup* host_release_setup,
                                  CommonSetup* debug64_setup,
                                  CommonSetup* release64_setup,
                                  Err* err) {
  // Group all targets by output GYP file name.
  std::vector<const BuilderRecord*> debug_targets =
      GetAllResolvedTargetRecords(debug_setup->builder());
  std::vector<const BuilderRecord*> release_targets =
      GetAllResolvedTargetRecords(release_setup->builder());

  // Host build is optional.
  std::vector<const BuilderRecord*> host_debug_targets;
  std::vector<const BuilderRecord*> host_release_targets;
  if (host_debug_setup && host_release_setup) {
      host_debug_targets = GetAllResolvedTargetRecords(
          host_debug_setup->builder());
      host_release_targets = GetAllResolvedTargetRecords(
          host_release_setup->builder());
  }

  // 64-bit build is optional.
  std::vector<const BuilderRecord*> debug64_targets;
  std::vector<const BuilderRecord*> release64_targets;
  if (debug64_setup && release64_setup) {
      debug64_targets = GetAllResolvedTargetRecords(
          debug64_setup->builder());
      release64_targets = GetAllResolvedTargetRecords(
          release64_setup->builder());
  }

  // Match up the debug and release version of each target by label.
  CorrelatedTargetsMap correlated;
  CorrelateTargets(debug_targets, release_targets,
                   host_debug_targets, host_release_targets,
                   debug64_targets, release64_targets,
                   &correlated);

  GypHelper helper;
  GroupedTargetsMap grouped_targets;
  int target_count = 0;
  for (CorrelatedTargetsMap::iterator i = correlated.begin();
       i != correlated.end(); ++i) {
    const TargetGroup& group = i->second;
    if (!group.debug->should_generate())
      continue;  // Skip non-generated ones.
    if (group.debug->item()->AsTarget()->external())
      continue;  // Skip external ones.
    if (group.debug->item()->AsTarget()->gyp_file().is_null())
      continue;  // Skip ones without GYP files.

    if (!EnsureTargetsMatch(group, err))
      return std::make_pair(0, 0);

    target_count++;
    grouped_targets[
            helper.GetGypFileForTarget(group.debug->item()->AsTarget(), err)]
        .push_back(group);
    if (err->has_error())
      return std::make_pair(0, 0);
  }

  // Extract the toolchain for the debug targets.
  const Toolchain* debug_toolchain = NULL;
  if (!grouped_targets.empty()) {
    debug_toolchain = debug_setup->builder()->GetToolchain(
        grouped_targets.begin()->second[0].debug->item()->settings()->
        default_toolchain_label());
  }

  // Write each GYP file.
  for (GroupedTargetsMap::iterator i = grouped_targets.begin();
       i != grouped_targets.end(); ++i) {
    GypTargetWriter::WriteFile(i->first, i->second, debug_toolchain, err);
    if (err->has_error())
      return std::make_pair(0, 0);
  }

  return std::make_pair(target_count,
                        static_cast<int>(grouped_targets.size()));
}

}  // namespace

// Suppress output on success.
const char kSwitchQuiet[] = "q";

const char kGyp[] = "gyp";
const char kGyp_HelpShort[] =
    "gyp: Make GYP files from GN.";
const char kGyp_Help[] =
    "gyp: Make GYP files from GN.\n"
    "\n"
    "  This command will generate GYP files from GN sources. You can then run\n"
    "  GYP over the result to produce a build. Native GYP targets can depend\n"
    "  on any GN target except source sets. GN targets can depend on native\n"
    "  GYP targets, but all/direct dependent settings will NOT be pushed\n"
    "  across the boundary.\n"
    "\n"
    "  To make this work you first need to manually run GN, then GYP, then\n"
    "  do the build. Because GN doesn't generate the final .ninja files,\n"
    "  there will be no rules to regenerate the .ninja files if the inputs\n"
    "  change, so you will have to manually repeat these steps each time\n"
    "  something changes:\n"
    "\n"
    "    out/Debug/gn gyp\n"
    "    python build/gyp_chromiunm\n"
    "    ninja -C out/Debug foo_target\n"
    "\n"
    "  Two variables are used to control how a target relates to GYP:\n"
    "\n"
    "  - \"external != true\" and \"gyp_file\" is set: This target will be\n"
    "    written to the named GYP file in the source tree (not restricted to\n"
    "    an output or generated files directory).\n"
    "\n"
    "  - \"external == true\" and \"gyp_file\" is set: The target will not\n"
    "    be written to a GYP file. But other targets being written to GYP\n"
    "    files can depend on it, and they will reference the given GYP file\n"
    "    name for GYP to use. This allows you to specify how GN->GYP\n"
    "    dependencies and named, and provides a place to manually set the\n"
    "    dependent configs from GYP to GN.\n"
    "\n"
    "  - \"gyp_file\" is unset: Like the previous case, but if a GN target is\n"
    "    being written to a GYP file that depends on this one, the default\n"
    "    GYP file name will be assumed. The default name will match the name\n"
    "    of the current directory, so \"//foo/bar:baz\" would be\n"
    "    \"<(DEPTH)/foo/bar/bar.gyp:baz\".\n"
    "\n"
    "Switches\n"
    "  --gyp_vars\n"
    "      The GYP variables converted to a GN-style string lookup.\n"
    "      For example:\n"
    "      --gyp_vars=\"component=\\\"shared_library\\\" use_aura=\\\"1\\\"\"\n"
    "\n"
    "Example:\n"
    "  # This target is assumed to be in the GYP build in the file\n"
    "  # \"foo/foo.gyp\". This declaration tells GN where to find the GYP\n"
    "  # equivalent, and gives it some direct dependent settings that targets\n"
    "  # depending on it should receive (since these don't flow from GYP to\n"
    "  # GN-generated targets).\n"
    "  shared_library(\"gyp_target\") {\n"
    "    gyp_file = \"//foo/foo.gyp\"\n"
    "    external = true\n"
    "    direct_dependen_configs = [ \":gyp_target_config\" ]\n"
    "  }\n"
    "\n"
    "  executable(\"my_app\") {\n"
    "    deps = [ \":gyp_target\" ]\n"
    "    gyp_file = \"//foo/myapp.gyp\"\n"
    "    sources = ...\n"
    "  }\n";

int RunGyp(const std::vector<std::string>& args) {
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();

  base::TimeTicks begin_time = base::TimeTicks::Now();

  // Deliberately leaked to avoid expensive process teardown.
  Setup* setup_debug = new Setup;
  if (!setup_debug->DoSetup())
    return 1;
  const char kIsDebug[] = "is_debug";

  // Make a release build based on the debug one. We use a new directory for
  // the build output so that they don't stomp on each other.
  DependentSetup* setup_release = new DependentSetup(setup_debug);
  setup_release->build_settings().build_args().AddArgOverride(
      kIsDebug, Value(NULL, false));
  setup_release->build_settings().SetBuildDir(
      SourceDir(setup_release->build_settings().build_dir().value() +
                "gn_release.tmp/"));

  // Host build.
  DependentSetup* setup_host_debug = NULL;
  DependentSetup* setup_host_release = NULL;
  // TODO(brettw) hook up host build.

  // 64-bit build (Windows only).
  DependentSetup* setup_debug64 = NULL;
  DependentSetup* setup_release64 = NULL;
#if defined(OS_WIN)
  static const char kForceWin64[] = "force_win64";
  setup_debug64 = new DependentSetup(setup_debug);
  setup_debug64->build_settings().build_args().AddArgOverride(
      kForceWin64, Value(NULL, true));
  setup_debug64->build_settings().SetBuildDir(
      SourceDir(setup_release->build_settings().build_dir().value() +
                "gn_debug64.tmp/"));

  setup_release64 = new DependentSetup(setup_release);
  setup_release64->build_settings().build_args().AddArgOverride(
      kForceWin64, Value(NULL, true));
  setup_release64->build_settings().SetBuildDir(
      SourceDir(setup_release->build_settings().build_dir().value() +
                "gn_release64.tmp/"));
#endif

  // Run all the builds in parellel.
  setup_release->RunPreMessageLoop();
  if (setup_host_debug && setup_host_release) {
    setup_host_debug->RunPreMessageLoop();
    setup_host_release->RunPreMessageLoop();
  }
  if (setup_debug64 && setup_release64) {
    setup_debug64->RunPreMessageLoop();
    setup_release64->RunPreMessageLoop();
  }

  if (!setup_debug->Run())
    return 1;

  if (!setup_release->RunPostMessageLoop())
    return 1;
  if (setup_host_debug && !setup_host_debug->RunPostMessageLoop())
    return 1;
  if (setup_host_release && !setup_host_release->RunPostMessageLoop())
    return 1;
  if (setup_debug64 && !setup_debug64->RunPostMessageLoop())
    return 1;
  if (setup_release64 && !setup_release64->RunPostMessageLoop())
    return 1;

  Err err;
  std::pair<int, int> counts =
      WriteGypFiles(setup_debug, setup_release,
                    setup_host_debug, setup_host_release,
                    setup_debug64, setup_release64,
                    &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return 1;
  }

  // Timing info.
  base::TimeTicks end_time = base::TimeTicks::Now();
  if (!cmdline->HasSwitch(kSwitchQuiet)) {
    OutputString("Done. ", DECORATION_GREEN);

    std::string stats = "Wrote " +
        base::IntToString(counts.first) + " targets to " +
        base::IntToString(counts.second) + " GYP files read from " +
        base::IntToString(
            setup_debug->scheduler().input_file_manager()->GetInputFileCount())
        + " GN files in " +
        base::IntToString((end_time - begin_time).InMilliseconds()) + "ms\n";

    OutputString(stats);
  }

  return 0;
}

}  // namespace commands
