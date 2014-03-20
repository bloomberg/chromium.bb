// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_GYP_ACTION_TARGET_WRITER_H_
#define TOOLS_GN_GYP_ACTION_TARGET_WRITER_H_

#include "base/compiler_specific.h"
#include "tools/gn/gyp_target_writer.h"
#include "tools/gn/target.h"
#include "tools/gn/toolchain.h"

class GypActionTargetWriter : public GypTargetWriter {
 public:
  GypActionTargetWriter(const TargetGroup& group,
                        const Toolchain* toolchain,
                        const SourceDir& gyp_dir,
                        std::ostream& out);
  virtual ~GypActionTargetWriter();

  virtual void Run() OVERRIDE;

 private:
  void WriteActionInputs(int indent);
  void WriteActionOutputs(int indent);

  DISALLOW_COPY_AND_ASSIGN(GypActionTargetWriter);
};

#endif  // TOOLS_GN_GYP_ACTION_TARGET_WRITER_H_
