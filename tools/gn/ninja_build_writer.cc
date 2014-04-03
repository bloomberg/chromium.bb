// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_build_writer.h"

#include <fstream>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
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
  escape_shell.mode = ESCAPE_SHELL;
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
    if (i->first != "q" && i->first != "root") {
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
      path_output_(build_settings->build_dir(), ESCAPE_NINJA, false),
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

  // Provide a way to force regenerating ninja files if the user is suspicious
  // something is out-of-date. This will be "ninja refresh".
  out_ << "\nbuild refresh: gn\n";

  // Provide a way to see what flags are associated with this build:
  // This will be "ninja show".
  const CommandLine& our_cmdline = *CommandLine::ForCurrentProcess();
  std::string args = our_cmdline.GetSwitchValueASCII("args");
  out_ << "rule echo\n";
  out_ << "  command = echo $text\n";
  out_ << "  description = ECHO $desc\n";
  out_ << "build show: echo\n";
  out_ << "  desc = build arguments:\n";
  out_ << "  text = "
       << (args.empty() ? std::string("No build args, using defaults.") : args)
       << "\n";

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

  // Write phony rules for the default toolchain (don't do other toolchains or
  // we'll get naming conflicts).
  for (size_t i = 0; i < default_toolchain_targets_.size(); i++) {
    const Target* target = default_toolchain_targets_[i];

    OutputFile target_file = helper_.GetTargetOutputFile(target);
    if (target_file.value() != target->label().name()) {
      out_ << "build " << target->label().name() << ": phony ";
      path_output_.WriteFile(out_, target_file);
      out_ << std::endl;
    }

    if (!all_rules.empty())
      all_rules.append(" $\n    ");
    all_rules.append(target_file.value());
  }

  if (!all_rules.empty()) {
    out_ << "\nbuild all: phony " << all_rules << std::endl;
    out_ << "default all" << std::endl;
  }
}

