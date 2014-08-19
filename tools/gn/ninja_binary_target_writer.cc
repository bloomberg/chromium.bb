// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_binary_target_writer.h"

#include <set>

#include "base/strings/string_util.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/ninja_utils.h"
#include "tools/gn/settings.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/substitution_writer.h"
#include "tools/gn/target.h"

namespace {

// Returns the proper escape options for writing compiler and linker flags.
EscapeOptions GetFlagOptions() {
  EscapeOptions opts;
  opts.mode = ESCAPE_NINJA_COMMAND;

  // Some flag strings are actually multiple flags that expect to be just
  // added to the command line. We assume that quoting is done by the
  // buildfiles if it wants such things quoted.
  opts.inhibit_quoting = true;

  return opts;
}

struct DefineWriter {
  DefineWriter() {
    options.mode = ESCAPE_NINJA_COMMAND;
  }

  void operator()(const std::string& s, std::ostream& out) const {
    out << " -D";
    EscapeStringToStream(out, s, options);
  }

  EscapeOptions options;
};

struct IncludeWriter {
  IncludeWriter(PathOutput& path_output) : path_output_(path_output) {
  }
  ~IncludeWriter() {
  }

  void operator()(const SourceDir& d, std::ostream& out) const {
    out << " -I";
    path_output_.WriteDir(out, d, PathOutput::DIR_NO_LAST_SLASH);
  }

  PathOutput& path_output_;
};

}  // namespace

NinjaBinaryTargetWriter::NinjaBinaryTargetWriter(const Target* target,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, out),
      tool_(target->toolchain()->GetToolForTargetFinalOutput(target)) {
}

NinjaBinaryTargetWriter::~NinjaBinaryTargetWriter() {
}

void NinjaBinaryTargetWriter::Run() {
  WriteCompilerVars();

  std::vector<OutputFile> obj_files;
  WriteSources(&obj_files);

  if (target_->output_type() == Target::SOURCE_SET)
    WriteSourceSetStamp(obj_files);
  else
    WriteLinkerStuff(obj_files);
}

void NinjaBinaryTargetWriter::WriteCompilerVars() {
  const SubstitutionBits& subst = target_->toolchain()->substitution_bits();

  // Defines.
  if (subst.used[SUBSTITUTION_DEFINES]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_DEFINES] << " =";
    RecursiveTargetConfigToStream<std::string>(
        target_, &ConfigValues::defines, DefineWriter(), out_);
    out_ << std::endl;
  }

  // Include directories.
  if (subst.used[SUBSTITUTION_INCLUDE_DIRS]) {
    out_ << kSubstitutionNinjaNames[SUBSTITUTION_INCLUDE_DIRS] << " =";
    RecursiveTargetConfigToStream<SourceDir>(
        target_, &ConfigValues::include_dirs,
        IncludeWriter(path_output_), out_);
    out_ << std::endl;
  }

  // C flags and friends.
  EscapeOptions flag_escape_options = GetFlagOptions();
#define WRITE_FLAGS(name, subst_enum) \
    if (subst.used[subst_enum]) { \
      out_ << kSubstitutionNinjaNames[subst_enum] << " ="; \
      RecursiveTargetConfigStringsToStream(target_, &ConfigValues::name, \
                                           flag_escape_options, out_); \
      out_ << std::endl; \
    }

  WRITE_FLAGS(cflags, SUBSTITUTION_CFLAGS)
  WRITE_FLAGS(cflags_c, SUBSTITUTION_CFLAGS_C)
  WRITE_FLAGS(cflags_cc, SUBSTITUTION_CFLAGS_CC)
  WRITE_FLAGS(cflags_objc, SUBSTITUTION_CFLAGS_OBJC)
  WRITE_FLAGS(cflags_objcc, SUBSTITUTION_CFLAGS_OBJCC)

#undef WRITE_FLAGS

  WriteSharedVars(subst);
}

void NinjaBinaryTargetWriter::WriteSources(
    std::vector<OutputFile>* object_files) {
  const Target::FileList& sources = target_->sources();
  object_files->reserve(sources.size());

  std::string implicit_deps =
      WriteInputDepsStampAndGetDep(std::vector<const Target*>());

  std::string rule_prefix = GetNinjaRulePrefixForToolchain(settings_);

  std::vector<OutputFile> tool_outputs;  // Prevent reallocation in loop.
  for (size_t i = 0; i < sources.size(); i++) {
    Toolchain::ToolType tool_type = Toolchain::TYPE_NONE;
    if (!GetOutputFilesForSource(target_, sources[i],
                                 &tool_type, &tool_outputs))
      continue;  // No output for this source.

    if (tool_type != Toolchain::TYPE_NONE) {
      out_ << "build";
      path_output_.WriteFiles(out_, tool_outputs);
      out_ << ": " << rule_prefix << Toolchain::ToolTypeToName(tool_type);
      out_ << " ";
      path_output_.WriteFile(out_, sources[i]);
      out_ << implicit_deps << std::endl;
    }

    // It's theoretically possible for a compiler to produce more than one
    // output, but we'll only link to the first output.
    object_files->push_back(tool_outputs[0]);
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLinkerStuff(
    const std::vector<OutputFile>& object_files) {
  std::vector<OutputFile> output_files;
  SubstitutionWriter::ApplyListToLinkerAsOutputFile(
      target_, tool_, tool_->outputs(), &output_files);

  out_ << "build";
  path_output_.WriteFiles(out_, output_files);

  out_ << ": "
       << GetNinjaRulePrefixForToolchain(settings_)
       << Toolchain::ToolTypeToName(
              target_->toolchain()->GetToolTypeForTargetFinalOutput(target_));

  UniqueVector<OutputFile> extra_object_files;
  UniqueVector<const Target*> linkable_deps;
  UniqueVector<const Target*> non_linkable_deps;
  GetDeps(&extra_object_files, &linkable_deps, &non_linkable_deps);

  // Object files.
  for (size_t i = 0; i < object_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, object_files[i]);
  }
  for (size_t i = 0; i < extra_object_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, extra_object_files[i]);
  }

  std::vector<OutputFile> implicit_deps;
  std::vector<OutputFile> solibs;

  for (size_t i = 0; i < linkable_deps.size(); i++) {
    const Target* cur = linkable_deps[i];

    // All linkable deps should have a link output file.
    DCHECK(!cur->link_output_file().value().empty())
        << "No link output file for "
        << target_->label().GetUserVisibleName(false);

    if (cur->dependency_output_file().value() !=
        cur->link_output_file().value()) {
      // This is a shared library with separate link and deps files. Save for
      // later.
      implicit_deps.push_back(cur->dependency_output_file());
      solibs.push_back(cur->link_output_file());
    } else {
      // Normal case, just link to this target.
      out_ << " ";
      path_output_.WriteFile(out_, cur->link_output_file());
    }
  }

  // Append implicit dependencies collected above.
  if (!implicit_deps.empty()) {
    out_ << " |";
    path_output_.WriteFiles(out_, implicit_deps);
  }

  // Append data dependencies as order-only dependencies.
  WriteOrderOnlyDependencies(non_linkable_deps);

  // End of the link "build" line.
  out_ << std::endl;

  // These go in the inner scope of the link line.
  WriteLinkerFlags();
  WriteLibs();
  WriteOutputExtension();
  WriteSolibs(solibs);
}

void NinjaBinaryTargetWriter::WriteLinkerFlags() {
  out_ << "  ldflags =";

  // First the ldflags from the target and its config.
  EscapeOptions flag_options = GetFlagOptions();
  RecursiveTargetConfigStringsToStream(target_, &ConfigValues::ldflags,
                                       flag_options, out_);

  // Followed by library search paths that have been recursively pushed
  // through the dependency tree.
  const OrderedSet<SourceDir> all_lib_dirs = target_->all_lib_dirs();
  if (!all_lib_dirs.empty()) {
    // Since we're passing these on the command line to the linker and not
    // to Ninja, we need to do shell escaping.
    PathOutput lib_path_output(path_output_.current_dir(),
                               ESCAPE_NINJA_COMMAND);
    for (size_t i = 0; i < all_lib_dirs.size(); i++) {
      out_ << " " << tool_->lib_dir_switch();
      lib_path_output.WriteDir(out_, all_lib_dirs[i],
                               PathOutput::DIR_NO_LAST_SLASH);
    }
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLibs() {
  out_ << "  libs =";

  // Libraries that have been recursively pushed through the dependency tree.
  EscapeOptions lib_escape_opts;
  lib_escape_opts.mode = ESCAPE_NINJA_COMMAND;
  const OrderedSet<std::string> all_libs = target_->all_libs();
  const std::string framework_ending(".framework");
  for (size_t i = 0; i < all_libs.size(); i++) {
    if (settings_->IsMac() && EndsWith(all_libs[i], framework_ending, false)) {
      // Special-case libraries ending in ".framework" on Mac. Add the
      // -framework switch and don't add the extension to the output.
      out_ << " -framework ";
      EscapeStringToStream(out_,
          all_libs[i].substr(0, all_libs[i].size() - framework_ending.size()),
          lib_escape_opts);
    } else {
      out_ << " " << tool_->lib_switch();
      EscapeStringToStream(out_, all_libs[i], lib_escape_opts);
    }
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteOutputExtension() {
  out_ << "  output_extension = ";
  if (target_->output_extension().empty()) {
    // Use the default from the tool.
    out_ << tool_->default_output_extension();
  } else {
    // Use the one specified in the target. Note that the one in the target
    // does not include the leading dot, so add that.
    out_ << "." << target_->output_extension();
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteSolibs(
    const std::vector<OutputFile>& solibs) {
  if (solibs.empty())
    return;

  out_ << "  solibs =";
  path_output_.WriteFiles(out_, solibs);
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteSourceSetStamp(
    const std::vector<OutputFile>& object_files) {
  // The stamp rule for source sets is generally not used, since targets that
  // depend on this will reference the object files directly. However, writing
  // this rule allows the user to type the name of the target and get a build
  // which can be convenient for development.
  UniqueVector<OutputFile> extra_object_files;
  UniqueVector<const Target*> linkable_deps;
  UniqueVector<const Target*> non_linkable_deps;
  GetDeps(&extra_object_files, &linkable_deps, &non_linkable_deps);

  // The classifier should never put extra object files in a source set:
  // any source sets that we depend on should appear in our non-linkable
  // deps instead.
  DCHECK(extra_object_files.empty());

  std::vector<OutputFile> order_only_deps;
  for (size_t i = 0; i < non_linkable_deps.size(); i++)
    order_only_deps.push_back(non_linkable_deps[i]->dependency_output_file());

  WriteStampForTarget(object_files, order_only_deps);
}

void NinjaBinaryTargetWriter::GetDeps(
    UniqueVector<OutputFile>* extra_object_files,
    UniqueVector<const Target*>* linkable_deps,
    UniqueVector<const Target*>* non_linkable_deps) const {
  const LabelTargetVector& deps = target_->deps();
  const UniqueVector<const Target*>& inherited =
      target_->inherited_libraries();

  // Normal deps.
  for (size_t i = 0; i < deps.size(); i++) {
    ClassifyDependency(deps[i].ptr, extra_object_files,
                       linkable_deps, non_linkable_deps);
  }

  // Inherited libraries.
  for (size_t i = 0; i < inherited.size(); i++) {
    ClassifyDependency(inherited[i], extra_object_files,
                       linkable_deps, non_linkable_deps);
  }

  // Data deps.
  const LabelTargetVector& datadeps = target_->datadeps();
  for (size_t i = 0; i < datadeps.size(); i++)
    non_linkable_deps->push_back(datadeps[i].ptr);
}

void NinjaBinaryTargetWriter::ClassifyDependency(
    const Target* dep,
    UniqueVector<OutputFile>* extra_object_files,
    UniqueVector<const Target*>* linkable_deps,
    UniqueVector<const Target*>* non_linkable_deps) const {
  // Only these types of outputs have libraries linked into them. Child deps of
  // static libraries get pushed up the dependency tree until one of these is
  // reached, and source sets don't link at all.
  bool can_link_libs =
      (target_->output_type() == Target::EXECUTABLE ||
       target_->output_type() == Target::SHARED_LIBRARY);

  if (dep->output_type() == Target::SOURCE_SET) {
    // Source sets have their object files linked into final targets (shared
    // libraries and executables). Intermediate static libraries and other
    // source sets just forward the dependency, otherwise the files in the
    // source set can easily get linked more than once which will cause
    // multiple definition errors.
    //
    // In the future, we may need a way to specify a "complete" static library
    // for cases where you want a static library that includes all source sets
    // (like if you're shipping that to customers to link against).
    if (target_->output_type() != Target::SOURCE_SET &&
        target_->output_type() != Target::STATIC_LIBRARY) {
      // Linking in a source set to an executable or shared library, copy its
      // object files.
      std::vector<OutputFile> tool_outputs;  // Prevent allocation in loop.
      for (size_t i = 0; i < dep->sources().size(); i++) {
        Toolchain::ToolType tool_type = Toolchain::TYPE_NONE;
        if (GetOutputFilesForSource(dep, dep->sources()[i], &tool_type,
                                    &tool_outputs)) {
          // Only link the first output if there are more than one.
          extra_object_files->push_back(tool_outputs[0]);
        }
      }
    }
  } else if (can_link_libs && dep->IsLinkable()) {
    linkable_deps->push_back(dep);
  } else {
    non_linkable_deps->push_back(dep);
  }
}

void NinjaBinaryTargetWriter::WriteOrderOnlyDependencies(
    const UniqueVector<const Target*>& non_linkable_deps) {
  const std::vector<SourceFile>& data = target_->data();
  if (!non_linkable_deps.empty() || !data.empty()) {
    out_ << " ||";

    // Non-linkable targets.
    for (size_t i = 0; i < non_linkable_deps.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(
          out_, non_linkable_deps[i]->dependency_output_file());
    }
  }
}

bool NinjaBinaryTargetWriter::GetOutputFilesForSource(
    const Target* target,
    const SourceFile& source,
    Toolchain::ToolType* computed_tool_type,
    std::vector<OutputFile>* outputs) const {
  outputs->clear();
  *computed_tool_type = Toolchain::TYPE_NONE;

  SourceFileType file_type = GetSourceFileType(source);
  if (file_type == SOURCE_UNKNOWN)
    return false;
  if (file_type == SOURCE_O) {
    // Object files just get passed to the output and not compiled.
    outputs->push_back(OutputFile(settings_->build_settings(), source));
    return true;
  }

  *computed_tool_type =
      target->toolchain()->GetToolTypeForSourceType(file_type);
  if (*computed_tool_type == Toolchain::TYPE_NONE)
    return false;  // No tool for this file (it's a header file or something).
  const Tool* tool = target->toolchain()->GetTool(*computed_tool_type);
  if (!tool)
    return false;  // Tool does not apply for this toolchain.file.

  // Figure out what output(s) this compiler produces.
  SubstitutionWriter::ApplyListToCompilerAsOutputFile(
      target, source, tool->outputs(), outputs);
  return !outputs->empty();
}
