// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_target_writer.h"

#include <fstream>
#include <sstream>

#include "base/file_util.h"
#include "tools/gn/err.h"
#include "tools/gn/file_template.h"
#include "tools/gn/ninja_action_target_writer.h"
#include "tools/gn/ninja_binary_target_writer.h"
#include "tools/gn/ninja_copy_target_writer.h"
#include "tools/gn/ninja_group_target_writer.h"
#include "tools/gn/scheduler.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/target.h"
#include "tools/gn/trace.h"

NinjaTargetWriter::NinjaTargetWriter(const Target* target,
                                     const Toolchain* toolchain,
                                     std::ostream& out)
    : settings_(target->settings()),
      target_(target),
      toolchain_(toolchain),
      out_(out),
      path_output_(settings_->build_settings()->build_dir(),
                   ESCAPE_NINJA,
                   false),
      helper_(settings_->build_settings()) {
}

NinjaTargetWriter::~NinjaTargetWriter() {
}

// static
void NinjaTargetWriter::RunAndWriteFile(const Target* target,
                                        const Toolchain* toolchain) {
  const Settings* settings = target->settings();
  NinjaHelper helper(settings->build_settings());

  ScopedTrace trace(TraceItem::TRACE_FILE_WRITE,
                    target->label().GetUserVisibleName(false));
  trace.SetToolchain(settings->toolchain_label());

  base::FilePath ninja_file(settings->build_settings()->GetFullPath(
      helper.GetNinjaFileForTarget(target).GetSourceFile(
          settings->build_settings())));

  if (g_scheduler->verbose_logging())
    g_scheduler->Log("Writing", FilePathToUTF8(ninja_file));

  base::CreateDirectory(ninja_file.DirName());

  // It's rediculously faster to write to a string and then write that to
  // disk in one operation than to use an fstream here.
  std::stringstream file;

  // Call out to the correct sub-type of writer.
  if (target->output_type() == Target::COPY_FILES) {
    NinjaCopyTargetWriter writer(target, toolchain, file);
    writer.Run();
  } else if (target->output_type() == Target::ACTION ||
             target->output_type() == Target::ACTION_FOREACH) {
    NinjaActionTargetWriter writer(target, toolchain, file);
    writer.Run();
  } else if (target->output_type() == Target::GROUP) {
    NinjaGroupTargetWriter writer(target, toolchain, file);
    writer.Run();
  } else if (target->output_type() == Target::EXECUTABLE ||
             target->output_type() == Target::STATIC_LIBRARY ||
             target->output_type() == Target::SHARED_LIBRARY ||
             target->output_type() == Target::SOURCE_SET) {
    NinjaBinaryTargetWriter writer(target, toolchain, file);
    writer.Run();
  } else {
    CHECK(0);
  }

  std::string contents = file.str();
  base::WriteFile(ninja_file, contents.c_str(),
                  static_cast<int>(contents.size()));
}

std::string NinjaTargetWriter::WriteInputDepsStampAndGetDep(
    const std::vector<const Target*>& extra_hard_deps) const {
  // For an action (where we run a script only once) the sources are the same
  // as the source prereqs.
  bool list_sources_as_input_deps = target_->output_type() == Target::ACTION;

  if (extra_hard_deps.empty() &&
      target_->source_prereqs().empty() &&
      target_->recursive_hard_deps().empty() &&
      (!list_sources_as_input_deps || target_->sources().empty()))
    return std::string();  // No input/hard deps.

  // One potential optimization is if there are few input dependencies (or
  // potentially few sources that depend on these) it's better to just write
  // all hard deps on each sources line than have this intermediate stamp. We
  // do the stamp file because duplicating all the order-only deps for each
  // source file can really explode the ninja file but this won't be the most
  // optimal thing in all cases.

  OutputFile input_stamp_file = helper_.GetTargetOutputDir(target_);
  input_stamp_file.value().append(target_->label().name());
  input_stamp_file.value().append(".inputdeps.stamp");

  std::ostringstream stamp_file_stream;
  path_output_.WriteFile(stamp_file_stream, input_stamp_file);
  std::string stamp_file_string = stamp_file_stream.str();

  out_ << "build " << stamp_file_string << ": stamp";

  // Input files are order-only deps.
  const Target::FileList& prereqs = target_->source_prereqs();
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

  // Add on any hard deps that are direct or indirect dependencies.
  const std::set<const Target*>& hard_deps = target_->recursive_hard_deps();
  for (std::set<const Target*>::const_iterator i = hard_deps.begin();
       i != hard_deps.end(); ++i) {
    out_ << " ";
    path_output_.WriteFile(out_, helper_.GetTargetOutputFile(*i));
  }

  // Extra hard deps passed in.
  for (size_t i = 0; i < extra_hard_deps.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_,
        helper_.GetTargetOutputFile(extra_hard_deps[i]));
  }

  out_ << "\n";
  return " | " + stamp_file_string;
}

FileTemplate NinjaTargetWriter::GetOutputTemplate() const {
  const Target::FileList& outputs = target_->action_values().outputs();
  std::vector<std::string> output_template_args;
  for (size_t i = 0; i < outputs.size(); i++) {
    // All outputs should be in the output dir.
    output_template_args.push_back(
        RemovePrefix(outputs[i].value(),
                     settings_->build_settings()->build_dir().value()));
  }
  return FileTemplate(output_template_args);
}
