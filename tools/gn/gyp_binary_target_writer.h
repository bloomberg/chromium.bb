// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_GYP_BINARY_TARGET_WRITER_H_
#define TOOLS_GN_GYP_BINARY_TARGET_WRITER_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "tools/gn/gyp_target_writer.h"
#include "tools/gn/toolchain.h"

// Writes a portion of a .gyp file for a binary target type (an executable, a
// shared library, or a static library).
class GypBinaryTargetWriter : public GypTargetWriter {
 public:
  GypBinaryTargetWriter(const Target* debug_target,
                        const Target* release_target,
                        const Target* debug_host_target,
                        const Target* release_host_target,
                        std::ostream& out);
  virtual ~GypBinaryTargetWriter();

  virtual void Run() OVERRIDE;

 private:
  // Writes the given number of spaces to the output stream and returns it.
  std::ostream& Indent(int spaces);

  void WriteName(int indent);
  void WriteType(int indent);

  // The flags can vary between debug and release, so we must pass in the
  // correct debug or release target to the functions.
  void WriteDefines(const Target* target, int indent);
  void WriteIncludes(const Target* target, int indent);

  // For writing VisualStudio flags.
  void WriteVCConfiguration(int indent);
  void WriteVCFlags(const Target* target, int indent);

  void WriteLinuxConfiguration(int indent);
  void WriteLinuxFlags(const Target* target, int indent);

  void WriteSources(const Target* target, int indent);
  void WriteDeps(const Target* target, int indent);

  // The normal |target_| in the base class stores the debug version.
  const Target* release_target_;

  // When doing cross-compiles (Linux-only) these will be the host version of
  // the target, if any (may be null).
  const Target* debug_host_target_;
  const Target* release_host_target_;

  DISALLOW_COPY_AND_ASSIGN(GypBinaryTargetWriter);
};

#endif  // TOOLS_GN_GYP_BINARY_TARGET_WRITER_H_

