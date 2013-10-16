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
#include "tools/gn/item_node.h"
#include "tools/gn/location.h"
#include "tools/gn/setup.h"
#include "tools/gn/source_file.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/target.h"

namespace commands {

namespace {

typedef GypTargetWriter::TargetPair TargetPair;
typedef std::map<Label, TargetPair> CorrelatedTargetsMap;
typedef std::map<SourceFile, std::vector<TargetPair> > GroupedTargetsMap;
typedef std::map<std::string, std::string> StringStringMap;

// Groups targets sharing the same label between debug and release.
void CorrelateTargets(const std::vector<const Target*>& debug_targets,
                      const std::vector<const Target*>& release_targets,
                      CorrelatedTargetsMap* correlated) {
  for (size_t i = 0; i < debug_targets.size(); i++) {
    const Target* target = debug_targets[i];
    (*correlated)[target->label()].debug = target;
  }
  for (size_t i = 0; i < release_targets.size(); i++) {
    const Target* target = release_targets[i];
    (*correlated)[target->label()].release = target;
  }
}

// Verifies that both debug and release variants match. They can differ only
// by flags.
bool EnsureTargetsMatch(const TargetPair& pair, Err* err) {
  // Check that both debug and release made this target.
  if (!pair.debug || !pair.release) {
    const Target* non_null_one = pair.debug ? pair.debug : pair.release;
    *err = Err(Location(), "The debug and release builds did not both generate "
        "a target with the name\n" +
        non_null_one->label().GetUserVisibleName(true));
    return false;
  }

  // Check the flags that determine if and where we write the GYP file.
  if (pair.debug->item_node()->should_generate() !=
          pair.release->item_node()->should_generate() ||
      pair.debug->external() != pair.release->external() ||
      pair.debug->gyp_file() != pair.release->gyp_file()) {
    *err = Err(Location(), "The metadata for the target\n" +
        pair.debug->label().GetUserVisibleName(true) +
        "\ndoesn't match between the debug and release builds.");
    return false;
  }

  // Check that the sources match.
  if (pair.debug->sources().size() != pair.release->sources().size()) {
    *err = Err(Location(), "The source file count for the target\n" +
        pair.debug->label().GetUserVisibleName(true) +
        "\ndoesn't have the same number of files between the debug and "
        "release builds.");
    return false;
  }
  for (size_t i = 0; i < pair.debug->sources().size(); i++) {
    if (pair.debug->sources()[i] != pair.release->sources()[i]) {
      *err = Err(Location(), "The debug and release version of the target \n" +
          pair.debug->label().GetUserVisibleName(true) +
          "\ndon't agree on the file\n" +
          pair.debug->sources()[i].value());
      return false;
    }
  }

  // Check that the deps match.
  if (pair.debug->deps().size() != pair.release->deps().size()) {
    *err = Err(Location(), "The source file count for the target\n" +
        pair.debug->label().GetUserVisibleName(true) +
        "\ndoesn't have the same number of deps between the debug and "
        "release builds.");
    return false;
  }
  for (size_t i = 0; i < pair.debug->deps().size(); i++) {
    if (pair.debug->deps()[i]->label() != pair.release->deps()[i]->label()) {
      *err = Err(Location(), "The debug and release version of the target \n" +
          pair.debug->label().GetUserVisibleName(true) +
          "\ndon't agree on the dep\n" +
          pair.debug->deps()[i]->label().GetUserVisibleName(true));
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

  if (gyp_defines["component"] == "shared_library") {
    result["is_component_build"] = Value(NULL, true);
  } else {
    result["is_component_build"] = Value(NULL, false);
  }

  // Windows SDK path. GYP and the GN build use the same name.
  const char kWinSdkPath[] = "windows_sdk_path";
  if (gyp_defines[kWinSdkPath].empty())
    result[kWinSdkPath] = Value(NULL, gyp_defines[kWinSdkPath]);

  return result;
}

// Returns the number of targets, number of GYP files.
std::pair<int, int> WriteGypFiles(
    const BuildSettings& debug_settings,
    const BuildSettings& release_settings,
    Err* err) {
  // Group all targets by output GYP file name.
  std::vector<const Target*> debug_targets;
  std::vector<const Target*> release_targets;
  debug_settings.target_manager().GetAllTargets(&debug_targets);
  release_settings.target_manager().GetAllTargets(&release_targets);

  // Match up the debug and release version of each target by label.
  CorrelatedTargetsMap correlated;
  CorrelateTargets(debug_targets, release_targets, &correlated);

  GypHelper helper;
  GroupedTargetsMap grouped_targets;
  int target_count = 0;
  for (CorrelatedTargetsMap::iterator i = correlated.begin();
       i != correlated.end(); ++i) {
    const TargetPair& pair = i->second;
    if (!EnsureTargetsMatch(pair, err))
      return std::make_pair(0, 0);

    if (!pair.debug->item_node()->should_generate())
      continue;  // Skip non-generated ones.
    if (pair.debug->external())
      continue;  // Skip external ones.
    if (pair.debug->gyp_file().is_null())
      continue;  // Skip ones without GYP files.

    target_count++;
    grouped_targets[helper.GetGypFileForTarget(pair.debug, err)].push_back(
        pair);
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
const char kGyp_Help[] = "Doooooom.\n";

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
  std::pair<int, int> counts = WriteGypFiles(setup_debug->build_settings(),
                                             setup_release->build_settings(),
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
