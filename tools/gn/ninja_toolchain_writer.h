// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_NINJA_TOOLCHAIN_WRITER_H_
#define TOOLS_GN_NINJA_TOOLCHAIN_WRITER_H_

#include <iosfwd>
#include <set>
#include <string>
#include <vector>

#include "tools/gn/ninja_helper.h"
#include "tools/gn/path_output.h"

class BuildSettings;
class Settings;
class Target;

class NinjaToolchainWriter {
 public:
  // Takes the settings for the toolchain, as well as the list of all targets
  // assicoated with the toolchain. Ninja files exactly matching "skip_files"
  // will not be included in the subninja list.
  static bool RunAndWriteFile(const Settings* settings,
                              const std::vector<const Target*>& targets,
                              const std::set<std::string>& skip_files);

 private:
  NinjaToolchainWriter(const Settings* settings,
                       const std::vector<const Target*>& targets,
                       const std::set<std::string>& skip_files,
                       std::ostream& out);
  ~NinjaToolchainWriter();

  void Run();

  void WriteRules();
  void WriteSubninjas();

  const Settings* settings_;
  std::vector<const Target*> targets_;
  const std::set<std::string>& skip_files_;
  std::ostream& out_;
  PathOutput path_output_;

  NinjaHelper helper_;

  DISALLOW_COPY_AND_ASSIGN(NinjaToolchainWriter);
};

#endif  // TOOLS_GN_NINJA_TOOLCHAIN_WRITER_H_
