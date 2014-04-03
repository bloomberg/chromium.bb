// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/setup.h"

#include <stdlib.h>

#include <algorithm>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/input_file.h"
#include "tools/gn/parse_tree.h"
#include "tools/gn/parser.h"
#include "tools/gn/source_dir.h"
#include "tools/gn/source_file.h"
#include "tools/gn/standard_out.h"
#include "tools/gn/tokenizer.h"
#include "tools/gn/trace.h"
#include "tools/gn/value.h"

#if defined(OS_WIN)
#include <windows.h>
#endif

extern const char kDotfile_Help[] =
    ".gn file\n"
    "\n"
    "  When gn starts, it will search the current directory and parent\n"
    "  directories for a file called \".gn\". This indicates the source root.\n"
    "  You can override this detection by using the --root command-line\n"
    "  argument\n"
    "\n"
    "  The .gn file in the source root will be executed. The syntax is the\n"
    "  same as a buildfile, but with very limited build setup-specific\n"
    "  meaning.\n"
    "\n"
    "Variables\n"
    "\n"
    "  buildconfig [required]\n"
    "      Label of the build config file. This file will be used to setup\n"
    "      the build file execution environment for each toolchain.\n"
    "\n"
    "  secondary_source [optional]\n"
    "      Label of an alternate directory tree to find input files. When\n"
    "      searching for a BUILD.gn file (or the build config file discussed\n"
    "      above), the file fill first be looked for in the source root.\n"
    "      If it's not found, the secondary source root will be checked\n"
    "      (which would contain a parallel directory hierarchy).\n"
    "\n"
    "      This behavior is intended to be used when BUILD.gn files can't be\n"
    "      checked in to certain source directories for whatever reason.\n"
    "\n"
    "      The secondary source root must be inside the main source tree.\n"
    "\n"
    "Example .gn file contents\n"
    "\n"
    "  buildconfig = \"//build/config/BUILDCONFIG.gn\"\n"
    "\n"
    "  secondary_source = \"//build/config/temporary_buildfiles/\"\n";

namespace {

// More logging.
const char kSwitchVerbose[] = "v";

// Set build args.
const char kSwitchArgs[] = "args";

// Set root dir.
const char kSwitchRoot[] = "root";

// Enable timing.
const char kTimeSwitch[] = "time";

const char kTracelogSwitch[] = "tracelog";

const char kSecondarySource[] = "secondary";

const base::FilePath::CharType kGnFile[] = FILE_PATH_LITERAL(".gn");

base::FilePath FindDotFile(const base::FilePath& current_dir) {
  base::FilePath try_this_file = current_dir.Append(kGnFile);
  if (base::PathExists(try_this_file))
    return try_this_file;

  base::FilePath with_no_slash = current_dir.StripTrailingSeparators();
  base::FilePath up_one_dir = with_no_slash.DirName();
  if (up_one_dir == current_dir)
    return base::FilePath();  // Got to the top.

  return FindDotFile(up_one_dir);
}

// Called on any thread. Post the item to the builder on the main thread.
void ItemDefinedCallback(base::MessageLoop* main_loop,
                         scoped_refptr<Builder> builder,
                         scoped_ptr<Item> item) {
  DCHECK(item);
  main_loop->PostTask(FROM_HERE, base::Bind(&Builder::ItemDefined, builder,
                                            base::Passed(&item)));
}

void DecrementWorkCount() {
  g_scheduler->DecrementWorkCount();
}

}  // namespace

// CommonSetup -----------------------------------------------------------------

CommonSetup::CommonSetup()
    : build_settings_(),
      loader_(new LoaderImpl(&build_settings_)),
      builder_(new Builder(loader_.get())),
      check_for_bad_items_(true),
      check_for_unused_overrides_(true) {
  loader_->set_complete_callback(base::Bind(&DecrementWorkCount));
}

CommonSetup::CommonSetup(const CommonSetup& other)
    : build_settings_(other.build_settings_),
      loader_(new LoaderImpl(&build_settings_)),
      builder_(new Builder(loader_.get())),
      check_for_bad_items_(other.check_for_bad_items_),
      check_for_unused_overrides_(other.check_for_unused_overrides_) {
  loader_->set_complete_callback(base::Bind(&DecrementWorkCount));
}

CommonSetup::~CommonSetup() {
}

void CommonSetup::RunPreMessageLoop() {
  // Load the root build file.
  loader_->Load(SourceFile("//BUILD.gn"), Label());

  // Will be decremented with the loader is drained.
  g_scheduler->IncrementWorkCount();
}

bool CommonSetup::RunPostMessageLoop() {
  Err err;
  if (check_for_bad_items_) {
    if (!builder_->CheckForBadItems(&err)) {
      err.PrintToStdout();
      return false;
    }
  }

  if (check_for_unused_overrides_) {
    if (!build_settings_.build_args().VerifyAllOverridesUsed(&err)) {
      // TODO(brettw) implement a system of warnings. Until we have a better
      // system, print the error but don't return failure.
      err.PrintToStdout();
      return true;
    }
  }

  // Write out tracing and timing if requested.
  const CommandLine* cmdline = CommandLine::ForCurrentProcess();
  if (cmdline->HasSwitch(kTimeSwitch))
    PrintLongHelp(SummarizeTraces());
  if (cmdline->HasSwitch(kTracelogSwitch))
    SaveTraces(cmdline->GetSwitchValuePath(kTracelogSwitch));

  return true;
}

// Setup -----------------------------------------------------------------------

Setup::Setup()
    : CommonSetup(),
      empty_settings_(&empty_build_settings_, std::string()),
      dotfile_scope_(&empty_settings_) {
  empty_settings_.set_toolchain_label(Label());
  build_settings_.set_item_defined_callback(
      base::Bind(&ItemDefinedCallback, scheduler_.main_loop(), builder_));

  // The scheduler's main loop wasn't created when the Loader was created, so
  // we need to set it now.
  loader_->set_main_loop(scheduler_.main_loop());
}

Setup::~Setup() {
}

bool Setup::DoSetup(const std::string& build_dir) {
  CommandLine* cmdline = CommandLine::ForCurrentProcess();

  scheduler_.set_verbose_logging(cmdline->HasSwitch(kSwitchVerbose));
  if (cmdline->HasSwitch(kTimeSwitch) ||
      cmdline->HasSwitch(kTracelogSwitch))
    EnableTracing();

  if (!FillArguments(*cmdline))
    return false;
  if (!FillSourceDir(*cmdline))
    return false;
  if (!RunConfigFile())
    return false;
  if (!FillOtherConfig(*cmdline))
    return false;
  if (!FillBuildDir(build_dir))  // Must be after FillSourceDir to resolve.
    return false;
  FillPythonPath();

  return true;
}

bool Setup::Run() {
  RunPreMessageLoop();
  if (!scheduler_.Run())
    return false;
  return RunPostMessageLoop();
}

Scheduler* Setup::GetScheduler() {
  return &scheduler_;
}

bool Setup::FillArguments(const CommandLine& cmdline) {
  std::string args = cmdline.GetSwitchValueASCII(kSwitchArgs);
  if (args.empty())
    return true;  // Nothing to set.

  args_input_file_.reset(new InputFile(SourceFile()));
  args_input_file_->SetContents(args);
  args_input_file_->set_friendly_name("the command-line \"--args\" settings");

  Err err;
  args_tokens_ = Tokenizer::Tokenize(args_input_file_.get(), &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  args_root_ = Parser::Parse(args_tokens_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  Scope arg_scope(&empty_settings_);
  args_root_->AsBlock()->ExecuteBlockInScope(&arg_scope, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  // Save the result of the command args.
  Scope::KeyValueMap overrides;
  arg_scope.GetCurrentScopeValues(&overrides);
  build_settings_.build_args().AddArgOverrides(overrides);
  return true;
}

bool Setup::FillSourceDir(const CommandLine& cmdline) {
  // Find the .gn file.
  base::FilePath root_path;

  // Prefer the command line args to the config file.
  base::FilePath relative_root_path = cmdline.GetSwitchValuePath(kSwitchRoot);
  if (!relative_root_path.empty()) {
    root_path = base::MakeAbsoluteFilePath(relative_root_path);
    dotfile_name_ = root_path.Append(kGnFile);
  } else {
    base::FilePath cur_dir;
    base::GetCurrentDirectory(&cur_dir);
    dotfile_name_ = FindDotFile(cur_dir);
    if (dotfile_name_.empty()) {
      Err(Location(), "Can't find source root.",
          "I could not find a \".gn\" file in the current directory or any "
          "parent,\nand the --root command-line argument was not specified.")
          .PrintToStdout();
      return false;
    }
    root_path = dotfile_name_.DirName();
  }

  if (scheduler_.verbose_logging())
    scheduler_.Log("Using source root", FilePathToUTF8(root_path));
  build_settings_.SetRootPath(root_path);

  return true;
}

bool Setup::FillBuildDir(const std::string& build_dir) {
  SourceDir resolved =
      SourceDirForCurrentDirectory(build_settings_.root_path()).
          ResolveRelativeDir(build_dir);
  if (resolved.is_null()) {
    Err(Location(), "Couldn't resolve build directory.",
        "The build directory supplied (\"" + build_dir + "\") was not valid.").
        PrintToStdout();
    return false;
  }

  if (scheduler_.verbose_logging())
    scheduler_.Log("Using build dir", resolved.value());
  build_settings_.SetBuildDir(resolved);
  return true;
}

void Setup::FillPythonPath() {
#if defined(OS_WIN)
  // Find Python on the path so we can use the absolute path in the build.
  const base::char16 kGetPython[] =
      L"cmd.exe /c python -c \"import sys; print sys.executable\"";
  std::string python_path;
  if (base::GetAppOutput(kGetPython, &python_path)) {
    base::TrimWhitespaceASCII(python_path, base::TRIM_ALL, &python_path);
    if (scheduler_.verbose_logging())
      scheduler_.Log("Found python", python_path);
  } else {
    scheduler_.Log("WARNING", "Could not find python on path, using "
        "just \"python.exe\"");
    python_path = "python.exe";
  }
  build_settings_.set_python_path(base::FilePath(base::UTF8ToUTF16(python_path))
                                      .NormalizePathSeparatorsTo('/'));
#else
  build_settings_.set_python_path(base::FilePath("python"));
#endif
}

bool Setup::RunConfigFile() {
  if (scheduler_.verbose_logging())
    scheduler_.Log("Got dotfile", FilePathToUTF8(dotfile_name_));

  dotfile_input_file_.reset(new InputFile(SourceFile("//.gn")));
  if (!dotfile_input_file_->Load(dotfile_name_)) {
    Err(Location(), "Could not load dotfile.",
        "The file \"" + FilePathToUTF8(dotfile_name_) + "\" cound't be loaded")
        .PrintToStdout();
    return false;
  }

  Err err;
  dotfile_tokens_ = Tokenizer::Tokenize(dotfile_input_file_.get(), &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  dotfile_root_ = Parser::Parse(dotfile_tokens_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  dotfile_root_->AsBlock()->ExecuteBlockInScope(&dotfile_scope_, &err);
  if (err.has_error()) {
    err.PrintToStdout();
    return false;
  }

  return true;
}

bool Setup::FillOtherConfig(const CommandLine& cmdline) {
  Err err;

  // Secondary source path.
  SourceDir secondary_source;
  if (cmdline.HasSwitch(kSecondarySource)) {
    // Prefer the command line over the config file.
    secondary_source =
        SourceDir(cmdline.GetSwitchValueASCII(kSecondarySource));
  } else {
    // Read from the config file if present.
    const Value* secondary_value =
        dotfile_scope_.GetValue("secondary_source", true);
    if (secondary_value) {
      if (!secondary_value->VerifyTypeIs(Value::STRING, &err)) {
        err.PrintToStdout();
        return false;
      }
      build_settings_.SetSecondarySourcePath(
          SourceDir(secondary_value->string_value()));
    }
  }

  // Build config file.
  const Value* build_config_value =
      dotfile_scope_.GetValue("buildconfig", true);
  if (!build_config_value) {
    Err(Location(), "No build config file.",
        "Your .gn file (\"" + FilePathToUTF8(dotfile_name_) + "\")\n"
        "didn't specify a \"buildconfig\" value.").PrintToStdout();
    return false;
  } else if (!build_config_value->VerifyTypeIs(Value::STRING, &err)) {
    err.PrintToStdout();
    return false;
  }
  build_settings_.set_build_config_file(
      SourceFile(build_config_value->string_value()));

  return true;
}

// DependentSetup --------------------------------------------------------------

DependentSetup::DependentSetup(Setup* derive_from)
    : CommonSetup(*derive_from),
      scheduler_(derive_from->GetScheduler()) {
  build_settings_.set_item_defined_callback(
      base::Bind(&ItemDefinedCallback, scheduler_->main_loop(), builder_));
}

DependentSetup::DependentSetup(DependentSetup* derive_from)
    : CommonSetup(*derive_from),
      scheduler_(derive_from->GetScheduler()) {
  build_settings_.set_item_defined_callback(
      base::Bind(&ItemDefinedCallback, scheduler_->main_loop(), builder_));
}

DependentSetup::~DependentSetup() {
}

Scheduler* DependentSetup::GetScheduler() {
  return scheduler_;
}

void DependentSetup::RunPreMessageLoop() {
  CommonSetup::RunPreMessageLoop();
}

bool DependentSetup::RunPostMessageLoop() {
  return CommonSetup::RunPostMessageLoop();
}

