// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/gyp_binary_target_writer.h"

#include <set>

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "tools/gn/builder_record.h"
#include "tools/gn/config_values_extractors.h"
#include "tools/gn/err.h"
#include "tools/gn/escape.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/settings.h"
#include "tools/gn/target.h"

namespace {

// This functor is used to capture the output of RecursiveTargetConfigToStream
// in an vector.
template<typename T>
struct Accumulator {
  Accumulator(std::vector<T>* result_in) : result(result_in) {}

  void operator()(const T& s, std::ostream&) const {
    result->push_back(s);
  }

  std::vector<T>* result;
};

// Writes the given array values. The array should already be declared with the
// opening "[" written to the output. The function will not write the
// terminating "]" either.
void WriteArrayValues(std::ostream& out,
                      const std::vector<std::string>& values) {
  EscapeOptions options;
  options.mode = ESCAPE_JSON;
  for (size_t i = 0; i < values.size(); i++) {
    out << " '";
    EscapeStringToStream(out, values[i], options);
    out << "',";
  }
}

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

// Returns the value from the already-filled in cflags for the processor
// architecture to set in the GYP file. Additionally, this removes the flag
// from the given vector so we don't get duplicates.
std::string GetMacArch(std::vector<std::string>* cflags) {
  // Searches for the "-arch" option and returns the corresponding GYP value.
  for (size_t i = 0; i < cflags->size(); i++) {
    const std::string& cur = (*cflags)[i];
    if (cur == "-arch") {
      // This is the first part of a list with ["-arch", "i386"], return the
      // following item, and delete both of them.
      if (i < cflags->size() - 1) {
        std::string ret = (*cflags)[i + 1];
        cflags->erase(cflags->begin() + i, cflags->begin() + i + 2);
        return ret;
      }
    } else if (StartsWithASCII(cur, "-arch ", true)) {
      // The arch was passed as one GN string value, e.g. "-arch i386". Return
      // the stuff following the space and delete the item.
      std::string ret = cur.substr(6);
      cflags->erase(cflags->begin() + i);
      return ret;
    }
  }
  return std::string();
}

// Searches for -miphoneos-version-min, returns the resulting value, and
// removes it from the list. Returns the empty string if it's not found.
std::string GetIPhoneVersionMin(std::vector<std::string>* cflags) {
  // Searches for the "-arch" option and returns the corresponding GYP value.
  const char prefix[] = "-miphoneos-version-min=";
  for (size_t i = 0; i < cflags->size(); i++) {
    const std::string& cur = (*cflags)[i];
    if (StartsWithASCII(cur, prefix, true)) {
      std::string result = cur.substr(arraysize(prefix) - 1);
      cflags->erase(cflags->begin() + i);
      return result;
    }
  }
  return std::string();
}

// Finds all values from the given getter from all configs in the given list,
// and adds them to the given result vector.
template<typename T>
void FillConfigListValues(
    const LabelConfigVector& configs,
    const std::vector<T>& (ConfigValues::* getter)() const,
    std::vector<T>* result) {
  for (size_t config_i = 0; config_i < configs.size(); config_i++) {
    const std::vector<T>& values =
        (configs[config_i].ptr->config_values().*getter)();
    for (size_t val_i = 0; val_i < values.size(); val_i++)
      result->push_back(values[val_i]);
  }
}

bool IsClang(const Target* target) {
  const Value* is_clang =
      target->settings()->base_config()->GetValue("is_clang");
  return is_clang && is_clang->type() == Value::BOOLEAN &&
      is_clang->boolean_value();
}

bool IsIOS(const Target* target) {
  const Value* is_ios = target->settings()->base_config()->GetValue("is_ios");
  return is_ios && is_ios->type() == Value::BOOLEAN && is_ios->boolean_value();
}

// Returns true if the current target is an iOS simulator build.
bool IsIOSSimulator(const std::vector<std::string>& cflags) {
  // Search for the sysroot flag. We expect one flag to be the
  // switch, and the following one to be the value.
  const char sysroot[] = "-isysroot";
  for (size_t i = 0; i < cflags.size(); i++) {
    const std::string& cur = cflags[i];
    if (cur == sysroot) {
      if (i == cflags.size() - 1)
        return false;  // No following value.

      // The argument is a file path, we check the prefix of the file name.
      base::FilePath path(UTF8ToFilePath(cflags[i + 1]));
      std::string path_file_part = FilePathToUTF8(path.BaseName());
      return StartsWithASCII(path_file_part, "iphonesimulator", false);
    }
  }
  return false;
}

}  // namespace

GypBinaryTargetWriter::Flags::Flags() {}
GypBinaryTargetWriter::Flags::~Flags() {}

GypBinaryTargetWriter::GypBinaryTargetWriter(const TargetGroup& group,
                                             const Toolchain* debug_toolchain,
                                             const SourceDir& gyp_dir,
                                             std::ostream& out)
    : GypTargetWriter(group.get()->item()->AsTarget(), debug_toolchain,
                      gyp_dir, out),
      group_(group) {
}

GypBinaryTargetWriter::~GypBinaryTargetWriter() {
}

void GypBinaryTargetWriter::Run() {
  int indent = 4;

  Indent(indent) << "{\n";

  WriteName(indent + kExtraIndent);
  WriteType(indent + kExtraIndent);

  if (target_->settings()->IsLinux())
    WriteLinuxConfiguration(indent + kExtraIndent);
  else if (target_->settings()->IsWin())
    WriteVCConfiguration(indent + kExtraIndent);
  else if (target_->settings()->IsMac())
    WriteMacConfiguration(indent + kExtraIndent);
  WriteDirectDependentSettings(indent + kExtraIndent);
  WriteAllDependentSettings(indent + kExtraIndent);

  Indent(indent) << "},\n";
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

  std::string product_extension = target_->output_extension();
  if (!product_extension.empty())
    Indent(indent) << "'product_extension': '" << product_extension << "',\n";
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

  // Write out the toolsets depending on whether there is a host build. If no
  // toolset is specified, GYP assumes a target build.
  if (group_.debug && group_.host_debug)
    Indent(indent) << "'toolsets': ['target', 'host'],\n";
  else if (group_.host_debug)
    Indent(indent) << "'toolsets': ['host'],\n";
}

void GypBinaryTargetWriter::WriteVCConfiguration(int indent) {
  Indent(indent) << "'configurations': {\n";

  Indent(indent + kExtraIndent) << "'Debug': {\n";
  Indent(indent + kExtraIndent * 2) <<
      "'msvs_configuration_platform': 'Win32',\n";
  Flags debug_flags(FlagsFromTarget(group_.debug->item()->AsTarget()));
  WriteVCFlags(debug_flags, indent + kExtraIndent * 2);
  Indent(indent + kExtraIndent) << "},\n";

  Indent(indent + kExtraIndent) << "'Release': {\n";
  Indent(indent + kExtraIndent * 2) <<
      "'msvs_configuration_platform': 'Win32',\n";
  Flags release_flags(FlagsFromTarget(group_.release->item()->AsTarget()));
  WriteVCFlags(release_flags, indent + kExtraIndent * 2);
  Indent(indent + kExtraIndent) << "},\n";

  // Note that we always need Debug_x64 and Release_x64 defined or GYP will get
  // confused, but we ca leave them empty if there's no 64-bit target.
  Indent(indent + kExtraIndent) << "'Debug_x64': {\n";
  if (group_.debug64) {
    Indent(indent + kExtraIndent * 2) <<
        "'msvs_configuration_platform': 'x64',\n";
    Flags flags(FlagsFromTarget(group_.debug64->item()->AsTarget()));
    WriteVCFlags(flags, indent + kExtraIndent * 2);
  }
  Indent(indent + kExtraIndent) << "},\n";

  Indent(indent + kExtraIndent) << "'Release_x64': {\n";
  if (group_.release64) {
    Indent(indent + kExtraIndent * 2) <<
        "'msvs_configuration_platform': 'x64',\n";
    Flags flags(FlagsFromTarget(group_.release64->item()->AsTarget()));
    WriteVCFlags(flags, indent + kExtraIndent * 2);
  }
  Indent(indent + kExtraIndent) << "},\n";

  Indent(indent) << "},\n";

  WriteSources(target_, indent);
  WriteDeps(target_, indent);
}

void GypBinaryTargetWriter::WriteLinuxConfiguration(int indent) {
  // The Linux stuff works differently. On Linux we support cross-compiles and
  // all ninja generators know to look for target conditions. Other platforms'
  // generators don't all do this, so we can't have the same GYP structure.
  Indent(indent) << "'target_conditions': [\n";
  // The host toolset is configured for the current computer, we will only have
  // this when doing cross-compiles.
  if (group_.host_debug && group_.host_release) {
    Indent(indent + kExtraIndent) << "['_toolset == \"host\"', {\n";
    Indent(indent + kExtraIndent * 2) << "'configurations': {\n";
    Indent(indent + kExtraIndent * 3) << "'Debug': {\n";
    WriteLinuxFlagsForTarget(group_.host_debug->item()->AsTarget(),
                             indent + kExtraIndent * 4);
    Indent(indent + kExtraIndent * 3) << "},\n";
    Indent(indent + kExtraIndent * 3) << "'Release': {\n";
    WriteLinuxFlagsForTarget(group_.host_release->item()->AsTarget(),
                             indent + kExtraIndent * 4);
    Indent(indent + kExtraIndent * 3) << "},\n";
    Indent(indent + kExtraIndent * 2) << "}\n";

    // The sources are per-toolset but shared between debug & release.
    WriteSources(group_.host_debug->item()->AsTarget(),
                 indent + kExtraIndent * 2);

    Indent(indent + kExtraIndent) << "],\n";
  }

  // The target toolset is the "regular" one.
  Indent(indent + kExtraIndent) << "['_toolset == \"target\"', {\n";
  Indent(indent + kExtraIndent * 2) << "'configurations': {\n";
  Indent(indent + kExtraIndent * 3) << "'Debug': {\n";
  WriteLinuxFlagsForTarget(group_.debug->item()->AsTarget(),
                           indent + kExtraIndent * 4);
  Indent(indent + kExtraIndent * 3) << "},\n";
  Indent(indent + kExtraIndent * 3) << "'Release': {\n";
  WriteLinuxFlagsForTarget(group_.release->item()->AsTarget(),
                           indent + kExtraIndent * 4);
  Indent(indent + kExtraIndent * 3) << "},\n";
  Indent(indent + kExtraIndent * 2) << "},\n";

  WriteSources(target_, indent + kExtraIndent * 2);

  Indent(indent + kExtraIndent) << "},],\n";
  Indent(indent) << "],\n";

  // Deps in GYP can not vary based on the toolset.
  WriteDeps(target_, indent);
}

void GypBinaryTargetWriter::WriteMacConfiguration(int indent) {
  // The Mac flags are parameterized by the GYP generator (Ninja vs. XCode).
  const char kNinjaGeneratorCondition[] =
      "'conditions': [['\"<(GENERATOR)\"==\"ninja\"', {\n";
  const char kNinjaGeneratorElse[] = "}, {\n";
  const char kNinjaGeneratorEnd[] = "}]],\n";

  Indent(indent) << "'configurations': {\n";

  // Debug.
  Indent(indent + kExtraIndent) << "'Debug': {\n";
  Indent(indent + kExtraIndent * 2) << kNinjaGeneratorCondition;
  {
    // Ninja generator.
    WriteMacTargetAndHostFlags(group_.debug, group_.host_debug,
                               indent + kExtraIndent * 3);
  }
  Indent(indent + kExtraIndent * 2) << kNinjaGeneratorElse;
  if (group_.xcode_debug) {
    // XCode generator.
    WriteMacTargetAndHostFlags(group_.xcode_debug, group_.xcode_host_debug,
                               indent + kExtraIndent * 3);
  }
  Indent(indent + kExtraIndent * 2) << kNinjaGeneratorEnd;
  Indent(indent + kExtraIndent) << "},\n";

  // Release.
  Indent(indent + kExtraIndent) << "'Release': {\n";
  Indent(indent + kExtraIndent * 2) << kNinjaGeneratorCondition;
  {
    // Ninja generator.
    WriteMacTargetAndHostFlags(group_.release, group_.host_release,
                               indent + kExtraIndent * 3);
  }
  Indent(indent + kExtraIndent * 2) << kNinjaGeneratorElse;
  if (group_.xcode_release) {
    // XCode generator.
    WriteMacTargetAndHostFlags(group_.xcode_release, group_.xcode_host_release,
                               indent + kExtraIndent * 3);
  }
  Indent(indent + kExtraIndent * 2) << kNinjaGeneratorEnd;
  Indent(indent + kExtraIndent) << "},\n";

  Indent(indent) << "},\n";

  WriteSources(target_, indent);
  WriteDeps(target_, indent);
}

void GypBinaryTargetWriter::WriteVCFlags(Flags& flags, int indent) {
  // Defines and includes go outside of the msvs settings.
  WriteNamedArray("defines", flags.defines, indent);
  WriteIncludeDirs(flags, indent);

  // C flags.
  Indent(indent) << "'msvs_settings': {\n";
  Indent(indent + kExtraIndent) << "'VCCLCompilerTool': {\n";

  // GYP always uses the VC optimization flag to add a /O? on Visual Studio.
  // This can produce duplicate values. So look up the GYP value corresponding
  // to the flags used, and set the same one.
  std::string optimization = GetVCOptimization(&flags.cflags);
  WriteNamedArray("AdditionalOptions", flags.cflags, indent + kExtraIndent * 2);
  // TODO(brettw) cflags_c and cflags_cc!
  Indent(indent + kExtraIndent * 2) << "'Optimization': "
                                    << optimization << ",\n";
  Indent(indent + kExtraIndent) << "},\n";

  // Linker tool stuff.
  Indent(indent + kExtraIndent) << "'VCLinkerTool': {\n";

  // ...Library dirs.
  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_JSON;
  if (!flags.lib_dirs.empty()) {
    Indent(indent + kExtraIndent * 2) << "'AdditionalLibraryDirectories': [";
    for (size_t i = 0; i < flags.lib_dirs.size(); i++) {
      out_ << " '";
      EscapeStringToStream(out_,
                           helper_.GetDirReference(flags.lib_dirs[i], false),
                           escape_options);
      out_ << "',";
    }
    out_ << " ],\n";
  }

  // ...Libraries.
  WriteNamedArray("AdditionalDependencies", flags.libs,
                  indent + kExtraIndent * 2);

  // ...LD flags.
  // TODO(brettw) EnableUAC defaults to on and needs to be set. Also
  // UACExecutionLevel and UACUIAccess depends on that and defaults to 0/false.
  WriteNamedArray("AdditionalOptions", flags.ldflags, 14);
  Indent(indent + kExtraIndent) << "},\n";
  Indent(indent) << "},\n";
}

void GypBinaryTargetWriter::WriteMacFlags(const Target* target,
                                          Flags& flags,
                                          int indent) {
  WriteNamedArray("defines", flags.defines, indent);
  WriteIncludeDirs(flags, indent);

  // Libraries and library directories.
  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_JSON;
  if (!flags.lib_dirs.empty()) {
    Indent(indent + kExtraIndent) << "'library_dirs': [";
    for (size_t i = 0; i < flags.lib_dirs.size(); i++) {
      out_ << " '";
      EscapeStringToStream(out_,
                           helper_.GetDirReference(flags.lib_dirs[i], false),
                           escape_options);
      out_ << "',";
    }
    out_ << " ],\n";
  }
  if (!flags.libs.empty()) {
    Indent(indent) << "'link_settings': {\n";
    Indent(indent + kExtraIndent) << "'libraries': [";
    for (size_t i = 0; i < flags.libs.size(); i++) {
      out_ << " '-l";
      EscapeStringToStream(out_, flags.libs[i], escape_options);
      out_ << "',";
    }
    out_ << " ],\n";
    Indent(indent) << "},\n";
  }

  Indent(indent) << "'xcode_settings': {\n";

  // Architecture. GYP reads this value and uses it to generate the -arch
  // flag, so we always want to remove it from the cflags (which is a side
  // effect of what GetMacArch does), even in cases where we don't use the
  // value (in the iOS arm below).
  std::string arch = GetMacArch(&flags.cflags);
  if (IsIOS(target)) {
    // When writing an iOS "target" (not host) target, we set VALID_ARCHS
    // instead of ARCHS and always use this hardcoded value. This matches the
    // GYP build.
    Indent(indent + kExtraIndent) << "'VALID_ARCHS': ['armv7', 'i386'],\n";

    // Tell XCode to target both iPhone and iPad. GN has no such concept.
    Indent(indent + kExtraIndent) << "'TARGETED_DEVICE_FAMILY': '1,2',\n";

    if (IsIOSSimulator(flags.cflags)) {
      Indent(indent + kExtraIndent) << "'SDKROOT': 'iphonesimulator',\n";
    } else {
      Indent(indent + kExtraIndent) << "'SDKROOT': 'iphoneos',\n";
      std::string min_ver = GetIPhoneVersionMin(&flags.cflags);
      if (!min_ver.empty()) {
        Indent(indent + kExtraIndent) << "'IPHONEOS_DEPLOYMENT_TARGET': '"
                                      << min_ver << "',\n";
      }
    }
  } else {
    // When doing regular Mac and "host" iOS (which look like regular Mac)
    // builds, we can set the ARCHS value to what's specified in the build.
    if (arch == "i386")
      Indent(indent + kExtraIndent) << "'ARCHS': [ 'i386' ],\n";
    else if (arch == "x86_64")
      Indent(indent + kExtraIndent) << "'ARCHS': [ 'x86_64' ],\n";
  }

  // C/C++ flags.
  if (!flags.cflags.empty() || !flags.cflags_c.empty() ||
      !flags.cflags_objc.empty()) {
    Indent(indent + kExtraIndent) << "'OTHER_CFLAGS': [";
    WriteArrayValues(out_, flags.cflags);
    WriteArrayValues(out_, flags.cflags_c);
    WriteArrayValues(out_, flags.cflags_objc);
    out_ << " ],\n";
  }
  if (!flags.cflags.empty() || !flags.cflags_cc.empty() ||
      !flags.cflags_objcc.empty()) {
    Indent(indent + kExtraIndent) << "'OTHER_CPLUSPLUSFLAGS': [";
    WriteArrayValues(out_, flags.cflags);
    WriteArrayValues(out_, flags.cflags_cc);
    WriteArrayValues(out_, flags.cflags_objcc);
    out_ << " ],\n";
  }

  // Ld flags. Don't write these for static libraries. Otherwise, they'll be
  // passed to the library tool which doesn't expect it (the toolchain does
  // not use ldflags so these are ignored in the normal build).
  if (target->output_type() != Target::STATIC_LIBRARY)
    WriteNamedArray("OTHER_LDFLAGS", flags.ldflags, indent + kExtraIndent);

  // Write the compiler that XCode should use. When we're using clang, we want
  // the custom one, otherwise don't add this and the default compiler will be
  // used.
  //
  // TODO(brettw) this is a hack. We could add a way for the GN build to set
  // these values but as far as I can see this is the only use for them, so
  // currently we manually check the build config's is_clang value.
  if (IsClang(target)) {
    base::FilePath clang_path =
        target_->settings()->build_settings()->GetFullPath(SourceFile(
            "//third_party/llvm-build/Release+Asserts/bin/clang"));
    base::FilePath clang_pp_path =
        target_->settings()->build_settings()->GetFullPath(SourceFile(
            "//third_party/llvm-build/Release+Asserts/bin/clang++"));

    Indent(indent + kExtraIndent)
        << "'CC': '" << FilePathToUTF8(clang_path) << "',\n";
    Indent(indent + kExtraIndent)
        << "'LDPLUSPLUS': '" << FilePathToUTF8(clang_pp_path) << "',\n";
  }

  Indent(indent) << "},\n";
}

void GypBinaryTargetWriter::WriteLinuxFlagsForTarget(const Target* target,
                                                     int indent) {
  Flags flags(FlagsFromTarget(target));
  WriteLinuxFlags(flags, indent);
}

void GypBinaryTargetWriter::WriteLinuxFlags(const Flags& flags, int indent) {
  WriteIncludeDirs(flags, indent);
  WriteNamedArray("defines",      flags.defines,      indent);
  WriteNamedArray("cflags",       flags.cflags,       indent);
  WriteNamedArray("cflags_c",     flags.cflags_c,     indent);
  WriteNamedArray("cflags_cc",    flags.cflags_cc,    indent);
  WriteNamedArray("cflags_objc",  flags.cflags_objc,  indent);
  WriteNamedArray("cflags_objcc", flags.cflags_objcc, indent);

  // Put libraries and library directories in with ldflags.
  Indent(indent) << "'ldflags': ["; \
  WriteArrayValues(out_, flags.ldflags);

  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_JSON;
  for (size_t i = 0; i < flags.lib_dirs.size(); i++) {
    out_ << " '-L";
    EscapeStringToStream(out_,
                         helper_.GetDirReference(flags.lib_dirs[i], false),
                         escape_options);
    out_ << "',";
  }

  for (size_t i = 0; i < flags.libs.size(); i++) {
    out_ << " '-l";
    EscapeStringToStream(out_, flags.libs[i], escape_options);
    out_ << "',";
  }
  out_ << " ],\n";
}

void GypBinaryTargetWriter::WriteMacTargetAndHostFlags(
    const BuilderRecord* target,
    const BuilderRecord* host,
    int indent) {
  // The Mac flags are sometimes (when cross-compiling) also parameterized on
  // the toolset.
  const char kToolsetTargetCondition[] =
      "'target_conditions': [['_toolset==\"target\"', {\n";
  const char kToolsetTargetElse[] = "}, {\n";
  const char kToolsetTargetEnd[] = "}]],\n";

  int extra_indent = 0;
  if (host) {
    // Write out the first part of the conditional.
    Indent(indent) << kToolsetTargetCondition;
    extra_indent = kExtraIndent;
  }

  // Always write the target flags (may or may not be inside a target
  // conditional).
  {
    Flags flags(FlagsFromTarget(target->item()->AsTarget()));
    WriteMacFlags(target->item()->AsTarget(), flags, indent + extra_indent);
  }

  // Now optionally write the host conditional arm.
  if (host) {
    Indent(indent) << kToolsetTargetElse;
    Flags flags(FlagsFromTarget(host->item()->AsTarget()));
    WriteMacFlags(host->item()->AsTarget(), flags, indent + kExtraIndent);
    Indent(indent) << kToolsetTargetEnd;
  }
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
  const LabelTargetVector& deps = target->deps();
  if (deps.empty())
    return;

  EscapeOptions escape_options;
  escape_options.mode = ESCAPE_JSON;

  Indent(indent) << "'dependencies': [\n";
  for (size_t i = 0; i < deps.size(); i++) {
    Indent(indent + kExtraIndent) << "'";
    EscapeStringToStream(out_, helper_.GetFullRefForTarget(deps[i].ptr),
                         escape_options);
    out_ << "',\n";
  }
  Indent(indent) << "],\n";
}

void GypBinaryTargetWriter::WriteIncludeDirs(const Flags& flags, int indent) {
  if (flags.include_dirs.empty())
    return;

  EscapeOptions options;
  options.mode = ESCAPE_JSON;

  Indent(indent) << "'include_dirs': [";
  for (size_t i = 0; i < flags.include_dirs.size(); i++) {
    out_ << " '";
    EscapeStringToStream(out_,
                         helper_.GetDirReference(flags.include_dirs[i], false),
                         options);
    out_ << "',";
  }
  out_ << " ],\n";
}

void GypBinaryTargetWriter::WriteDirectDependentSettings(int indent) {
  if (target_->direct_dependent_configs().empty())
    return;
  Indent(indent) << "'direct_dependent_settings': {\n";

  Flags flags(FlagsFromConfigList(target_->direct_dependent_configs()));
  if (target_->settings()->IsLinux())
    WriteLinuxFlags(flags, indent + kExtraIndent);
  else if (target_->settings()->IsWin())
    WriteVCFlags(flags, indent + kExtraIndent);
  else if (target_->settings()->IsMac())
    WriteMacFlags(target_, flags, indent + kExtraIndent);
  Indent(indent) << "},\n";
}

void GypBinaryTargetWriter::WriteAllDependentSettings(int indent) {
  if (target_->all_dependent_configs().empty())
    return;
  Indent(indent) << "'all_dependent_settings': {\n";

  Flags flags(FlagsFromConfigList(target_->all_dependent_configs()));
  if (target_->settings()->IsLinux())
    WriteLinuxFlags(flags, indent + kExtraIndent);
  else if (target_->settings()->IsWin())
    WriteVCFlags(flags, indent + kExtraIndent);
  else if (target_->settings()->IsMac())
    WriteMacFlags(target_, flags, indent + kExtraIndent);
  Indent(indent) << "},\n";
}

GypBinaryTargetWriter::Flags GypBinaryTargetWriter::FlagsFromTarget(
    const Target* target) const {
  Flags ret;

  // Extracts a vector of the given type and name from the config values.
  #define EXTRACT(type, name) \
      { \
        Accumulator<type> acc(&ret.name); \
        RecursiveTargetConfigToStream<type>(target, &ConfigValues::name, \
                                            acc, out_); \
      }

  EXTRACT(std::string, defines);
  EXTRACT(SourceDir,   include_dirs);
  EXTRACT(std::string, cflags);
  EXTRACT(std::string, cflags_c);
  EXTRACT(std::string, cflags_cc);
  EXTRACT(std::string, cflags_objc);
  EXTRACT(std::string, cflags_objcc);
  EXTRACT(std::string, ldflags);

  #undef EXTRACT

  const OrderedSet<SourceDir>& all_lib_dirs = target->all_lib_dirs();
  for (size_t i = 0; i < all_lib_dirs.size(); i++)
    ret.lib_dirs.push_back(all_lib_dirs[i]);

  const OrderedSet<std::string> all_libs = target->all_libs();
  for (size_t i = 0; i < all_libs.size(); i++)
    ret.libs.push_back(all_libs[i]);

  return ret;
}

GypBinaryTargetWriter::Flags GypBinaryTargetWriter::FlagsFromConfigList(
    const LabelConfigVector& configs) const {
  Flags ret;

  #define EXTRACT(type, name) \
      FillConfigListValues<type>(configs, &ConfigValues::name, &ret.name);

  EXTRACT(std::string, defines);
  EXTRACT(SourceDir,   include_dirs);
  EXTRACT(std::string, cflags);
  EXTRACT(std::string, cflags_c);
  EXTRACT(std::string, cflags_cc);
  EXTRACT(std::string, cflags_objc);
  EXTRACT(std::string, cflags_objcc);
  EXTRACT(std::string, ldflags);
  EXTRACT(SourceDir,   lib_dirs);
  EXTRACT(std::string, libs);

  #undef EXTRACT

  return ret;
}

void GypBinaryTargetWriter::WriteNamedArray(
    const char* name,
    const std::vector<std::string>& values,
    int indent) {
  if (values.empty())
    return;

  EscapeOptions options;
  options.mode = ESCAPE_JSON;

  Indent(indent) << "'" << name << "': [";
  WriteArrayValues(out_, values);
  out_ << " ],\n";
}

