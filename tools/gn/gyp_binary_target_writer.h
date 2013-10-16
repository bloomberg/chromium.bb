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
                        std::ostream& out);
  virtual ~GypBinaryTargetWriter();

  virtual void Run() OVERRIDE;

 private:
  void WriteName();
  void WriteType();

  // The flags can vary between debug and release, so we must pass in the
  // correct debug or release target to the functions.
  void WriteFlags(const Target* target);
  void WriteDefines(const Target* target);
  void WriteIncludes(const Target* target);
  void WriteVCFlags(const Target* target);

  void WriteSources();
  void WriteDeps();

  // The normal |target_| in the base class stores the debug version.
  const Target* release_target_;

  DISALLOW_COPY_AND_ASSIGN(GypBinaryTargetWriter);
};

#endif  // TOOLS_GN_GYP_BINARY_TARGET_WRITER_H_

