// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_build_writer.h"

#include <fstream>
#include <map>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file_manager.h"
#include "tools/gn/ninja_utils.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/switches.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {

std::string GetSelfInvocationCommand(const BuildSettings* build_settings) {
  base::FilePath executable;
  PathService::Get(base::FILE_EXE, &executable);

  base::CommandLine cmdline(executable.NormalizePathSeparatorsTo('/'));
  cmdline.AppendArg("gen");
  cmdline.AppendArg(build_settings->build_dir().value());
  cmdline.AppendSwitchPath(std::string("--") + switches::kRoot,
                           build_settings->root_path());
  // Successful automatic invocations shouldn't print output.
  cmdline.AppendSwitch(std::string("-") + switches::kQuiet);

  EscapeOptions escape_shell;
  escape_shell.mode = ESCAPE_NINJA_COMMAND;
#if defined(OS_WIN)
  // The command line code quoting varies by platform. We have one string,
  // possibly with spaces, that we want to quote. The Windows command line
  // quotes again, so we don't want quoting. The Posix one doesn't.
  escape_shell.inhibit_quoting = true;
#endif

  const base::CommandLine& our_cmdline =
      *base::CommandLine::ForCurrentProcess();
  const base::CommandLine::SwitchMap& switches = our_cmdline.GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator i = switches.begin();
       i != switches.end(); ++i) {
    // Only write arguments we haven't already written. Always skip "args"
    // since those will have been written to the file and will be used
    // implicitly in the future. Keeping --args would mean changes to the file
    // would be ignored.
    if (i->first != switches::kQuiet &&
        i->first != switches::kRoot &&
        i->first != switches::kArgs) {
      std::string escaped_value =
          EscapeString(FilePathToUTF8(i->second), escape_shell, nullptr);
      cmdline.AppendSwitchASCII(i->first, escaped_value);
    }
  }

#if defined(OS_WIN)
  return base::WideToUTF8(cmdline.GetCommandLineString());
#else
  return cmdline.GetCommandLineString();
#endif
}

OutputFile GetTargetOutputFile(const Target* target) {
  OutputFile result(target->dependency_output_file());

  // The output files may have leading "./" so normalize those away.
  NormalizePath(&result.value());
  return result;
}

// Given an output that appears more than once, generates an error message
// that describes the problem and which targets generate it.
Err GetDuplicateOutputError(const std::vector<const Target*>& all_targets,
                            const OutputFile& bad_output) {
  std::vector<const Target*> matches;
  for (const Target* target : all_targets) {
    if (GetTargetOutputFile(target) == bad_output)
      matches.push_back(target);
  }

  // There should always be at least two targets generating this file for this
  // function to be called in the first place.
  DCHECK(matches.size() >= 2);
  std::string matches_string;
  for (const Target* target : matches)
    matches_string += "  " + target->label().GetUserVisibleName(false) + "\n";

  Err result(matches[0]->defined_from(), "Duplicate output file.",
      "Two or more targets generate the same output:\n  " +
      bad_output.value() + "\n"
      "This is normally the result of either overriding the output name or\n"
      "having two shared libraries or executables in different directories\n"
      "with the same name (since all such targets will be written to the root\n"
      "output directory).\n\nCollisions:\n" + matches_string);
  for (size_t i = 1; i < matches.size(); i++)
    result.AppendSubErr(Err(matches[i]->defined_from(), "Collision."));
  return result;
}

}  // namespace

NinjaBuildWriter::NinjaBuildWriter(
    const BuildSettings* build_settings,
    const std::vector<const Settings*>& all_settings,
    const Toolchain* default_toolchain,
    const std::vector<const Target*>& default_toolchain_targets,
    std::ostream& out,
    std::ostream& dep_out)
    : build_settings_(build_settings),
      all_settings_(all_settings),
      default_toolchain_(default_toolchain),
      default_toolchain_targets_(default_toolchain_targets),
      out_(out),
      dep_out_(dep_out),
      path_output_(build_settings->build_dir(),
                   build_settings->root_path_utf8(), ESCAPE_NINJA) {
}

NinjaBuildWriter::~NinjaBuildWriter() {
}

bool NinjaBuildWriter::Run(Err* err) {
  WriteNinjaRules();
  WriteLinkPool();
  WriteSubninjas();
  return WritePhonyAndAllRules(err);
}

// static
bool NinjaBuildWriter::RunAndWriteFile(
    const BuildSettings* build_settings,
    const std::vector<const Settings*>& all_settings,
    const Toolchain* default_toolchain,
    const std::vector<const Target*>& default_toolchain_targets,
    Err* err) {
  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, "build.ninja");

  base::FilePath ninja_file(build_settings->GetFullPath(
      SourceFile(build_settings->build_dir().value() + "build.ninja")));
  base::CreateDirectory(ninja_file.DirName());

  std::ofstream file;
  file.open(FilePathToUTF8(ninja_file).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail()) {
    *err = Err(Location(), "Couldn't open build.ninja for writing");
    return false;
  }

  std::ofstream depfile;
  depfile.open((FilePathToUTF8(ninja_file) + ".d").c_str(),
               std::ios_base::out | std::ios_base::binary);
  if (depfile.fail()) {
    *err = Err(Location(), "Couldn't open depfile for writing");
    return false;
  }

  NinjaBuildWriter gen(build_settings, all_settings, default_toolchain,
                       default_toolchain_targets, file, depfile);
  return gen.Run(err);
}

void NinjaBuildWriter::WriteNinjaRules() {
  out_ << "rule gn\n";
  out_ << "  command = " << GetSelfInvocationCommand(build_settings_) << "\n";
  out_ << "  description = Regenerating ninja files\n\n";

  // This rule will regenerate the ninja files when any input file has changed.
  out_ << "build build.ninja: gn\n"
       << "  generator = 1\n"
       << "  depfile = build.ninja.d\n";

  // Input build files. These go in the ".d" file. If we write them as
  // dependencies in the .ninja file itself, ninja will expect the files to
  // exist and will error if they don't. When files are listed in a depfile,
  // missing files are ignored.
  dep_out_ << "build.ninja:";
  std::vector<base::FilePath> input_files;
  g_scheduler->input_file_manager()->GetAllPhysicalInputFileNames(&input_files);
  for (const auto& input_file : input_files)
    dep_out_ << " " << FilePathToUTF8(input_file);

  // Other files read by the build.
  std::vector<base::FilePath> other_files = g_scheduler->GetGenDependencies();
  for (const auto& other_file : other_files)
    dep_out_ << " " << FilePathToUTF8(other_file);

  out_ << std::endl;
}

void NinjaBuildWriter::WriteLinkPool() {
  out_ << "pool link_pool\n"
       << "  depth = " << default_toolchain_->concurrent_links() << std::endl
       << std::endl;
}

void NinjaBuildWriter::WriteSubninjas() {
  for (const auto& elem : all_settings_) {
    out_ << "subninja ";
    path_output_.WriteFile(out_, GetNinjaFileForToolchain(elem));
    out_ << std::endl;
  }
  out_ << std::endl;
}

bool NinjaBuildWriter::WritePhonyAndAllRules(Err* err) {
  std::string all_rules;

  // Track rules as we generate them so we don't accidentally write a phony
  // rule that collides with something else.
  // GN internally generates an "all" target, so don't duplicate it.
  std::set<std::string> written_rules;
  written_rules.insert("all");

  // Write phony rules for all uniquely-named targets in the default toolchain.
  // Don't do other toolchains or we'll get naming conflicts, and if the name
  // isn't unique, also skip it. The exception is for the toplevel targets
  // which we also find.
  std::map<std::string, int> small_name_count;
  std::map<std::string, int> exe_count;
  std::vector<const Target*> toplevel_targets;
  base::hash_set<std::string> target_files;
  for (const auto& target : default_toolchain_targets_) {
    const Label& label = target->label();
    small_name_count[label.name()]++;

    // Look for targets with a name of the form
    //   dir = "//foo/", name = "foo"
    // i.e. where the target name matches the top level directory. We will
    // always write phony rules for these even if there is another target with
    // the same short name.
    const std::string& dir_string = label.dir().value();
    if (dir_string.size() == label.name().size() + 3 &&  // Size matches.
        dir_string[0] == '/' && dir_string[1] == '/' &&  // "//" at beginning.
        dir_string[dir_string.size() - 1] == '/' &&  // "/" at end.
        dir_string.compare(2, label.name().size(), label.name()) == 0)
      toplevel_targets.push_back(target);

    // Look for executables; later we will generate phony rules for them
    // even if there are non-executable targets with the same name.
    if (target->output_type() == Target::EXECUTABLE)
      exe_count[label.name()]++;

    // Add the files to the list of generated targets so we don't write phony
    // rules that collide.
    std::string target_file(target->dependency_output_file().value());
    NormalizePath(&target_file);
    written_rules.insert(target_file);
  }

  for (const auto& target : default_toolchain_targets_) {
    const Label& label = target->label();
    OutputFile target_file = GetTargetOutputFile(target);
    if (!target_files.insert(target_file.value()).second) {
      *err = GetDuplicateOutputError(default_toolchain_targets_, target_file);
      return false;
    }

    // Write the long name "foo/bar:baz" for the target "//foo/bar:baz".
    std::string long_name = label.GetUserVisibleName(false);
    base::TrimString(long_name, "/", &long_name);
    WritePhonyRule(target, target_file, long_name, &written_rules);

    // Write the directory name with no target name if they match
    // (e.g. "//foo/bar:bar" -> "foo/bar").
    if (FindLastDirComponent(label.dir()) == label.name()) {
      std::string medium_name =  DirectoryWithNoLastSlash(label.dir());
      base::TrimString(medium_name, "/", &medium_name);
      // That may have generated a name the same as the short name of the
      // target which we already wrote.
      if (medium_name != label.name())
        WritePhonyRule(target, target_file, medium_name, &written_rules);
    }

    // Write short names for ones which are either completely unique or there
    // at least only one of them in the default toolchain that is an exe.
    if (small_name_count[label.name()] == 1 ||
        (target->output_type() == Target::EXECUTABLE &&
         exe_count[label.name()] == 1)) {
      WritePhonyRule(target, target_file, label.name(), &written_rules);
    }

    if (!all_rules.empty())
      all_rules.append(" $\n    ");
    all_rules.append(target_file.value());
  }

  // Pick up phony rules for the toplevel targets with non-unique names (which
  // would have been skipped in the above loop).
  for (const auto& toplevel_target : toplevel_targets) {
    if (small_name_count[toplevel_target->label().name()] > 1) {
      WritePhonyRule(toplevel_target, toplevel_target->dependency_output_file(),
                     toplevel_target->label().name(), &written_rules);
    }
  }

  // Figure out if the BUILD file wants to declare a custom "default"
  // target (rather than building 'all' by default). By convention
  // we use group("default") but it doesn't have to be a group.
  bool default_target_exists = false;
  for (const auto& target : default_toolchain_targets_) {
    const Label& label = target->label();
    if (label.dir().value() == "//" && label.name() == "default")
      default_target_exists = true;
  }

  if (!all_rules.empty()) {
    out_ << "\nbuild all: phony " << all_rules << std::endl;
  }

  if (default_target_exists) {
    out_ << "default default" << std::endl;
  } else if (!all_rules.empty()) {
    out_ << "default all" << std::endl;
  }

  return true;
}

void NinjaBuildWriter::WritePhonyRule(const Target* target,
                                      const OutputFile& target_file,
                                      const std::string& phony_name,
                                      std::set<std::string>* written_rules) {
  if (target_file.value() == phony_name)
    return;  // No need for a phony rule.

  if (written_rules->find(phony_name) != written_rules->end())
    return;  // Already exists.
  written_rules->insert(phony_name);

  EscapeOptions ninja_escape;
  ninja_escape.mode = ESCAPE_NINJA;

  // Escape for special chars Ninja will handle.
  std::string escaped = EscapeString(phony_name, ninja_escape, nullptr);

  out_ << "build " << escaped << ": phony ";
  path_output_.WriteFile(out_, target_file);
  out_ << std::endl;
}
