// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_build_writer.h"

#include <fstream>
#include <map>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/escape.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file_manager.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

namespace {

std::string GetSelfInvocationCommand(const BuildSettings* build_settings) {
  base::FilePath executable;
  PathService::Get(base::FILE_EXE, &executable);

  CommandLine cmdline(executable.NormalizePathSeparatorsTo('/'));
  cmdline.AppendArg("gen");
  cmdline.AppendArg(build_settings->build_dir().value());
  cmdline.AppendSwitchPath("--root", build_settings->root_path());
  cmdline.AppendSwitch("-q");  // Don't write output.

  EscapeOptions escape_shell;
  escape_shell.mode = ESCAPE_NINJA_COMMAND;
#if defined(OS_WIN)
  // The command line code quoting varies by platform. We have one string,
  // possibly with spaces, that we want to quote. The Windows command line
  // quotes again, so we don't want quoting. The Posix one doesn't.
  escape_shell.inhibit_quoting = true;
#endif

  const CommandLine& our_cmdline = *CommandLine::ForCurrentProcess();
  const CommandLine::SwitchMap& switches = our_cmdline.GetSwitches();
  for (CommandLine::SwitchMap::const_iterator i = switches.begin();
       i != switches.end(); ++i) {
    // Only write arguments we haven't already written. Always skip "args"
    // since those will have been written to the file and will be used
    // implicitly in the future. Keeping --args would mean changes to the file
    // would be ignored.
    if (i->first != "q" && i->first != "root" && i->first != "args") {
      std::string escaped_value =
          EscapeString(FilePathToUTF8(i->second), escape_shell, NULL);
      cmdline.AppendSwitchASCII(i->first, escaped_value);
    }
  }

#if defined(OS_WIN)
  return base::WideToUTF8(cmdline.GetCommandLineString());
#else
  return cmdline.GetCommandLineString();
#endif
}

}  // namespace

NinjaBuildWriter::NinjaBuildWriter(
    const BuildSettings* build_settings,
    const std::vector<const Settings*>& all_settings,
    const std::vector<const Target*>& default_toolchain_targets,
    std::ostream& out,
    std::ostream& dep_out)
    : build_settings_(build_settings),
      all_settings_(all_settings),
      default_toolchain_targets_(default_toolchain_targets),
      out_(out),
      dep_out_(dep_out),
      path_output_(build_settings->build_dir(), ESCAPE_NINJA),
      helper_(build_settings) {
}

NinjaBuildWriter::~NinjaBuildWriter() {
}

void NinjaBuildWriter::Run() {
  WriteNinjaRules();
  WriteSubninjas();
  WritePhonyAndAllRules();
}

// static
bool NinjaBuildWriter::RunAndWriteFile(
    const BuildSettings* build_settings,
    const std::vector<const Settings*>& all_settings,
    const std::vector<const Target*>& default_toolchain_targets) {
  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, "build.ninja");

  base::FilePath ninja_file(build_settings->GetFullPath(
      SourceFile(build_settings->build_dir().value() + "build.ninja")));
  base::CreateDirectory(ninja_file.DirName());

  std::ofstream file;
  file.open(FilePathToUTF8(ninja_file).c_str(),
            std::ios_base::out | std::ios_base::binary);
  if (file.fail())
    return false;

  std::ofstream depfile;
  depfile.open((FilePathToUTF8(ninja_file) + ".d").c_str(),
               std::ios_base::out | std::ios_base::binary);
  if (depfile.fail())
    return false;

  NinjaBuildWriter gen(build_settings, all_settings,
                       default_toolchain_targets, file, depfile);
  gen.Run();
  return true;
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
  for (size_t i = 0; i < input_files.size(); i++)
    dep_out_ << " " << FilePathToUTF8(input_files[i]);

  // Other files read by the build.
  std::vector<base::FilePath> other_files = g_scheduler->GetGenDependencies();
  for (size_t i = 0; i < other_files.size(); i++)
    dep_out_ << " " << FilePathToUTF8(other_files[i]);

  out_ << std::endl;
}

void NinjaBuildWriter::WriteSubninjas() {
  for (size_t i = 0; i < all_settings_.size(); i++) {
    out_ << "subninja ";
    path_output_.WriteFile(out_,
                           helper_.GetNinjaFileForToolchain(all_settings_[i]));
    out_ << std::endl;
  }
  out_ << std::endl;
}

void NinjaBuildWriter::WritePhonyAndAllRules() {
  std::string all_rules;

  // Write phony rules for all uniquely-named targets in the default toolchain.
  // Don't do other toolchains or we'll get naming conflicts, and if the name
  // isn't unique, also skip it. The exception is for the toplevel targets
  // which we also find.
  std::map<std::string, int> small_name_count;
  std::vector<const Target*> toplevel_targets;
  for (size_t i = 0; i < default_toolchain_targets_.size(); i++) {
    const Target* target = default_toolchain_targets_[i];
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
  }

  for (size_t i = 0; i < default_toolchain_targets_.size(); i++) {
    const Target* target = default_toolchain_targets_[i];
    const Label& label = target->label();
    OutputFile target_file = helper_.GetTargetOutputFile(target);

    // Write the long name "foo/bar:baz" for the target "//foo/bar:baz".
    std::string long_name = label.GetUserVisibleName(false);
    base::TrimString(long_name, "/", &long_name);
    WritePhonyRule(target, target_file, long_name);

    // Write the directory name with no target name if they match
    // (e.g. "//foo/bar:bar" -> "foo/bar").
    if (FindLastDirComponent(label.dir()) == label.name()) {
      std::string medium_name =  DirectoryWithNoLastSlash(label.dir());
      base::TrimString(medium_name, "/", &medium_name);
      // That may have generated a name the same as the short name of the
      // target which we already wrote.
      if (medium_name != label.name())
        WritePhonyRule(target, target_file, medium_name);
    }

    // Write short names for ones which are unique.
    if (small_name_count[label.name()] == 1)
      WritePhonyRule(target, target_file, label.name());

    if (!all_rules.empty())
      all_rules.append(" $\n    ");
    all_rules.append(target_file.value());
  }

  // Pick up phony rules for the toplevel targets with non-unique names (which
  // would have been skipped in the above loop).
  for (size_t i = 0; i < toplevel_targets.size(); i++) {
    if (small_name_count[toplevel_targets[i]->label().name()] > 1) {
      const Target* target = toplevel_targets[i];
      WritePhonyRule(target, helper_.GetTargetOutputFile(target),
                     target->label().name());
    }
  }

  if (!all_rules.empty()) {
    out_ << "\nbuild all: phony " << all_rules << std::endl;
    out_ << "default all" << std::endl;
  }
}

void NinjaBuildWriter::WritePhonyRule(const Target* target,
                                      const OutputFile& target_file,
                                      const std::string& phony_name) {
  if (target_file.value() == phony_name)
    return;  // No need for a phony rule.

  EscapeOptions ninja_escape;
  ninja_escape.mode = ESCAPE_NINJA;

  // Escape for special chars Ninja will handle.
  std::string escaped = EscapeString(phony_name, ninja_escape, NULL);

  out_ << "build " << escaped << ": phony ";
  path_output_.WriteFile(out_, target_file);
  out_ << std::endl;
}
