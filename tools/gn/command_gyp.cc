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
#include "tools/gn/setup.h"
#include "tools/gn/source_file.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"

namespace commands {

namespace {

typedef GypTargetWriter::TargetGroup TargetGroup;
typedef std::map<Label, TargetGroup> CorrelatedTargetsMap;
typedef std::map<SourceFile, std::vector<TargetGroup> > GroupedTargetsMap;
typedef std::map<std::string, std::string> StringStringMap;

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
void CorrelateTargets(const std::vector<const BuilderRecord*>& debug_targets,
                      const std::vector<const BuilderRecord*>& release_targets,
                      CorrelatedTargetsMap* correlated) {
  for (size_t i = 0; i < debug_targets.size(); i++) {
    const BuilderRecord* record = debug_targets[i];
    (*correlated)[record->label()].debug = record;
  }
  for (size_t i = 0; i < release_targets.size(); i++) {
    const BuilderRecord* record = release_targets[i];
    (*correlated)[record->label()].release = record;
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

// Python uses shlex.split, which we partially emulate here.
//
// Advances to the next "word" in a GYP_DEFINES entry. This is something
// separated by whitespace or '='. We allow backslash escaping and quoting.
// The return value will be the index into the array immediately following the
// word, and the contents of the word will be placed into |*word|.
size_t GetNextGypDefinesWord(const std::string& defines,
                             size_t cur,
                             std::string* word) {
  size_t i = cur;
  bool is_quoted = false;
  if (cur < defines.size() && defines[cur] == '"') {
    i++;
    is_quoted = true;
  }

  for (; i < defines.size(); i++) {
    // Handle certain escape sequences: \\, \", \<space>.
    if (defines[i] == '\\' && i < defines.size() - 1 &&
        (defines[i + 1] == '\\' ||
         defines[i + 1] == ' ' ||
         defines[i + 1] == '=' ||
         defines[i + 1] == '"')) {
      i++;
      word->push_back(defines[i]);
      continue;
    }
    if (is_quoted && defines[i] == '"') {
      // Got to the end of the quoted sequence.
      return i + 1;
    }
    if (!is_quoted && (defines[i] == ' ' || defines[i] == '=')) {
      return i;
    }
    word->push_back(defines[i]);
  }
  return i;
}

// Advances to the beginning of the next word, or the size of the string if
// the end was encountered.
size_t AdvanceToNextGypDefinesWord(const std::string& defines, size_t cur) {
  while (cur < defines.size() && defines[cur] == ' ')
    cur++;
  return cur;
}

// The GYP defines looks like:
//   component=shared_library
//   component=shared_library foo=1
//   component=shared_library foo=1 windows_sdk_dir="C:\Program Files\..."
StringStringMap GetGypDefines() {
  StringStringMap result;

  scoped_ptr<base::Environment> env(base::Environment::Create());
  std::string defines;
  if (!env->GetVar("GYP_DEFINES", &defines) || defines.empty())
    return result;

  size_t cur = 0;
  while (cur < defines.size()) {
    std::string key;
    cur = AdvanceToNextGypDefinesWord(defines, cur);
    cur = GetNextGypDefinesWord(defines, cur, &key);

    // The words should be separated by an equals.
    cur = AdvanceToNextGypDefinesWord(defines, cur);
    if (cur == defines.size())
      break;
    if (defines[cur] != '=')
      continue;
    cur++;  // Skip over '='.

    std::string value;
    cur = AdvanceToNextGypDefinesWord(defines, cur);
    cur = GetNextGypDefinesWord(defines, cur, &value);

    result[key] = value;
  }

  return result;
}

// Returns a set of args from known GYP define values.
Scope::KeyValueMap GetArgsFromGypDefines() {
  StringStringMap gyp_defines = GetGypDefines();

  Scope::KeyValueMap result;

  static const char kIsComponentBuild[] = "is_component_build";
  if (gyp_defines["component"] == "shared_library") {
    result[kIsComponentBuild] = Value(NULL, true);
  } else {
    result[kIsComponentBuild] = Value(NULL, false);
  }

  // Windows SDK path. GYP and the GN build use the same name.
  static const char kWinSdkPath[] = "windows_sdk_path";
  if (!gyp_defines[kWinSdkPath].empty())
    result[kWinSdkPath] = Value(NULL, gyp_defines[kWinSdkPath]);

  return result;
}

// Returns the (number of targets, number of GYP files).
std::pair<int, int> WriteGypFiles(CommonSetup* debug_setup,
                                  CommonSetup* release_setup,
                                  Err* err) {
  // Group all targets by output GYP file name.
  std::vector<const BuilderRecord*> debug_targets =
      GetAllResolvedTargetRecords(debug_setup->builder());
  std::vector<const BuilderRecord*> release_targets =
      GetAllResolvedTargetRecords(release_setup->builder());

  // Match up the debug and release version of each target by label.
  CorrelatedTargetsMap correlated;
  CorrelateTargets(debug_targets, release_targets, &correlated);

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

  // Write each GYP file.
  for (GroupedTargetsMap::iterator i = grouped_targets.begin();
       i != grouped_targets.end(); ++i) {
    GypTargetWriter::WriteFile(i->first, i->second, err);
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
  setup_debug->build_settings().build_args().AddArgOverrides(
      GetArgsFromGypDefines());
  setup_debug->build_settings().build_args().AddArgOverride(
      kIsDebug, Value(NULL, true));

  // Make a release build based on the debug one. We use a new directory for
  // the build output so that they don't stomp on each other.
  DependentSetup* setup_release = new DependentSetup(*setup_debug);
  setup_release->build_settings().build_args().AddArgOverride(
      kIsDebug, Value(NULL, false));
  setup_release->build_settings().SetBuildDir(
      SourceDir(setup_release->build_settings().build_dir().value() +
                "gn_release.tmp/"));

  // Run both debug and release builds in parallel.
  setup_release->RunPreMessageLoop();
  if (!setup_debug->Run())
    return 1;
  if (!setup_release->RunPostMessageLoop())
    return 1;

  Err err;
  std::pair<int, int> counts = WriteGypFiles(setup_debug, setup_release, &err);
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
