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
#include "base/timer/elapsed_timer.h"
#include "build/build_config.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/commands.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
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

struct Setups {
  Setups()
      : debug(NULL),
        release(NULL),
        host_debug(NULL),
        host_release(NULL),
        debug64(NULL),
        release64(NULL),
        xcode_debug(NULL),
        xcode_release(NULL),
        xcode_host_debug(NULL),
        xcode_host_release(NULL) {
  }

  Setup* debug;
  DependentSetup* release;
  DependentSetup* host_debug;
  DependentSetup* host_release;
  DependentSetup* debug64;
  DependentSetup* release64;
  DependentSetup* xcode_debug;
  DependentSetup* xcode_release;
  DependentSetup* xcode_host_debug;
  DependentSetup* xcode_host_release;
};

struct TargetVectors {
  RecordVector debug;
  RecordVector release;
  RecordVector host_debug;
  RecordVector host_release;
  RecordVector debug64;
  RecordVector release64;
  RecordVector xcode_debug;
  RecordVector xcode_release;
  RecordVector xcode_host_debug;
  RecordVector xcode_host_release;
};

// This function appends a suffix to the given source directory name. We append
// a suffix to the last directory component rather than adding a new level so
// that the relative location of the files don't change (i.e. a file
// relative to the build dir might be "../../foo/bar.cc") and we want these to
// be the same in all builds, and in particular the GYP build directories.
SourceDir AppendDirSuffix(const SourceDir& base, const std::string& suffix) {
  return SourceDir(DirectoryWithNoLastSlash(base) + suffix + "/");
}

// We need a "host" build when targeting a device (iOS, Android, ChromeOS). The
// host build will correspond to the the system doing the building.
bool NeedsHostBuild(CommonSetup* setup) {
  Args& args = setup->build_settings().build_args();
  const Value* os_override = args.GetArgOverride("os");
  if (!os_override)
    return false;  // Target OS is the default, which is always the host.
  if (os_override->type() != Value::STRING)
    return false;  // Build will likely fail later.
  return os_override->string_value() == "android" ||
         os_override->string_value() == "ios" ||
         os_override->string_value() == "chromeos";
}

void SetupHostBuildParams(CommonSetup* setup) {
  // Re-override the target OS to the default of the current system.
#if defined(OS_WIN)
  const char kDefaultOs[] = "win";
#elif defined(OS_MACOSX)
  const char kDefaultOs[] = "mac";
#elif defined(OS_LINUX)
  const char kDefaultOs[] = "linux";
#endif
  setup->build_settings().build_args().AddArgOverride(
      "os", Value(NULL, kDefaultOs));
}

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

void CorrelateRecordVector(const RecordVector& records,
                           CorrelatedTargetsMap* correlated,
                           const BuilderRecord* TargetGroup::* record_ptr) {
  for (size_t i = 0; i < records.size(); i++) {
    const BuilderRecord* record = records[i];
    (*correlated)[record->label().GetWithNoToolchain()].*record_ptr = record;
  }
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
void CorrelateTargets(const TargetVectors& targets,
                      CorrelatedTargetsMap* correlated) {
  // Normal.
  CorrelateRecordVector(
      targets.debug, correlated, &TargetGroup::debug);
  CorrelateRecordVector(
      targets.release, correlated, &TargetGroup::release);

  // Host build.
  CorrelateRecordVector(
      targets.host_debug, correlated, &TargetGroup::host_debug);
  CorrelateRecordVector(
      targets.host_release, correlated, &TargetGroup::host_release);

  // 64-bit build.
  CorrelateRecordVector(
      targets.debug64, correlated, &TargetGroup::debug64);
  CorrelateRecordVector(
      targets.release64, correlated, &TargetGroup::release64);

  // XCode build.
  CorrelateRecordVector(
      targets.xcode_debug, correlated, &TargetGroup::xcode_debug);
  CorrelateRecordVector(
      targets.xcode_release, correlated, &TargetGroup::xcode_release);
  CorrelateRecordVector(
      targets.xcode_host_debug, correlated, &TargetGroup::xcode_host_debug);
  CorrelateRecordVector(
      targets.xcode_host_release, correlated, &TargetGroup::xcode_host_release);
}

// Verifies that both debug and release variants match. They can differ only
// by flags.
bool EnsureTargetsMatch(const TargetGroup& group, Err* err) {
  if (!group.debug && !group.release)
    return true;

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
std::pair<int, int> WriteGypFiles(Setups& setups, Err* err) {
  TargetVectors targets;

  // Group all targets by output GYP file name.
  targets.debug = GetAllResolvedTargetRecords(setups.debug->builder());
  targets.release = GetAllResolvedTargetRecords(setups.release->builder());

  // Host build is optional.
  if (setups.host_debug && setups.host_release) {
    targets.host_debug =
        GetAllResolvedTargetRecords(setups.host_debug->builder());
    targets.host_release =
        GetAllResolvedTargetRecords(setups.host_release->builder());
  }

  // 64-bit build is optional.
  if (setups.debug64 && setups.release64) {
    targets.debug64 =
        GetAllResolvedTargetRecords(setups.debug64->builder());
    targets.release64 =
        GetAllResolvedTargetRecords(setups.release64->builder());
  }

  // Xcode build is optional.
  if (setups.xcode_debug && setups.xcode_release) {
    targets.xcode_debug =
        GetAllResolvedTargetRecords(setups.xcode_debug->builder());
    targets.xcode_release =
        GetAllResolvedTargetRecords(setups.xcode_release->builder());
  }
  if (setups.xcode_host_debug && setups.xcode_host_release) {
    targets.xcode_host_debug =
        GetAllResolvedTargetRecords(setups.xcode_host_debug->builder());
    targets.xcode_host_release =
        GetAllResolvedTargetRecords(setups.xcode_host_release->builder());
  }

  // Match up the debug and release version of each target by label.
  CorrelatedTargetsMap correlated;
  CorrelateTargets(targets, &correlated);

  GypHelper helper;
  GroupedTargetsMap grouped_targets;
  int target_count = 0;
  for (CorrelatedTargetsMap::iterator i = correlated.begin();
       i != correlated.end(); ++i) {
    const TargetGroup& group = i->second;
    if (!group.get()->should_generate())
      continue;  // Skip non-generated ones.
    if (group.get()->item()->AsTarget()->external())
      continue;  // Skip external ones.
    if (group.get()->item()->AsTarget()->gyp_file().is_null())
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
    debug_toolchain = setups.debug->builder()->GetToolchain(
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
  base::ElapsedTimer timer;
  Setups setups;

  // Deliberately leaked to avoid expensive process teardown.
  setups.debug = new Setup;
  if (!setups.debug->DoSetup())
    return 1;
  const char kIsDebug[] = "is_debug";

  SourceDir base_build_dir = setups.debug->build_settings().build_dir();
  setups.debug->build_settings().SetBuildDir(
      AppendDirSuffix(base_build_dir, ".Debug"));

  // Make a release build based on the debug one. We use a new directory for
  // the build output so that they don't stomp on each other.
  setups.release = new DependentSetup(setups.debug);
  setups.release->build_settings().build_args().AddArgOverride(
      kIsDebug, Value(NULL, false));
  setups.release->build_settings().SetBuildDir(
      AppendDirSuffix(base_build_dir, ".Release"));

  // Host build.
  if (NeedsHostBuild(setups.debug)) {
    setups.host_debug = new DependentSetup(setups.debug);
    setups.host_debug->build_settings().SetBuildDir(
        AppendDirSuffix(base_build_dir, ".HostDebug"));
    SetupHostBuildParams(setups.host_debug);

    setups.host_release = new DependentSetup(setups.release);
    setups.host_release->build_settings().SetBuildDir(
        AppendDirSuffix(base_build_dir, ".HostRelease"));
    SetupHostBuildParams(setups.host_release);
  }

  // 64-bit build (Windows only).
#if defined(OS_WIN)
  static const char kForceWin64[] = "force_win64";
  setups.debug64 = new DependentSetup(setups.debug);
  setups.debug64->build_settings().build_args().AddArgOverride(
      kForceWin64, Value(NULL, true));
  setups.debug64->build_settings().SetBuildDir(
      AppendDirSuffix(base_build_dir, ".Debug64"));

  setups.release64 = new DependentSetup(setups.release);
  setups.release64->build_settings().build_args().AddArgOverride(
      kForceWin64, Value(NULL, true));
  setups.release64->build_settings().SetBuildDir(
      AppendDirSuffix(base_build_dir, ".Release64"));
#endif

  // XCode build (Mac only).
#if defined(OS_MACOSX)
  static const char kGypXCode[] = "is_gyp_xcode_generator";
  setups.xcode_debug = new DependentSetup(setups.debug);
  setups.xcode_debug->build_settings().build_args().AddArgOverride(
      kGypXCode, Value(NULL, true));
  setups.xcode_debug->build_settings().SetBuildDir(
      AppendDirSuffix(base_build_dir, ".XCodeDebug"));

  setups.xcode_release = new DependentSetup(setups.release);
  setups.xcode_release->build_settings().build_args().AddArgOverride(
      kGypXCode, Value(NULL, true));
  setups.xcode_release->build_settings().SetBuildDir(
      AppendDirSuffix(base_build_dir, ".XCodeRelease"));

  if (NeedsHostBuild(setups.debug)) {
    setups.xcode_host_debug = new DependentSetup(setups.xcode_debug);
    setups.xcode_host_debug->build_settings().SetBuildDir(
        AppendDirSuffix(base_build_dir, ".XCodeHostDebug"));
    SetupHostBuildParams(setups.xcode_host_debug);

    setups.xcode_host_release = new DependentSetup(setups.xcode_release);
    setups.xcode_host_release->build_settings().SetBuildDir(
        AppendDirSuffix(base_build_dir, ".XCodeHostRelease"));
    SetupHostBuildParams(setups.xcode_host_release);
  }
#endif

  // Run all the builds in parellel.
  setups.release->RunPreMessageLoop();
  if (setups.host_debug && setups.host_release) {
    setups.host_debug->RunPreMessageLoop();
    setups.host_release->RunPreMessageLoop();
  }
  if (setups.debug64 && setups.release64) {
    setups.debug64->RunPreMessageLoop();
    setups.release64->RunPreMessageLoop();
  }
  if (setups.xcode_debug && setups.xcode_release) {
    setups.xcode_debug->RunPreMessageLoop();
    setups.xcode_release->RunPreMessageLoop();
  }
  if (setups.xcode_host_debug && setups.xcode_host_release) {
    setups.xcode_host_debug->RunPreMessageLoop();
    setups.xcode_host_release->RunPreMessageLoop();
  }

  if (!setups.debug->Run())
    return 1;

  if (!setups.release->RunPostMessageLoop())
    return 1;
  if (setups.host_debug && !setups.host_debug->RunPostMessageLoop())
    return 1;
  if (setups.host_release && !setups.host_release->RunPostMessageLoop())
    return 1;
  if (setups.debug64 && !setups.debug64->RunPostMessageLoop())
    return 1;
  if (setups.release64 && !setups.release64->RunPostMessageLoop())
    return 1;
  if (setups.xcode_debug && !setups.xcode_debug->RunPostMessageLoop())
    return 1;
  if (setups.xcode_release && !setups.xcode_release->RunPostMessageLoop())
    return 1;
  if (setups.xcode_host_debug &&
      !setups.xcode_host_debug->RunPostMessageLoop())
    return 1;
  if (setups.xcode_host_release &&
      !setups.xcode_host_release->RunPostMessageLoop())
    return 1;

  Err err;
  std::pair<int, int> counts = WriteGypFiles(setups, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return 1;
  }

  base::TimeDelta elapsed_time = timer.Elapsed();

  if (!CommandLine::ForCurrentProcess()->HasSwitch(kSwitchQuiet)) {
    OutputString("Done. ", DECORATION_GREEN);

    std::string stats = "Wrote " +
        base::IntToString(counts.first) + " targets to " +
        base::IntToString(counts.second) + " GYP files read from " +
        base::IntToString(
            setups.debug->scheduler().input_file_manager()->GetInputFileCount())
        + " GN files in " +
        base::IntToString(elapsed_time.InMilliseconds()) + "ms\n";

    OutputString(stats);
  }

  return 0;
}

}  // namespace commands
