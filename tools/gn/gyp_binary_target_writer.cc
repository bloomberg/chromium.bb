// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/gyp_binary_target_writer.h"

#include <set>

#include "base/logging.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"

namespace {

// This functor is used to capture the output of RecursiveTargetConfigToStream
// in an vector.
struct StringAccumulator {
  StringAccumulator(std::vector<std::string>* result_in) : result(result_in) {}

  void operator()(const std::string& s, std::ostream&) const {
    result->push_back(s);
  }

  std::vector<std::string>* result;
};

// Writes the given array with the given name. The indent should be the
// indenting for the name, the values will be indented 2 spaces from there.
// Writes nothing if there is nothing in the array.
void WriteArray(std::ostream& out,
                const char* name,
                const std::vector<std::string>& values,
                int indent) {
  if (values.empty())
    return;

  EscapeOptions options;
  options.mode = ESCAPE_JSON;

  std::string indent_str(indent, ' ');
  out << indent_str << "'" << name << "': [";
  for (size_t i = 0; i < values.size(); i++) {
    out << " '";
    EscapeStringToStream(out, values[i], options);
    out << "',";
  }
  out << " ],\n";
}

struct StringWriter {
  StringWriter() {
    options.mode = ESCAPE_JSON;
  }

  void operator()(const std::string& s, std::ostream& out) const {
    out << " '";
    EscapeStringToStream(out, s, options);
    out << "',";
  }

  EscapeOptions options;
};

struct IncludeWriter {
  IncludeWriter(const GypHelper& h) : helper(h) {
    options.mode = ESCAPE_JSON;
  }

  void operator()(const SourceDir& d, std::ostream& out) const {
    out << " '";
    EscapeStringToStream(out, helper.GetDirReference(d, false), options);
    out << "',";
  }

  const GypHelper& helper;
  EscapeOptions options;
};

// Returns the value from the already-filled in cflags_* for the optimization
// level to set in the GYP file. Additionally, this removes the flag from the
// given vector so we don't get duplicates.
std::string GetVCOptimization(std::vector<std::string>* cflags) {
  // Searches for the "/O?" option and returns the corresponding GYP value.
  for (size_t i = 0; i < cflags->size(); i++) {
    const std::string& cur = (*cflags)[i];
    if (cur.size() == 3 && cur[0] == '/' && cur[1] == 'O') {
      char level = cur[2];
      cflags->erase(cflags->begin() + i);  // Invalidates |cur|!
      switch (level) {
        case 'd': return "'0'";
        case '1': return "'1'";
        case '2': return "'2'";
        case 'x': return "'3'";
        default:  return "'2'";
      }
    }
  }
  return "'2'";  // Default value.
}

const int kExtraIndent = 2;

}  // namespace

GypBinaryTargetWriter::GypBinaryTargetWriter(const Target* debug_target,
                                             const Target* release_target,
                                             const Target* debug_host_target,
                                             const Target* release_host_target,
                                             std::ostream& out)
    : GypTargetWriter(debug_target, out),
      release_target_(release_target),
      debug_host_target_(debug_host_target),
      release_host_target_(release_host_target) {
}

GypBinaryTargetWriter::~GypBinaryTargetWriter() {
}

void GypBinaryTargetWriter::Run() {
  int indent = 4;

  Indent(indent) << "{\n";

  WriteName(indent + kExtraIndent);
  WriteType(indent + kExtraIndent);

  if (target_->settings()->IsLinux()) {
    WriteLinuxConfiguration(indent + kExtraIndent);
  } else if (target_->settings()->IsWin()) {
    WriteVCConfiguration(indent + kExtraIndent);
  } else if (target_->settings()->IsMac()) {
    // TODO(brettw) mac.
    NOTREACHED();
    //WriteMacConfiguration();
  }

  Indent(indent) << "},\n";
}

std::ostream& GypBinaryTargetWriter::Indent(int spaces) {
  const char kSpaces[81] =
      "                                        "
      "                                        ";
  CHECK(static_cast<size_t>(spaces) <= arraysize(kSpaces) - 1);
  out_.write(kSpaces, spaces);
  return out_;
}

void GypBinaryTargetWriter::WriteName(int indent) {
  std::string name = helper_.GetNameForTarget(target_);
  Indent(indent) << "'target_name': '" << name << "',\n";

  std::string product_name;
  if (target_->output_name().empty())
    product_name = target_->label().name();
  else
    product_name = name;

  // TODO(brettw) GN knows not to prefix targets starting with "lib" with
  // another "lib" on Linux, but GYP doesn't. We need to rename applicable
  // targets here.

  Indent(indent) << "'product_name': '" << product_name << "',\n";
}

void GypBinaryTargetWriter::WriteType(int indent) {
  Indent(indent) << "'type': ";
  switch (target_->output_type()) {
    case Target::EXECUTABLE:
      out_ << "'executable',\n";
      break;
    case Target::STATIC_LIBRARY:
      out_ << "'static_library',\n";
      break;
    case Target::SHARED_LIBRARY:
      out_ << "'shared_library',\n";
      break;
    case Target::SOURCE_SET:
      out_ << "'static_library',\n";  // TODO(brettw) fixme.
      break;
    default:
      NOTREACHED();
  }

  if (target_->hard_dep())
    Indent(indent) << "'hard_dependency': 1,\n";
}

void GypBinaryTargetWriter::WriteVCConfiguration(int indent) {
  Indent(indent) << "'configurations': {\n";
  Indent(indent + kExtraIndent) << "'Debug': {\n";
  WriteDefines(target_, indent + kExtraIndent * 2);
  WriteIncludes(target_, indent + kExtraIndent * 2);
  WriteVCFlags(target_, indent + kExtraIndent * 2);
  Indent(indent + kExtraIndent) << "},\n";
  Indent(indent + kExtraIndent) << "'Release': {\n";
  WriteDefines(release_target_, indent + kExtraIndent * 2);
  WriteIncludes(release_target_, indent + kExtraIndent * 2);
  WriteVCFlags(release_target_, indent + kExtraIndent * 2);
  Indent(indent + kExtraIndent) << "},\n";
  Indent(indent + kExtraIndent) << "'Debug_x64': {},\n";
  Indent(indent + kExtraIndent) << "'Release_x64': {},\n";
  Indent(indent) << "},\n";

  WriteSources(target_, indent);
  WriteDeps(target_, indent);
}

void GypBinaryTargetWriter::WriteDefines(const Target* target, int indent) {
  Indent(indent) << "'defines': [";
  RecursiveTargetConfigToStream<std::string>(target, &ConfigValues::defines,
                                             StringWriter(), out_);
  out_ << " ],\n";
}

void GypBinaryTargetWriter::WriteIncludes(const Target* target, int indent) {
  Indent(indent) << "'include_dirs': [";
  RecursiveTargetConfigToStream<SourceDir>(target, &ConfigValues::include_dirs,
                                           IncludeWriter(helper_), out_);
  out_ << " ],\n";
}

void GypBinaryTargetWriter::WriteVCFlags(const Target* target, int indent) {
  // C flags.
  Indent(indent) << "'msvs_settings': {\n";
  Indent(indent + kExtraIndent) << "'VCCLCompilerTool': {\n";

  std::vector<std::string> cflags;
  StringAccumulator acc(&cflags);
  RecursiveTargetConfigToStream<std::string>(target, &ConfigValues::cflags,
                                             acc, out_);
  // GYP always uses the VC optimization flag to add a /O? on Visual Studio.
  // This can produce duplicate values. So look up the GYP value corresponding
  // to the flags used, and set the same one.
  std::string optimization = GetVCOptimization(&cflags);
  WriteArray(out_, "AdditionalOptions", cflags, 14);
  // TODO(brettw) cflags_c and cflags_cc!
  Indent(indent + kExtraIndent * 2) << "'Optimization': "
                                    << optimization << ",\n";
  Indent(indent + kExtraIndent) << "},\n";

  // Linker tool stuff.
  Indent(indent + kExtraIndent) << "'VCLinkerTool': {\n";

  // ...Library dirs.
  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_JSON;
  const OrderedSet<SourceDir> all_lib_dirs = target->all_lib_dirs();
  if (!all_lib_dirs.empty()) {
    Indent(indent + kExtraIndent * 2) << "'AdditionalLibraryDirectories': [";
    for (size_t i = 0; i < all_lib_dirs.size(); i++) {
      out_ << " '";
      EscapeStringToStream(out_,
                           helper_.GetDirReference(all_lib_dirs[i], false),
                           escape_options);
      out_ << "',";
    }
    out_ << " ],\n";
  }

  // ...Libraries.
  const OrderedSet<std::string> all_libs = target->all_libs();
  if (!all_libs.empty()) {
    Indent(indent + kExtraIndent * 2) << "'AdditionalDependencies': [";
    for (size_t i = 0; i < all_libs.size(); i++) {
      out_ << " '";
      EscapeStringToStream(out_, all_libs[i], escape_options);
      out_ << "',";
    }
    out_ << " ],\n";
  }

  // ...LD flags.
  // TODO(brettw) EnableUAC defaults to on and needs to be set. Also
  // UACExecutionLevel and UACUIAccess depends on that and defaults to 0/false.
  std::vector<std::string> ldflags;
  acc.result = &ldflags;
  RecursiveTargetConfigToStream<std::string>(target, &ConfigValues::ldflags,
                                             acc, out_);
  WriteArray(out_, "AdditionalOptions", ldflags, 14);
  Indent(indent + kExtraIndent) << "},\n";
  Indent(indent) << "},\n";
}

void GypBinaryTargetWriter::WriteLinuxConfiguration(int indent) {
  // The Linux stuff works differently. On Linux we support cross-compiles and
  // all ninja generators know to look for target conditions. Other platforms'
  // generators don't all do this, so we can't have the same GYP structure.
  Indent(indent) << "'target_conditions': [\n";
  // The host toolset is configured for the current computer, we will only have
  // this when doing cross-compiles.
  if (debug_host_target_ && release_host_target_) {
    Indent(indent + kExtraIndent) << "['_toolset == \"host\"', {\n";
    Indent(indent + kExtraIndent * 2) << "'configurations': {\n";
    Indent(indent + kExtraIndent * 3) << "'Debug': {\n";
    WriteDefines(debug_host_target_, indent + kExtraIndent * 4);
    WriteIncludes(debug_host_target_, indent + kExtraIndent * 4);
    WriteLinuxFlags(debug_host_target_, indent + kExtraIndent * 4);
    Indent(indent + kExtraIndent * 3) << "},\n";
    Indent(indent + kExtraIndent * 3) << "'Release': {\n";
    WriteDefines(release_host_target_, indent + kExtraIndent * 4);
    WriteIncludes(release_host_target_, indent + kExtraIndent * 4);
    WriteLinuxFlags(release_host_target_, indent + kExtraIndent * 4);
    Indent(indent + kExtraIndent * 3) << "},\n";
    Indent(indent + kExtraIndent * 2) << "}\n";

    // The sources and deps are per-toolset but shared between debug & release.
    WriteSources(debug_host_target_, indent + kExtraIndent * 2);

    Indent(indent + kExtraIndent) << "],\n";
  }

  // The target toolset is the "regular" one.
  Indent(indent + kExtraIndent) << "['_toolset == \"target\"', {\n";
  Indent(indent + kExtraIndent * 2) << "'configurations': {\n";
  Indent(indent + kExtraIndent * 3) << "'Debug': {\n";
  WriteDefines(target_, indent + kExtraIndent * 4);
  WriteIncludes(target_, indent + kExtraIndent * 4);
  WriteLinuxFlags(target_, indent + kExtraIndent * 4);
  Indent(indent + kExtraIndent * 3) << "},\n";
  Indent(indent + kExtraIndent * 3) << "'Release': {\n";
  WriteDefines(release_target_, indent + kExtraIndent * 4);
  WriteIncludes(release_target_, indent + kExtraIndent * 4);
  WriteLinuxFlags(release_target_, indent + kExtraIndent * 4);
  Indent(indent + kExtraIndent * 3) << "},\n";
  Indent(indent + kExtraIndent * 2) << "},\n";

  WriteSources(target_, indent + kExtraIndent * 2);

  Indent(indent + kExtraIndent) << "},],\n";
  Indent(indent) << "],\n";

  // TODO(brettw) The deps can probably vary based on the toolset. However,
  // GYP doesn't seem to properly expand the deps when we put this inside the
  // toolset condition above. Either we need to decide the deps never vary, or
  // make it work in the conditions above.
  WriteDeps(target_, indent);
}

void GypBinaryTargetWriter::WriteLinuxFlags(const Target* target, int indent) {
#define WRITE_FLAGS(name) \
    Indent(indent) << "'" #name "': ["; \
    RecursiveTargetConfigToStream<std::string>(target, &ConfigValues::name, \
                                               StringWriter(), out_); \
    out_ << " ],\n";
  WRITE_FLAGS(cflags)
  WRITE_FLAGS(cflags_c)
  WRITE_FLAGS(cflags_cc)
  WRITE_FLAGS(cflags_objc)
  WRITE_FLAGS(cflags_objcc)
#undef WRITE_FLAGS

  // Put libraries and library directories in with ldflags.
  Indent(indent) << "'ldflags': ["; \
  RecursiveTargetConfigToStream<std::string>(target, &ConfigValues::ldflags,
                                             StringWriter(), out_);

  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_JSON;
  const OrderedSet<SourceDir> all_lib_dirs = target->all_lib_dirs();
  for (size_t i = 0; i < all_lib_dirs.size(); i++) {
    out_ << " '-L";
    EscapeStringToStream(out_,
                         helper_.GetDirReference(all_lib_dirs[i], false),
                         escape_options);
    out_ << "',";
  }

  const OrderedSet<std::string> all_libs = target->all_libs();
  for (size_t i = 0; i < all_libs.size(); i++) {
    out_ << " '-l";
    EscapeStringToStream(out_, all_libs[i], escape_options);
    out_ << "',";
  }
  out_ << " ],\n";
}

void GypBinaryTargetWriter::WriteSources(const Target* target, int indent) {
  Indent(indent) << "'sources': [\n";

  const Target::FileList& sources = target->sources();
  for (size_t i = 0; i < sources.size(); i++) {
    const SourceFile& input_file = sources[i];
    Indent(indent + kExtraIndent) << "'" << helper_.GetFileReference(input_file)
                                  << "',\n";
  }

  Indent(indent) << "],\n";
}

void GypBinaryTargetWriter::WriteDeps(const Target* target, int indent) {
  const std::vector<const Target*>& deps = target->deps();
  if (deps.empty())
    return;

  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_JSON;

  Indent(indent) << "'dependencies': [\n";
  for (size_t i = 0; i < deps.size(); i++) {
    Indent(indent + kExtraIndent) << "'";
    EscapeStringToStream(out_, helper_.GetFullRefForTarget(deps[i]),
                         escape_options);
    out_ << "',\n";
  }
  Indent(indent) << "],\n";
}
