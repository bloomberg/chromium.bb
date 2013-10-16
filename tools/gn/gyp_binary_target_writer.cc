// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/gyp_binary_target_writer.h"

#include <set>

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

}  // namespace

GypBinaryTargetWriter::GypBinaryTargetWriter(const Target* debug_target,
                                             const Target* release_target,
                                             std::ostream& out)
    : GypTargetWriter(debug_target, out),
      release_target_(release_target) {
}

GypBinaryTargetWriter::~GypBinaryTargetWriter() {
}

void GypBinaryTargetWriter::Run() {
  out_ << "    {\n";

  WriteName();
  WriteType();

  out_ << "      'configurations': {\n";
  out_ << "        'Debug': {\n";
  WriteFlags(target_);
  out_ << "        },\n";
  out_ << "        'Release': {\n";
  WriteFlags(release_target_);
  out_ << "        },\n";
  out_ << "        'Debug_x64': {},\n";
  out_ << "        'Release_x64': {},\n";
  out_ << "      },\n";

  WriteSources();
  WriteDeps();

  out_ << "    },\n";
}

void GypBinaryTargetWriter::WriteName() {
  std::string name = helper_.GetNameForTarget(target_);
  out_ << "      'target_name': '";
  out_ << name;
  out_ << "',\n";

  std::string product_name;
  if (target_->output_name().empty())
    product_name = target_->label().name();
  else
    product_name = name;

  // TODO(brettw) GN knows not to prefix targets starting with "lib" with
  // another "lib" on Linux, but GYP doesn't. We need to rename applicable
  // targets here.

  out_ << "      'product_name': '" << product_name << "',\n";
}

void GypBinaryTargetWriter::WriteType() {
  out_ << "      'type': ";
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
    out_ << "      'hard_dependency': 1,\n";
}

void GypBinaryTargetWriter::WriteFlags(const Target* target) {
  WriteDefines(target);
  WriteIncludes(target);
  if (target->settings()->IsWin())
    WriteVCFlags(target);
}

void GypBinaryTargetWriter::WriteDefines(const Target* target) {
  out_ << "          'defines': [";
  RecursiveTargetConfigToStream<std::string>(target, &ConfigValues::defines,
                                             StringWriter(), out_);
  out_ << " ],\n";
}

void GypBinaryTargetWriter::WriteIncludes(const Target* target) {
  out_ << "          'include_dirs': [";
  RecursiveTargetConfigToStream<SourceDir>(target, &ConfigValues::include_dirs,
                                           IncludeWriter(helper_), out_);
  out_ << " ],\n";
}

void GypBinaryTargetWriter::WriteVCFlags(const Target* target) {
  // C flags.
  out_ << "          'msvs_settings': {\n";
  out_ << "            'VCCLCompilerTool': {\n";

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
  out_ << "              'Optimization': " << optimization << ",\n";
  out_ << "            },\n";

  // Linker tool stuff.
  out_ << "            'VCLinkerTool': {\n";

  // ...Library dirs.
  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_JSON;
  const OrderedSet<SourceDir> all_lib_dirs = target->all_lib_dirs();
  if (!all_lib_dirs.empty()) {
    out_ << "              'AdditionalLibraryDirectories': [";
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
    out_ << "              'AdditionalDependencies': [";
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
  out_ << "            },\n";

  out_ << "          },\n";
}

void GypBinaryTargetWriter::WriteSources() {
  out_ << "      'sources': [\n";

  const Target::FileList& sources = target_->sources();
  for (size_t i = 0; i < sources.size(); i++) {
    const SourceFile& input_file = sources[i];
    out_ << "        '" << helper_.GetFileReference(input_file) << "',\n";
  }

  out_ << "      ],\n";
}

void GypBinaryTargetWriter::WriteDeps() {
  const std::vector<const Target*>& deps = target_->deps();
  if (deps.empty())
    return;

  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_JSON;

  out_ << "      'dependencies': [\n";
  for (size_t i = 0; i < deps.size(); i++) {
    out_ << "        '";
    EscapeStringToStream(out_, helper_.GetFullRefForTarget(deps[i]),
                         escape_options);
    out_ << "',\n";
  }
  out_ << "      ],\n";
}
