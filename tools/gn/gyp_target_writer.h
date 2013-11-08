// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_GYP_TARGET_WRITER_H_
#define TOOLS_GN_GYP_TARGET_WRITER_H_

#include <iosfwd>
#include <vector>

#include "base/basictypes.h"
#include "tools/gn/gyp_helper.h"

class BuilderRecord;
class Err;
class Settings;
class SourceFile;
class Target;

class GypTargetWriter {
 public:
  struct TargetGroup {
    TargetGroup()
        : debug(NULL),
          release(NULL),
          host_debug(NULL),
          host_release(NULL) {
    }
    const BuilderRecord* debug;
    const BuilderRecord* release;
    const BuilderRecord* host_debug;
    const BuilderRecord* host_release;
  };

  GypTargetWriter(const Target* target, std::ostream& out);
  virtual ~GypTargetWriter();

  static void WriteFile(const SourceFile& gyp_file,
                        const std::vector<TargetGroup>& targets,
                        Err* err);

  virtual void Run() = 0;

 protected:
  const Settings* settings_;  // Non-owning.
  const Target* target_;  // Non-owning.
  std::ostream& out_;

  GypHelper helper_;

 private:
  DISALLOW_COPY_AND_ASSIGN(GypTargetWriter);
};

#endif  // TOOLS_GN_GYP_TARGET_WRITER_H_
