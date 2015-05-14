// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/runtime_deps.h"

#include <set>
#include <sstream>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/strings/string_split.h"
#include "tools/gn/build_settings.h"
#include "tools/gn/builder.h"
#include "tools/gn/deps_iterator.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/loader.h"
#include "tools/gn/output_file.h"
#include "tools/gn/settings.h"
#include "tools/gn/switches.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

namespace {

using RuntimeDepsVector = std::vector<std::pair<OutputFile, const Target*>>;

// Adds the given file to the deps list if it hasn't already been listed in
// the found_files list. Updates the list.
void AddIfNew(const OutputFile& output_file,
              const Target* source,
              RuntimeDepsVector* deps,
              std::set<OutputFile>* found_file) {
  if (found_file->find(output_file) != found_file->end())
    return;  // Already there.
  deps->push_back(std::make_pair(output_file, source));
}

// Automatically converts a SourceFile to an OutputFile.
void AddIfNew(const SourceFile& source_file,
              const Target* source,
              RuntimeDepsVector* deps,
              std::set<OutputFile>* found_file) {
  AddIfNew(OutputFile(source->settings()->build_settings(), source_file),
           source, deps, found_file);
}

// Returns the output file that the runtime deps considers for the given
// targets. This is weird only for shared libraries.
const OutputFile& GetMainOutput(const Target* target) {
  if (target->output_type() == Target::SHARED_LIBRARY)
    return target->link_output_file();
  return target->dependency_output_file();
}

// To avoid duplicate traversals of targets, or duplicating output files that
// might be listed by more than one target, the set of targets and output files
// that have been found so far is passed.
void RecursiveCollectRuntimeDeps(const Target* target,
                                 bool is_target_data_dep,
                                 RuntimeDepsVector* deps,
                                 std::set<const Target*>* seen_targets,
                                 std::set<OutputFile>* found_files) {
  if (seen_targets->find(target) != seen_targets->end())
    return;  // Already checked.
  seen_targets->insert(target);

  // Add the main output file for executables and shared libraries.
  if (target->output_type() == Target::EXECUTABLE ||
      target->output_type() == Target::SHARED_LIBRARY)
    AddIfNew(GetMainOutput(target), target, deps, found_files);

  // Add all data files.
  for (const auto& file : target->data())
    AddIfNew(file, target, deps, found_files);

  // Actions/copy have all outputs considered when the're a data dep.
  if (is_target_data_dep &&
      (target->output_type() == Target::ACTION ||
       target->output_type() == Target::ACTION_FOREACH ||
       target->output_type() == Target::COPY_FILES)) {
    std::vector<SourceFile> outputs;
    target->action_values().GetOutputsAsSourceFiles(target, &outputs);
    for (const auto& output_file : outputs)
      AddIfNew(output_file, target, deps, found_files);
  }

  // Non-data dependencies (both public and private).
  for (const auto& dep_pair : target->GetDeps(Target::DEPS_LINKED)) {
    if (dep_pair.ptr->output_type() == Target::EXECUTABLE)
      continue;  // Skip executables that aren't data deps.
    RecursiveCollectRuntimeDeps(dep_pair.ptr, false,
                                deps, seen_targets, found_files);
  }

  // Data dependencies.
  for (const auto& dep_pair : target->data_deps()) {
    RecursiveCollectRuntimeDeps(dep_pair.ptr, true,
                                deps, seen_targets, found_files);
  }
}

bool WriteRuntimeDepsFile(const Target* target) {
  SourceFile target_output_as_source =
      GetMainOutput(target).AsSourceFile(target->settings()->build_settings());
  std::string data_deps_file_as_str = target_output_as_source.value();
  data_deps_file_as_str.append(".runtime_deps");
  base::FilePath data_deps_file =
      target->settings()->build_settings()->GetFullPath(
          SourceFile(SourceFile::SwapIn(), &data_deps_file_as_str));

  std::stringstream contents;
  for (const auto& pair : ComputeRuntimeDeps(target))
    contents << pair.first.value() << std::endl;

  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE, data_deps_file_as_str);
  base::CreateDirectory(data_deps_file.DirName());

  std::string contents_str = contents.str();
  return base::WriteFile(data_deps_file, contents_str.c_str(),
                         static_cast<int>(contents_str.size())) > -1;
}

}  // namespace

const char kRuntimeDeps_Help[] =
    "Runtime dependencies\n"
    "\n"
    "  Runtime dependencies of a target are exposed via the \"runtime_deps\"\n"
    "  category of \"gn desc\" (see \"gn help desc\") or they can be written\n"
    "  at build generation time via \"--runtime-deps-list-file\"\n"
    "  (see \"gn help --runtime-deps-list-file\").\n"
    "\n"
    "  To a first approximation, the runtime dependencies of a target are\n"
    "  the set of \"data\" files and the shared libraries from all transitive\n"
    "  dependencies. Executables and shared libraries are considered runtime\n"
    "  dependencies of themselves.\n"
    "\n"
    "Details\n"
    "\n"
    "  Executable targets and those executable targets' transitive\n"
    "  dependencies are not considered unless that executable is listed in\n"
    "  \"data_deps\". Otherwise, GN assumes that the executable (and\n"
    "  everything it requires) is a build-time dependency only.\n"
    "\n"
    "  Action and copy targets that are listed as \"data_deps\" will have all\n"
    "  of their outputs and data files considered as runtime dependencies.\n"
    "  Action and copy targets that are \"deps\" or \"public_deps\" will have\n"
    "  only their data files considered as runtime dependencies. These\n"
    "  targets can list an output file in both the \"outputs\" and \"data\"\n"
    "  lists to force an output file as a runtime dependency in all cases.\n"
    "\n"
    "  The results of static_library or source_set targets are not considered\n"
    "  runtime dependencies since these are assumed to be intermediate\n"
    "  targets only. If you need to list a static library as a runtime\n"
    "  dependency, you can manually compute the .a/.lib file name for the\n"
    "  current platform and list it in the \"data\" list of a target\n"
    "  (possibly on the static library target itself).\n"
    "\n"
    "  When a tool produces more than one output, only the first output\n"
    "  is considered. For example, a shared library target may produce a\n"
    "  .dll and a .lib file on Windows. Only the .dll file will be considered\n"
    "  a runtime dependency.\n";

RuntimeDepsVector ComputeRuntimeDeps(const Target* target) {
  RuntimeDepsVector result;
  std::set<const Target*> seen_targets;
  std::set<OutputFile> found_files;

  // The initial target is not considered a data dependency so that actions's
  // outputs (if the current target is an action) are not automatically
  // considered data deps.
  RecursiveCollectRuntimeDeps(target, false,
                              &result, &seen_targets, &found_files);
  return result;
}

bool WriteRuntimeDepsFilesIfNecessary(const Builder& builder, Err* err) {
  std::string deps_target_list_file =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRuntimeDepsListFile);
  if (deps_target_list_file.empty())
    return true;  // Nothing to do.

  std::string list_contents;
  ScopedTrace load_trace(TraceItem::TRACE_FILE_LOAD, deps_target_list_file);
  if (!base::ReadFileToString(UTF8ToFilePath(deps_target_list_file),
                              &list_contents)) {
    *err = Err(Location(),
        std::string("File for --") + switches::kRuntimeDepsListFile +
            " doesn't exist.",
        "The file given was \"" + deps_target_list_file + "\"");
    return false;
  }
  load_trace.Done();

  std::vector<std::string> lines;
  base::SplitString(list_contents, '\n', &lines);

  SourceDir root_dir("//");
  Label default_toolchain_label = builder.loader()->GetDefaultToolchain();
  for (const auto& line : lines) {
    if (line.empty())
      continue;
    Label label = Label::Resolve(root_dir, default_toolchain_label,
                                 Value(nullptr, line), err);
    if (err->has_error())
      return false;

    const Item* item = builder.GetItem(label);
    const Target* target = item ? item->AsTarget() : nullptr;
    if (!target) {
      *err = Err(Location(), "The label \"" + label.GetUserVisibleName(true) +
          "\" isn't a target.",
          "When reading the line:\n  " + line + "\n"
          "from the --" + switches::kRuntimeDepsListFile + "=" +
          deps_target_list_file);
      return false;
    }

    // Currently this writes all runtime deps files sequentially. We generally
    // expect few of these. We can run this on the worker pool if it looks
    // like it's talking a long time.
    WriteRuntimeDepsFile(target);
  }
  return true;
}
