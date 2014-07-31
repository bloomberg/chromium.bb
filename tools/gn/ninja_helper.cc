// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/ninja_helper.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/string_utils.h"
#include "tools/gn/target.h"

namespace {

const char kObjectDirNoSlash[] = "obj";

}  // namespace

NinjaHelper::NinjaHelper(const BuildSettings* build_settings)
    : build_settings_(build_settings) {
  build_to_src_no_last_slash_ = build_settings->build_to_source_dir_string();
  if (!build_to_src_no_last_slash_.empty() &&
      build_to_src_no_last_slash_[build_to_src_no_last_slash_.size() - 1] ==
          '/')
    build_to_src_no_last_slash_.resize(build_to_src_no_last_slash_.size() - 1);

  build_to_src_system_no_last_slash_ = build_to_src_no_last_slash_;
}

NinjaHelper::~NinjaHelper() {
}

std::string NinjaHelper::GetTopleveOutputDir() const {
  return kObjectDirNoSlash;
}

OutputFile NinjaHelper::GetTargetOutputDir(const Target* target) const {
  OutputFile ret(target->settings()->toolchain_output_subdir());
  ret.value().append(kObjectDirNoSlash);
  AppendStringPiece(&ret.value(),
                    target->label().dir().SourceAbsoluteWithOneSlash());
  return ret;
}

OutputFile NinjaHelper::GetNinjaFileForTarget(const Target* target) const {
  OutputFile ret = GetTargetOutputDir(target);
  ret.value().append(target->label().name());
  ret.value().append(".ninja");
  return ret;
}

OutputFile NinjaHelper::GetNinjaFileForToolchain(
    const Settings* settings) const {
  OutputFile ret;
  ret.value().append(settings->toolchain_output_subdir().value());
  ret.value().append("toolchain.ninja");
  return ret;
}

// In Python, GypPathToUniqueOutput does the qualification. The only case where
// the Python version doesn't qualify the name is for target outputs, which we
// handle in a separate function.
OutputFile NinjaHelper::GetOutputFileForSource(
    const Target* target,
    const SourceFile& source,
    SourceFileType type) const {
  // Extract the filename and remove the extension (keep the dot).
  base::StringPiece filename = FindFilename(&source.value());
  std::string name(filename.data(), filename.size());
  size_t extension_offset = FindExtensionOffset(name);
  CHECK(extension_offset != std::string::npos);
  name.resize(extension_offset);

  // Append the new extension.
  switch (type) {
    case SOURCE_ASM:
    case SOURCE_C:
    case SOURCE_CC:
    case SOURCE_M:
    case SOURCE_MM:
    case SOURCE_S:
      name.append(target->settings()->IsWin() ? "obj" : "o");
      break;

    case SOURCE_RC:
      name.append("res");
      break;

    // Pass .o/.obj files through unchanged.
    case SOURCE_O: {
      // System-absolute file names get preserved (they don't need to be
      // rebased relative to the build dir).
      if (source.is_system_absolute())
        return OutputFile(source.value());

      // Files that are already inside the build dir should not be made
      // relative to the source tree. Doing so will insert an unnecessary
      // "../.." into the path which won't match the corresponding target
      // name in ninja.
      CHECK(build_settings_->build_dir().is_source_absolute());
      CHECK(source.is_source_absolute());
      if (StartsWithASCII(source.value(),
                          build_settings_->build_dir().value(),
                          true)) {
        return OutputFile(
            source.value().substr(
                build_settings_->build_dir().value().size()));
      }

      // Construct the relative location of the file from the build dir.
      OutputFile ret(build_to_src_no_last_slash());
      source.SourceAbsoluteWithOneSlash().AppendToString(&ret.value());
      return ret;
    }

    case SOURCE_H:
    case SOURCE_UNKNOWN:
      NOTREACHED();
      return OutputFile();
  }

  // Use the scheme <path>/<target>.<name>.<extension> so that all output
  // names are unique to different targets.

  // This will look like "obj" or "toolchain_name/obj".
  OutputFile ret(target->settings()->toolchain_output_subdir());
  ret.value().append(kObjectDirNoSlash);

  // Find the directory, assume it starts with two slashes, and trim to one.
  base::StringPiece dir = FindDir(&source.value());
  CHECK(dir.size() >= 2 && dir[0] == '/' && dir[1] == '/')
      << "Source file isn't in the source repo: " << dir;
  AppendStringPiece(&ret.value(), dir.substr(1));

  ret.value().append(target->label().name());
  ret.value().append(".");
  ret.value().append(name);
  return ret;
}

OutputFile NinjaHelper::GetTargetOutputFile(const Target* target) const {
  OutputFile ret;

  // Use the output name if given, fall back to target name if not.
  const std::string& name = target->output_name().empty() ?
      target->label().name() : target->output_name();

  // This is prepended to the output file name. Some platforms get "lib"
  // prepended to library names. but be careful not to make a duplicate (e.g.
  // some targets like "libxml" already have the "lib" in the name).
  const char* prefix;
  if (!target->settings()->IsWin() &&
      (target->output_type() == Target::SHARED_LIBRARY ||
       target->output_type() == Target::STATIC_LIBRARY) &&
      name.compare(0, 3, "lib") != 0)
    prefix = "lib";
  else
    prefix = "";

  const char* extension;
  if (target->output_extension().empty()) {
    if (target->output_type() == Target::GROUP ||
        target->output_type() == Target::SOURCE_SET ||
        target->output_type() == Target::COPY_FILES ||
        target->output_type() == Target::ACTION ||
        target->output_type() == Target::ACTION_FOREACH) {
      extension = "stamp";
    } else {
      extension = GetExtensionForOutputType(target->output_type(),
                                            target->settings()->target_os());
    }
  } else {
    extension = target->output_extension().c_str();
  }

  // Everything goes into the toolchain directory (which will be empty for the
  // default toolchain, and will end in a slash otherwise).
  ret.value().append(target->settings()->toolchain_output_subdir().value());

  // Binaries and shared libraries go into the toolchain root.
  if (target->output_type() == Target::EXECUTABLE ||
      target->output_type() == Target::SHARED_LIBRARY) {
    // Generate a name like "<toolchain>/<prefix><name>.<extension>".
    ret.value().append(prefix);
    ret.value().append(name);
    if (extension[0]) {
      ret.value().push_back('.');
      ret.value().append(extension);
    }
    return ret;
  }

  // Everything else goes next to the target's .ninja file like
  // "<toolchain>/obj/<path>/<name>.<extension>".
  ret.value().append(kObjectDirNoSlash);
  AppendStringPiece(&ret.value(),
                    target->label().dir().SourceAbsoluteWithOneSlash());
  ret.value().append(prefix);
  ret.value().append(name);
  if (extension[0]) {
    ret.value().push_back('.');
    ret.value().append(extension);
  }
  return ret;
}

std::string NinjaHelper::GetRulePrefix(const Settings* settings) const {
  // Don't prefix the default toolchain so it looks prettier, prefix everything
  // else.
  if (settings->is_default())
    return std::string();  // Default toolchain has no prefix.
  return settings->toolchain_label().name() + "_";
}

std::string NinjaHelper::GetRuleForSourceType(const Settings* settings,
                                              SourceFileType type) const {
  // This function may be hot since it will be called for every source file
  // in the tree. We could cache the results to avoid making a string for
  // every invocation.
  std::string prefix = GetRulePrefix(settings);

  if (type == SOURCE_C)
    return prefix + "cc";
  if (type == SOURCE_CC)
    return prefix + "cxx";
  if (type == SOURCE_M)
    return prefix + "objc";
  if (type == SOURCE_MM)
    return prefix + "objcxx";
  if (type == SOURCE_RC)
    return prefix + "rc";
  if (type == SOURCE_S)
    return prefix + "cc";  // Assembly files just get compiled by CC.

  // TODO(brettw) asm files.

  // .obj files have no rules to make them (they're already built) so we return
  // the enpty string for SOURCE_O.
  return std::string();
}
