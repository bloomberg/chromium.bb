// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_WRITER_H_
#define TOOLS_GN_NINJA_WRITER_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"

class Builder;
class BuildSettings;
class Settings;
class Target;

class NinjaWriter {
 public:
  // On failure will print an error and will return false.
  static bool RunAndWriteFiles(const BuildSettings* build_settings,
                               Builder* builder);

  // Writes only the toolchain.ninja files, skipping the root buildfile. The
  // settings for the files written will be added to the vector.
  static bool RunAndWriteToolchainFiles(
      const BuildSettings* build_settings,
      Builder* builder,
      std::vector<const Settings*>* all_settings);

 private:
  NinjaWriter(const BuildSettings* build_settings, Builder* builder);
  ~NinjaWriter();

  bool WriteToolchains(
      std::vector<const Settings*>* all_settings,
      std::vector<const Target*>* default_targets);
  bool WriteRootBuildfiles(const std::vector<const Settings*>& all_settings,
                           const std::vector<const Target*>& default_targets);

  const BuildSettings* build_settings_;
  Builder* builder_;

  DISALLOW_COPY_AND_ASSIGN(NinjaWriter);
};

#endif  // TOOLS_GN_NINJA_WRITER_H_
