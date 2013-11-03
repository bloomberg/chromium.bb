// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/gn/gyp_helper.h"

#include "tools/gn/err.h"
#include "tools/gn/filesystem_utils.h"
#include "tools/gn/settings.h"
#include "tools/gn/source_file.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"

GypHelper::GypHelper() {
}

GypHelper::~GypHelper() {
}

SourceFile GypHelper::GetGypFileForTarget(const Target* target,
                                          Err* err) const {
  // Use the manually specified one if its there.
  if (!target->gyp_file().is_null())
    return target->gyp_file();

  // Otherwise inherit the directory name.
  const std::string& dir = target->label().dir().value();
  size_t last_slash = dir.size() - 1;
  std::string dir_name;
  if (last_slash > 0) {
    size_t next_to_last_slash = dir.rfind('/', last_slash - 1);
    if (next_to_last_slash != std::string::npos) {
      dir_name = dir.substr(next_to_last_slash + 1,
                            last_slash - next_to_last_slash - 1);
    }
  }
  if (dir_name.empty()) {
    *err = Err(Location(), "Need to specify a gyp_file value for " +
        target->label().GetUserVisibleName(false) + "\n"
        "since I can't make up one with no name.");
    return SourceFile();
  }

  return SourceFile(dir + dir_name + ".gyp");
}

std::string GypHelper::GetNameForTarget(const Target* target) const {
  if (target->settings()->is_default())
    return target->label().name();  // Default toolchain has no suffix.
  return target->label().name() + "_" +
         target->settings()->toolchain_label().name();
}

std::string GypHelper::GetFullRefForTarget(const Target* target) const {
  Err err;
  SourceFile gypfile = GetGypFileForTarget(target, &err);
  CHECK(gypfile.is_source_absolute()) <<
      "GYP files should not be system-absolute: " << gypfile.value();

  std::string ret("<(DEPTH)/");
  // Trim off the leading double-slash.
  ret.append(&gypfile.value()[2], gypfile.value().size() - 2);
  ret.push_back(':');
  ret.append(GetNameForTarget(target));

  return ret;
}

std::string GypHelper::GetFileReference(const SourceFile& file) const {
  std::string ret;
  if (file.is_null())
    return ret;

  // Use FilePath's resolver to get the system paths out (on Windows this
  // requires special handling). Since it's absolute, it doesn't matter what
  // we pass in as the source root.
  if (file.is_system_absolute())
    return FilePathToUTF8(file.Resolve(base::FilePath()));

  // Source root relative, strip the "//".
  ret.assign("<(DEPTH)/");
  ret.append(&file.value()[2], file.value().size() - 2);
  return ret;
}

std::string GypHelper::GetDirReference(const SourceDir& dir,
                                       bool include_slash) const {
  std::string ret;
  if (dir.is_null())
    return ret;

  if (dir.is_system_absolute()) {
    ret = FilePathToUTF8(dir.Resolve(base::FilePath()));
  } else {
    ret.assign("<(DEPTH)/");
    ret.append(&dir.value()[2], dir.value().size() - 2);
  }

  // Optionally trim the slash off the end.
  if (!ret.empty() && !include_slash &&
      (ret[ret.size() - 1] == '/' || ret[ret.size() - 1] == '\\'))
    ret.resize(ret.size() - 1);

  return ret;
}
