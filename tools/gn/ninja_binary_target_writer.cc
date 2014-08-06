// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_binary_target_writer.h"

#include <set>

#include "base/strings/string_util.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/string_utils.h"

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
  IncludeWriter(PathOutput& path_output, const NinjaHelper& h)
      : helper(h),
        path_output_(path_output) {
  }
  ~IncludeWriter() {
  }

  void operator()(const SourceDir& d, std::ostream& out) const {
    out << " -I";
    path_output_.WriteDir(out, d, PathOutput::DIR_NO_LAST_SLASH);
  }

  const NinjaHelper& helper;
  PathOutput& path_output_;
};

Toolchain::ToolType GetToolTypeForTarget(const Target* target) {
  switch (target->output_type()) {
    case Target::STATIC_LIBRARY:
      return Toolchain::TYPE_ALINK;
    case Target::SHARED_LIBRARY:
      return Toolchain::TYPE_SOLINK;
    case Target::EXECUTABLE:
      return Toolchain::TYPE_LINK;
    default:
      return Toolchain::TYPE_NONE;
  }
}

}  // namespace

NinjaBinaryTargetWriter::NinjaBinaryTargetWriter(const Target* target,
                                                 const Toolchain* toolchain,
                                                 std::ostream& out)
    : NinjaTargetWriter(target, toolchain, out),
      tool_type_(GetToolTypeForTarget(target)){
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
  // Defines.
  out_ << "defines =";
  RecursiveTargetConfigToStream<std::string>(target_, &ConfigValues::defines,
                                             DefineWriter(), out_);
  out_ << std::endl;

  // Include directories.
  out_ << "includes =";
  RecursiveTargetConfigToStream<SourceDir>(target_, &ConfigValues::include_dirs,
                                           IncludeWriter(path_output_, helper_),
                                           out_);

  out_ << std::endl;

  // C flags and friends.
  EscapeOptions flag_escape_options = GetFlagOptions();
#define WRITE_FLAGS(name) \
    out_ << #name " ="; \
    RecursiveTargetConfigStringsToStream(target_, &ConfigValues::name, \
                                         flag_escape_options, out_); \
    out_ << std::endl;

  WRITE_FLAGS(cflags)
  WRITE_FLAGS(cflags_c)
  WRITE_FLAGS(cflags_cc)
  WRITE_FLAGS(cflags_objc)
  WRITE_FLAGS(cflags_objcc)

#undef WRITE_FLAGS

  // Write some variables about the target for the toolchain definition to use.
  out_ << "target_name = " << target_->label().name() << std::endl;
  out_ << "target_out_dir = ";
  path_output_.WriteDir(out_, helper_.GetTargetOutputDir(target_),
                        PathOutput::DIR_NO_LAST_SLASH);
  out_ << std::endl;
  out_ << "root_out_dir = ";
  path_output_.WriteDir(out_, target_->settings()->toolchain_output_subdir(),
                        PathOutput::DIR_NO_LAST_SLASH);
  out_ << std::endl << std::endl;
}

void NinjaBinaryTargetWriter::WriteSources(
    std::vector<OutputFile>* object_files) {
  const Target::FileList& sources = target_->sources();
  object_files->reserve(sources.size());

  std::string implicit_deps =
      WriteInputDepsStampAndGetDep(std::vector<const Target*>());

  for (size_t i = 0; i < sources.size(); i++) {
    const SourceFile& input_file = sources[i];

    SourceFileType input_file_type = GetSourceFileType(input_file);
    if (input_file_type == SOURCE_UNKNOWN)
      continue;  // Skip unknown file types.
    if (input_file_type == SOURCE_O) {
      // Object files just get passed to the output and not compiled.
      object_files->push_back(helper_.GetOutputFileForSource(
          target_, input_file, input_file_type));
      continue;
    }
    std::string command =
        helper_.GetRuleForSourceType(settings_, input_file_type);
    if (command.empty())
      continue;  // Skip files not needing compilation.

    OutputFile output_file = helper_.GetOutputFileForSource(
        target_, input_file, input_file_type);
    object_files->push_back(output_file);

    out_ << "build ";
    path_output_.WriteFile(out_, output_file);
    out_ << ": " << command << " ";
    path_output_.WriteFile(out_, input_file);
    out_ << implicit_deps << std::endl;
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLinkerStuff(
    const std::vector<OutputFile>& object_files) {
  // Manifest file on Windows.
  // TODO(brettw) this seems not to be necessary for static libs, skip in
  // that case?
  OutputFile windows_manifest;
  if (settings_->IsWin()) {
    windows_manifest = helper_.GetTargetOutputDir(target_);
    windows_manifest.value().append(target_->label().name());
    windows_manifest.value().append(".intermediate.manifest");
    out_ << "manifests = ";
    path_output_.WriteFile(out_, windows_manifest);
    out_ << std::endl;
  }

  const Toolchain::Tool& tool = toolchain_->GetTool(tool_type_);
  WriteLinkerFlags(tool, windows_manifest);
  WriteLibs(tool);

  // The external output file is the one that other libs depend on.
  OutputFile external_output_file = helper_.GetTargetOutputFile(target_);

  // The internal output file is the "main thing" we think we're making. In
  // the case of shared libraries, this is the shared library and the external
  // output file is the import library. In other cases, the internal one and
  // the external one are the same.
  OutputFile internal_output_file;
  if (target_->output_type() == Target::SHARED_LIBRARY) {
    if (settings_->IsWin()) {
      internal_output_file.value() =
          target_->settings()->toolchain_output_subdir().value();
      internal_output_file.value().append(target_->label().name());
      internal_output_file.value().append(".dll");
    } else {
      internal_output_file = external_output_file;
    }
  } else {
    internal_output_file = external_output_file;
  }

  // In Python see "self.ninja.build(output, command, input,"
  WriteLinkCommand(external_output_file, internal_output_file, object_files);

  if (target_->output_type() == Target::SHARED_LIBRARY) {
    // The shared object name doesn't include a path.
    out_ << "  soname = ";
    out_ << FindFilename(&internal_output_file.value());
    out_ << std::endl;

    out_ << "  lib = ";
    path_output_.WriteFile(out_, internal_output_file);
    out_ << std::endl;

    if (settings_->IsWin()) {
      out_ << "  dll = ";
      path_output_.WriteFile(out_, internal_output_file);
      out_ << std::endl;
    }

    if (settings_->IsWin()) {
      out_ << "  implibflag = /IMPLIB:";
      path_output_.WriteFile(out_, external_output_file);
      out_ << std::endl;
    }

    // TODO(brettw) postbuild steps.
    if (settings_->IsMac())
      out_ << "  postbuilds = $ && (export BUILT_PRODUCTS_DIR=/Users/brettw/prj/src/out/gn; export CONFIGURATION=Debug; export DYLIB_INSTALL_NAME_BASE=@rpath; export EXECUTABLE_NAME=libbase.dylib; export EXECUTABLE_PATH=libbase.dylib; export FULL_PRODUCT_NAME=libbase.dylib; export LD_DYLIB_INSTALL_NAME=@rpath/libbase.dylib; export MACH_O_TYPE=mh_dylib; export PRODUCT_NAME=base; export PRODUCT_TYPE=com.apple.product-type.library.dynamic; export SDKROOT=/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.7.sdk; export SRCROOT=/Users/brettw/prj/src/out/gn/../../base; export SOURCE_ROOT=\"$${SRCROOT}\"; export TARGET_BUILD_DIR=/Users/brettw/prj/src/out/gn; export TEMP_DIR=\"$${TMPDIR}\"; (cd ../../base && ../build/mac/strip_from_xcode); G=$$?; ((exit $$G) || rm -rf libbase.dylib) && exit $$G)";
  }

  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLinkerFlags(
    const Toolchain::Tool& tool,
    const OutputFile& windows_manifest) {
  out_ << "ldflags =";

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
      out_ << " " << tool.lib_dir_prefix;
      lib_path_output.WriteDir(out_, all_lib_dirs[i],
                               PathOutput::DIR_NO_LAST_SLASH);
    }
  }

  // Append manifest flag on Windows to reference our file.
  // HACK ERASEME BRETTW FIXME
  if (settings_->IsWin()) {
    out_ << " /MANIFEST /ManifestFile:";
    path_output_.WriteFile(out_, windows_manifest);
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLibs(const Toolchain::Tool& tool) {
  out_ << "libs =";

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
      out_ << " " << tool.lib_prefix;
      EscapeStringToStream(out_, all_libs[i], lib_escape_opts);
    }
  }
  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteLinkCommand(
    const OutputFile& external_output_file,
    const OutputFile& internal_output_file,
    const std::vector<OutputFile>& object_files) {
  out_ << "build ";
  path_output_.WriteFile(out_, internal_output_file);
  if (external_output_file != internal_output_file) {
    out_ << " ";
    path_output_.WriteFile(out_, external_output_file);
  }
  out_ << ": "
       << helper_.GetRulePrefix(target_->settings())
       << Toolchain::ToolTypeToName(tool_type_);

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

  // Libs.
  for (size_t i = 0; i < linkable_deps.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, helper_.GetTargetOutputFile(linkable_deps[i]));
  }

  // Append data dependencies as implicit dependencies.
  WriteImplicitDependencies(non_linkable_deps);

  out_ << std::endl;
}

void NinjaBinaryTargetWriter::WriteSourceSetStamp(
    const std::vector<OutputFile>& object_files) {
  // The stamp rule for source sets is generally not used, since targets that
  // depend on this will reference the object files directly. However, writing
  // this rule allows the user to type the name of the target and get a build
  // which can be convenient for development.
  out_ << "build ";
  path_output_.WriteFile(out_, helper_.GetTargetOutputFile(target_));
  out_ << ": "
       << helper_.GetRulePrefix(target_->settings())
       << "stamp";

  UniqueVector<OutputFile> extra_object_files;
  UniqueVector<const Target*> linkable_deps;
  UniqueVector<const Target*> non_linkable_deps;
  GetDeps(&extra_object_files, &linkable_deps, &non_linkable_deps);

  // The classifier should never put extra object files in a source set:
  // any source sets that we depend on should appear in our non-linkable
  // deps instead.
  DCHECK(extra_object_files.empty());

  for (size_t i = 0; i < object_files.size(); i++) {
    out_ << " ";
    path_output_.WriteFile(out_, object_files[i]);
  }

  // Append data dependencies as implicit dependencies.
  WriteImplicitDependencies(non_linkable_deps);

  out_ << std::endl;
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
      for (size_t i = 0; i < dep->sources().size(); i++) {
        SourceFileType input_file_type = GetSourceFileType(dep->sources()[i]);
        if (input_file_type != SOURCE_UNKNOWN &&
            input_file_type != SOURCE_H) {
          // Note we need to specify the target as the source_set target
          // itself, since this is used to prefix the object file name.
          extra_object_files->push_back(helper_.GetOutputFileForSource(
              dep, dep->sources()[i], input_file_type));
        }
      }
    }
  } else if (can_link_libs && dep->IsLinkable()) {
    linkable_deps->push_back(dep);
  } else {
    non_linkable_deps->push_back(dep);
  }
}

void NinjaBinaryTargetWriter::WriteImplicitDependencies(
    const UniqueVector<const Target*>& non_linkable_deps) {
  const std::vector<SourceFile>& data = target_->data();
  if (!non_linkable_deps.empty() || !data.empty()) {
    out_ << " ||";

    // Non-linkable targets.
    for (size_t i = 0; i < non_linkable_deps.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(out_,
                             helper_.GetTargetOutputFile(non_linkable_deps[i]));
    }

    // Data files.
    const std::vector<SourceFile>& data = target_->data();
    for (size_t i = 0; i < data.size(); i++) {
      out_ << " ";
      path_output_.WriteFile(out_, data[i]);
    }
  }
}
