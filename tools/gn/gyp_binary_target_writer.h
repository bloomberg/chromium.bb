// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_GYP_BINARY_TARGET_WRITER_H_
#define TOOLS_GN_GYP_BINARY_TARGET_WRITER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "tools/gn/gyp_target_writer.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"

// Writes a portion of a .gyp file for a binary target type (an executable, a
// shared library, or a static library).
class GypBinaryTargetWriter : public GypTargetWriter {
 public:
  GypBinaryTargetWriter(const TargetGroup& group,
                        const Toolchain* debug_toolchain,
                        const SourceDir& gyp_dir,
                        std::ostream& out);
  virtual ~GypBinaryTargetWriter();

  virtual void Run() OVERRIDE;

 private:
  struct Flags {
    Flags();
    ~Flags();

    std::vector<std::string> defines;
    std::vector<SourceDir> include_dirs;

    std::vector<std::string> cflags;
    std::vector<std::string> cflags_c;
    std::vector<std::string> cflags_cc;
    std::vector<std::string> cflags_objc;
    std::vector<std::string> cflags_objcc;
    std::vector<std::string> ldflags;
    std::vector<SourceDir> lib_dirs;
    std::vector<std::string> libs;
  };

  void WriteName(int indent);
  void WriteType(int indent);

  // Writes the flags, sources, and deps.
  void WriteVCConfiguration(int indent);
  void WriteLinuxConfiguration(int indent);
  void WriteMacConfiguration(int indent);

  // Writes the flags, defines, etc. The flags input is non-const because the
  // cflags will be fixed up to account for things converted to VC settings
  // (rather than compiler flags).
  void WriteVCFlags(Flags& flags, int indent);
  void WriteMacFlags(const Target* target, Flags& flags, int indent);

  // Writes the Linux compiler and linker flags. The first version does the
  // flags for the given target, the second version takes a pregenerted list of
  // flags.
  void WriteLinuxFlagsForTarget(const Target* target, int indent);
  void WriteLinuxFlags(const Flags& flags, int indent);

  // Writes out the given target and optional host flags. This will insert a
  // target conditionn if there is a host build.
  void WriteMacTargetAndHostFlags(const BuilderRecord* target,
                                  const BuilderRecord* host,
                                  int indent);

  // Shared helpers for writing specific parts of GYP files.
  void WriteSources(const Target* target, int indent);
  void WriteDeps(const Target* target, int indent);
  void WriteIncludeDirs(const Flags& flags, int indent);
  void WriteDirectDependentSettings(int indent);
  void WriteAllDependentSettings(int indent);

  // Writes out the given flags and such from all configs in the given list.
  void WriteSettingsFromConfigList(const std::vector<const Config*>& configs,
                                   int indent);

  // Fills the given flags structure.
  Flags FlagsFromTarget(const Target* target) const;
  Flags FlagsFromConfigList(const LabelConfigVector& configs) const;

  // Writes the given array with the given name. The indent should be the
  // indenting for the name, the values will be indented 2 spaces from there.
  // Writes nothing if there is nothing in the array.
  void WriteNamedArray(const char* name,
                       const std::vector<std::string>& values,
                       int indent);

  // All associated targets.
  TargetGroup group_;

  DISALLOW_COPY_AND_ASSIGN(GypBinaryTargetWriter);
};

#endif  // TOOLS_GN_GYP_BINARY_TARGET_WRITER_H_

