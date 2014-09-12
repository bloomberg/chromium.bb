// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_target_writer.h"

#include <fstream>
#include <sstream>

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/ninja_action_target_writer.h"
#include "tools/gn/ninja_binary_target_writer.h"
#include "tools/gn/ninja_copy_target_writer.h"
#include "tools/gn/ninja_group_target_writer.h"
#include "tools/gn/ninja_utils.h"
#include "tools/gn/output_file.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

NinjaTargetWriter::NinjaTargetWriter(const Target* target,
                                     std::ostream& out)
    : settings_(target->settings()),
      target_(target),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(), ESCAPE_NINJA) {
}

NinjaTargetWriter::~NinjaTargetWriter() {
}

// static
void NinjaTargetWriter::RunAndWriteFile(const Target* target) {
  const Settings* settings = target->settings();

  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE,
                    target->label().GetUserVisibleName(false));
  trace.SetToolchain(settings->toolchain_label());

  base::FilePath ninja_file(settings->build_settings()->GetFullPath(
      GetNinjaFileForTarget(target)));

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Writing", FilePathToUTF8(ninja_file));

  base::CreateDirectory(ninja_file.DirName());

  // It's rediculously faster to write to a string and then write that to
  // disk in one operation than to use an fstream here.
  std::stringstream file;

  // Call out to the correct sub-type of writer.
  if (target->output_type() == Target::COPY_FILES) {
    NinjaCopyTargetWriter writer(target, file);
    writer.Run();
  } else if (target->output_type() == Target::ACTION ||
             target->output_type() == Target::ACTION_FOREACH) {
    NinjaActionTargetWriter writer(target, file);
    writer.Run();
  } else if (target->output_type() == Target::GROUP) {
    NinjaGroupTargetWriter writer(target, file);
    writer.Run();
  } else if (target->output_type() == Target::EXECUTABLE ||
             target->output_type() == Target::STATIC_LIBRARY ||
             target->output_type() == Target::SHARED_LIBRARY ||
             target->output_type() == Target::SOURCE_SET) {
    NinjaBinaryTargetWriter writer(target, file);
    writer.Run();
  } else {
    CHECK(0);
  }

  std::string contents = file.str();
  base::WriteFile(ninja_file, contents.c_str(),
                  static_cast<int>(contents.size()));
}

void NinjaTargetWriter::WriteSharedVars(const SubstitutionBits& bits) {
  bool written_anything = false;

  // Target label.
  if (bits.used[SUBSTITUTION_LABEL]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_LABEL] << " = "
         << SubstitutionWriter::GetTargetSubstitution(
                target_, SUBSTITUTION_LABEL)
         << std::endl;
    written_anything = true;
  }

  // Root gen dir.
  if (bits.used[SUBSTITUTION_ROOT_GEN_DIR]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_ROOT_GEN_DIR] << " = "
         << SubstitutionWriter::GetTargetSubstitution(
                target_, SUBSTITUTION_ROOT_GEN_DIR)
         << std::endl;
    written_anything = true;
  }

  // Root out dir.
  if (bits.used[SUBSTITUTION_ROOT_OUT_DIR]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_ROOT_OUT_DIR] << " = "
         << SubstitutionWriter::GetTargetSubstitution(
                target_, SUBSTITUTION_ROOT_OUT_DIR)
         << std::endl;
    written_anything = true;
  }

  // Target gen dir.
  if (bits.used[SUBSTITUTION_TARGET_GEN_DIR]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_TARGET_GEN_DIR] << " = "
         << SubstitutionWriter::GetTargetSubstitution(
                target_, SUBSTITUTION_TARGET_GEN_DIR)
         << std::endl;
    written_anything = true;
  }

  // Target out dir.
  if (bits.used[SUBSTITUTION_TARGET_OUT_DIR]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_TARGET_OUT_DIR] << " = "
         << SubstitutionWriter::GetTargetSubstitution(
                target_, SUBSTITUTION_TARGET_OUT_DIR)
         << std::endl;
    written_anything = true;
  }

  // Target output name.
  if (bits.used[SUBSTITUTION_TARGET_OUTPUT_NAME]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_TARGET_OUTPUT_NAME] << " = "
         << SubstitutionWriter::GetTargetSubstitution(
                target_, SUBSTITUTION_TARGET_OUTPUT_NAME)
         << std::endl;
    written_anything = true;
  }

  // If we wrote any vars, separate them from the rest of the file that follows
  // with a blank line.
  if (written_anything)
    out_ << std::endl;
}

OutputFile NinjaTargetWriter::WriteInputDepsStampAndGetDep(
    const std::vector<const Target*>& extra_hard_deps) const {
  CHECK(target_->toolchain())
      << "Toolchain not set on target "
      << target_->label().GetUserVisibleName(true);

  // For an action (where we run a script only once) the sources are the same
  // as the source prereqs.
  bool list_sources_as_input_deps = (target_->output_type() == Target::ACTION);

  // Actions get implicit dependencies on the script itself.
  bool add_script_source_as_dep =
      (target_->output_type() == Target::ACTION) ||
      (target_->output_type() == Target::ACTION_FOREACH);

  if (!add_script_source_as_dep &&
      extra_hard_deps.empty() &&
      target_->inputs().empty() &&
      target_->recursive_hard_deps().empty() &&
      (!list_sources_as_input_deps || target_->sources().empty()) &&
      target_->toolchain()->deps().empty())
    return OutputFile();  // No input/hard deps.

  // One potential optimization is if there are few input dependencies (or
  // potentially few sources that depend on these) it's better to just write
  // all hard deps on each sources line than have this intermediate stamp. We
  // do the stamp file because duplicating all the order-only deps for each
  // source file can really explode the ninja file but this won't be the most
  // optimal thing in all cases.

  OutputFile input_stamp_file(
      RebaseSourceAbsolutePath(GetTargetOutputDir(target_).value(),
                               settings_->build_settings()->build_dir()));
  input_stamp_file.value().append(target_->label().name());
  input_stamp_file.value().append(".inputdeps.stamp");

  out_ << "build ";
  path_output_.WriteFile(out_, input_stamp_file);
  out_ << ": "
       << GetNinjaRulePrefixForToolchain(settings_)
       << Toolchain::ToolTypeToName(Toolchain::TYPE_STAMP);

  // Script file (if applicable).
  if (add_script_source_as_dep) {
    out_ << " ";
    path_output_.WriteFile(out_, target_->action_values().script());
  }

  // Input files are order-only deps.
  const Target::FileList& prereqs = target_->inputs();
  for (size_t i = 0; i < prereqs.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, prereqs[i]);
  }
  if (list_sources_as_input_deps) {
    const Target::FileList& sources = target_->sources();
    for (size_t i = 0; i < sources.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(out_, sources[i]);
    }
  }

  // The different souces of input deps may duplicate some targets, so uniquify
  // them (ordering doesn't matter for this case).
  std::set<const Target*> unique_deps;

  // Hard dependencies that are direct or indirect dependencies.
  const std::set<const Target*>& hard_deps = target_->recursive_hard_deps();
  for (std::set<const Target*>::const_iterator i = hard_deps.begin();
       i != hard_deps.end(); ++i) {
    unique_deps.insert(*i);
  }

  // Extra hard dependencies passed in.
  unique_deps.insert(extra_hard_deps.begin(), extra_hard_deps.end());

  // Toolchain dependencies. These must be resolved before doing anything.
  // This just writs all toolchain deps for simplicity. If we find that
  // toolchains often have more than one dependency, we could consider writing
  // a toolchain-specific stamp file and only include the stamp here.
  const LabelTargetVector& toolchain_deps = target_->toolchain()->deps();
  for (size_t i = 0; i < toolchain_deps.size(); i++)
    unique_deps.insert(toolchain_deps[i].ptr);

  for (std::set<const Target*>::const_iterator i = unique_deps.begin();
       i != unique_deps.end(); ++i) {
    DCHECK(!(*i)->dependency_output_file().value().empty());
    out_ << " ";
    path_output_.WriteFile(out_, (*i)->dependency_output_file());
  }

  out_ << "\n";
  return input_stamp_file;
}

void NinjaTargetWriter::WriteStampForTarget(
    const std::vector<OutputFile>& files,
    const std::vector<OutputFile>& order_only_deps) {
  const OutputFile& stamp_file = target_->dependency_output_file();

  // First validate that the target's dependency is a stamp file. Otherwise,
  // we shouldn't have gotten here!
  CHECK(EndsWith(stamp_file.value(), ".stamp", false))
      << "Output should end in \".stamp\" for stamp file output. Instead got: "
      << "\"" << stamp_file.value() << "\"";

  out_ << "build ";
  path_output_.WriteFile(out_, stamp_file);

  out_ << ": "
       << GetNinjaRulePrefixForToolchain(settings_)
       << Toolchain::ToolTypeToName(Toolchain::TYPE_STAMP);
  path_output_.WriteFiles(out_, files);

  if (!order_only_deps.empty()) {
    out_ << " ||";
    path_output_.WriteFiles(out_, order_only_deps);
  }
  out_ << std::endl;
}
