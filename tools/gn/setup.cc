// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/setup.h"

#include <stdlib.h>

#include <algorithm>
#include <sstream>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/process/launch.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "tools/gn/commands.h"
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
    "  If you specify --root, by default GN will look for the file .gn in\n"
    "  that directory. If you want to specify a different file, you can\n"
    "  additionally pass --dotfile:\n"
    "\n"
    "    gn gen out/Debug --root=/home/build --dotfile=/home/my_gn_file.gn\n"
    "\n"
    "Variables\n"
    "\n"
    "  buildconfig [required]\n"
    "      Label of the build config file. This file will be used to set up\n"
    "      the build file execution environment for each toolchain.\n"
    "\n"
    "  root [optional]\n"
    "      Label of the root build target. The GN build will start by loading\n"
    "      the build file containing this target name. This defaults to\n"
    "      \"//:\" which will cause the file //BUILD.gn to be loaded.\n"
    "\n"
    "  secondary_source [optional]\n"
    "      Label of an alternate directory tree to find input files. When\n"
    "      searching for a BUILD.gn file (or the build config file discussed\n"
    "      above), the file will first be looked for in the source root.\n"
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
    "  root = \"//:root\"\n"
    "\n"
    "  secondary_source = \"//build/config/temporary_buildfiles/\"\n";

namespace {

// More logging.
const char kSwitchVerbose[] = "v";

// Set build args.
const char kSwitchArgs[] = "args";

// Set root dir.
const char kSwitchRoot[] = "root";

// Set dotfile name.
const char kSwitchDotfile[] = "dotfile";

// Enable timing.
const char kTimeSwitch[] = "time";

const char kTracelogSwitch[] = "tracelog";

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

const char CommonSetup::kBuildArgFileName[] = "args.gn";

CommonSetup::CommonSetup()
    : build_settings_(),
      loader_(new LoaderImpl(&build_settings_)),
      builder_(new Builder(loader_.get())),
      root_build_file_("//BUILD.gn"),
      check_for_bad_items_(true),
      check_for_unused_overrides_(true),
      check_public_headers_(false) {
  loader_->set_complete_callback(base::Bind(&DecrementWorkCount));
}

CommonSetup::CommonSetup(const CommonSetup& other)
    : build_settings_(other.build_settings_),
      loader_(new LoaderImpl(&build_settings_)),
      builder_(new Builder(loader_.get())),
      root_build_file_(other.root_build_file_),
      check_for_bad_items_(other.check_for_bad_items_),
      check_for_unused_overrides_(other.check_for_unused_overrides_),
      check_public_headers_(other.check_public_headers_) {
  loader_->set_complete_callback(base::Bind(&DecrementWorkCount));
}

CommonSetup::~CommonSetup() {
}

void CommonSetup::RunPreMessageLoop() {
  // Load the root build file.
  loader_->Load(root_build_file_, LocationRange(), Label());

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

  if (check_public_headers_) {
    if (!commands::CheckPublicHeaders(&build_settings_,
                                      builder_->GetAllResolvedTargets(),
                                      std::vector<const Target*>(),
                                      false)) {
      return false;
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
      dotfile_scope_(&empty_settings_),
      fill_arguments_(true) {
  empty_settings_.set_toolchain_label(Label());
  build_settings_.set_item_defined_callback(
      base::Bind(&ItemDefinedCallback, scheduler_.main_loop(), builder_));

  // The scheduler's main loop wasn't created when the Loader was created, so
  // we need to set it now.
  loader_->set_main_loop(scheduler_.main_loop());
}

Setup::~Setup() {
}

bool Setup::DoSetup(const std::string& build_dir, bool force_create) {
  CommandLine* cmdline = CommandLine::ForCurrentProcess();

  scheduler_.set_verbose_logging(cmdline->HasSwitch(kSwitchVerbose));
  if (cmdline->HasSwitch(kTimeSwitch) ||
      cmdline->HasSwitch(kTracelogSwitch))
    EnableTracing();

  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "DoSetup");

  if (!FillSourceDir(*cmdline))
    return false;
  if (!RunConfigFile())
    return false;
  if (!FillOtherConfig(*cmdline))
    return false;

  // Must be after FillSourceDir to resolve.
  if (!FillBuildDir(build_dir, !force_create))
    return false;

  if (fill_arguments_) {
    if (!FillArguments(*cmdline))
      return false;
  }
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

SourceFile Setup::GetBuildArgFile() const {
  return SourceFile(build_settings_.build_dir().value() + kBuildArgFileName);
}

bool Setup::FillArguments(const CommandLine& cmdline) {
  // Use the args on the command line if specified, and save them. Do this even
  // if the list is empty (this means clear any defaults).
  if (cmdline.HasSwitch(kSwitchArgs)) {
    if (!FillArgsFromCommandLine(cmdline.GetSwitchValueASCII(kSwitchArgs)))
      return false;
    SaveArgsToFile();
    return true;
  }

  // No command line args given, use the arguments from the build dir (if any).
  return FillArgsFromFile();
}

bool Setup::FillArgsFromCommandLine(const std::string& args) {
  args_input_file_.reset(new InputFile(SourceFile()));
  args_input_file_->SetContents(args);
  args_input_file_->set_friendly_name("the command-line \"--args\"");
  return FillArgsFromArgsInputFile();
}

bool Setup::FillArgsFromFile() {
  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "Load args file");

  SourceFile build_arg_source_file = GetBuildArgFile();
  base::FilePath build_arg_file =
      build_settings_.GetFullPath(build_arg_source_file);

  std::string contents;
  if (!base::ReadFileToString(build_arg_file, &contents))
    return true;  // File doesn't exist, continue with default args.

  // Add a dependency on the build arguments file. If this changes, we want
  // to re-generate the build.
  g_scheduler->AddGenDependency(build_arg_file);

  if (contents.empty())
    return true;  // Empty file, do nothing.

  args_input_file_.reset(new InputFile(build_arg_source_file));
  args_input_file_->SetContents(contents);
  args_input_file_->set_friendly_name(
      "build arg file (use \"gn args <out_dir>\" to edit)");

  setup_trace.Done();  // Only want to count the load as part of the trace.
  return FillArgsFromArgsInputFile();
}

bool Setup::FillArgsFromArgsInputFile() {
  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "Parse args");

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

bool Setup::SaveArgsToFile() {
  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "Save args file");

  Scope::KeyValueMap args = build_settings_.build_args().GetAllOverrides();

  std::ostringstream stream;
  for (Scope::KeyValueMap::const_iterator i = args.begin();
       i != args.end(); ++i) {
    stream << i->first.as_string() << " = " << i->second.ToString(true);
    stream << std::endl;
  }

  // For the first run, the build output dir might not be created yet, so do
  // that so we can write a file into it. Ignore errors, we'll catch the error
  // when we try to write a file to it below.
  base::FilePath build_arg_file =
      build_settings_.GetFullPath(GetBuildArgFile());
  base::CreateDirectory(build_arg_file.DirName());

  std::string contents = stream.str();
#if defined(OS_WIN)
  // Use Windows lineendings for this file since it will often open in
  // Notepad which can't handle Unix ones.
  ReplaceSubstringsAfterOffset(&contents, 0, "\n", "\r\n");
#endif
  if (base::WriteFile(build_arg_file, contents.c_str(),
      static_cast<int>(contents.size())) == -1) {
    Err(Location(), "Args file could not be written.",
      "The file is \"" + FilePathToUTF8(build_arg_file) +
        "\"").PrintToStdout();
    return false;
  }

  // Add a dependency on the build arguments file. If this changes, we want
  // to re-generate the build.
  g_scheduler->AddGenDependency(build_arg_file);

  return true;
}

bool Setup::FillSourceDir(const CommandLine& cmdline) {
  // Find the .gn file.
  base::FilePath root_path;

  // Prefer the command line args to the config file.
  base::FilePath relative_root_path = cmdline.GetSwitchValuePath(kSwitchRoot);
  if (!relative_root_path.empty()) {
    root_path = base::MakeAbsoluteFilePath(relative_root_path);
    if (root_path.empty()) {
      Err(Location(), "Root source path not found.",
          "The path \"" + FilePathToUTF8(relative_root_path) +
          "\" doesn't exist.").PrintToStdout();
      return false;
    }

    // When --root is specified, an alternate --dotfile can also be set.
    // --dotfile should be a real file path and not a "//foo" source-relative
    // path.
    base::FilePath dot_file_path = cmdline.GetSwitchValuePath(kSwitchDotfile);
    if (dot_file_path.empty()) {
      dotfile_name_ = root_path.Append(kGnFile);
    } else {
      dotfile_name_ = base::MakeAbsoluteFilePath(dot_file_path);
      if (dotfile_name_.empty()) {
        Err(Location(), "Could not load dotfile.",
            "The file \"" + FilePathToUTF8(dot_file_path) +
            "\" cound't be loaded.").PrintToStdout();
        return false;
      }
    }
  } else {
    // In the default case, look for a dotfile and that also tells us where the
    // source root is.
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

bool Setup::FillBuildDir(const std::string& build_dir, bool require_exists) {
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

  if (require_exists) {
    base::FilePath build_dir_path = build_settings_.GetFullPath(resolved);
    if (!base::PathExists(build_dir_path.Append(
            FILE_PATH_LITERAL("build.ninja")))) {
      Err(Location(), "Not a build directory.",
          "This command requires an existing build directory. I interpreted "
          "your input\n\"" + build_dir + "\" as:\n  " +
          FilePathToUTF8(build_dir_path) +
          "\nwhich doesn't seem to contain a previously-generated build.")
          .PrintToStdout();
      return false;
    }
  }

  build_settings_.SetBuildDir(resolved);
  return true;
}

void Setup::FillPythonPath() {
  // Trace this since it tends to be a bit slow on Windows.
  ScopedTrace setup_trace(TraceItem::TRACE_SETUP, "Fill Python Path");
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

  // Secondary source path, read from the config file if present.
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

  // Root build file.
  const Value* root_value = dotfile_scope_.GetValue("root", true);
  if (root_value) {
    if (!root_value->VerifyTypeIs(Value::STRING, &err)) {
      err.PrintToStdout();
      return false;
    }

    Label root_target_label =
        Label::Resolve(SourceDir("//"), Label(), *root_value, &err);
    if (err.has_error()) {
      err.PrintToStdout();
      return false;
    }

    root_build_file_ = Loader::BuildFileForLabel(root_target_label);
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

