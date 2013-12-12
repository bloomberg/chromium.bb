// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/gyp_script_target_writer.h"

#include "tools/gn/builder_record.h"
#include "tools/gn/err.h"
#include "tools/gn/file_template.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"

// Write script targets as GYP actions that just invoke Ninja. This allows us
// to not have to worry about duplicating the precise GN script execution
// semantices in GYP for each platform (GYP varies a bit).

GypScriptTargetWriter::GypScriptTargetWriter(const TargetGroup& group,
                                             const Toolchain* toolchain,
                                             const SourceDir& gyp_dir,
                                             std::ostream& out)
    : GypTargetWriter(group.debug->item()->AsTarget(), toolchain,
                      gyp_dir, out) {
}

GypScriptTargetWriter::~GypScriptTargetWriter() {
}

void GypScriptTargetWriter::Run() {
  int indent = 4;
  std::string name = helper_.GetNameForTarget(target_);

  // Put the ninja build for this script target in this directory.
  SourceDir ninja_dir(GetTargetOutputDir(target_).value() + name + "_ninja/");

  Indent(indent) << "{\n";

  Indent(indent + kExtraIndent) << "'target_name': '" << name << "',\n";
  Indent(indent + kExtraIndent) << "'type': 'none',\n";
  Indent(indent + kExtraIndent) << "'actions': [{\n";

  Indent(indent + kExtraIndent * 2) << "'action_name': '" << name
                                    << " action',\n";

  Indent(indent + kExtraIndent * 2) << "'action': [\n";
  Indent(indent + kExtraIndent * 3) << "'ninja',\n";
  Indent(indent + kExtraIndent * 3) << "'-C', '";
  path_output_.WriteDir(out_, ninja_dir, PathOutput::DIR_NO_LAST_SLASH);
  out_ << "',\n";
  Indent(indent + kExtraIndent * 3) << "'" << name << "',\n";
  Indent(indent + kExtraIndent * 2) << "],\n";

  WriteActionInputs(indent + kExtraIndent * 2);
  WriteActionOutputs(indent + kExtraIndent * 2);

  Indent(indent + kExtraIndent) << "}],\n";
  Indent(indent) << "},\n";
}

void GypScriptTargetWriter::WriteActionInputs(int indent) {
  Indent(indent) << "'inputs': [\n";

  // Write everything that should be considered an input for dependency
  // purposes, which is all sources as well as the prereqs.
  const Target::FileList& sources = target_->sources();
  for (size_t i = 0; i < sources.size(); i++) {
    Indent(indent + kExtraIndent) << "'";
    path_output_.WriteFile(out_, sources[i]);
    out_ << "',\n";
  }

  const Target::FileList& prereqs = target_->source_prereqs();
  for (size_t i = 0; i < prereqs.size(); i++) {
    Indent(indent + kExtraIndent) << "'";
    path_output_.WriteFile(out_, prereqs[i]);
    out_ << "',\n";
  }

  Indent(indent) << "],\n";
}

void GypScriptTargetWriter::WriteActionOutputs(int indent) {
  Indent(indent) << "'outputs': [\n";

  const Target::FileList& sources = target_->sources();
  if (sources.empty()) {
    // Just write outputs directly if there are no sources.
    const Target::FileList& output = target_->script_values().outputs();
    for (size_t output_i = 0; output_i < output.size(); output_i++) {
      Indent(indent + kExtraIndent) << "'";
      path_output_.WriteFile(out_, output[output_i]);
      out_ << "',\n";
    }
  } else {
    // There are sources, the outputs should be a template to apply to each.
    FileTemplate output_template = FileTemplate::GetForTargetOutputs(target_);

    std::vector<std::string> output;
    for (size_t source_i = 0; source_i < sources.size(); source_i++) {
      output_template.ApplyString(sources[source_i].value(), &output);
      for (size_t output_i = 0; output_i < output.size(); output_i++) {
        Indent(indent + kExtraIndent) << "'";
        path_output_.WriteFile(out_, SourceFile(output[output_i]));
        out_ << "',\n";
      }
    }
  }

  Indent(indent) << "],\n";
}
