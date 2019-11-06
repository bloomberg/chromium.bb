// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_ANDROID_DUMPSTATE_WRITER_H_
#define CHROMECAST_BASE_ANDROID_DUMPSTATE_WRITER_H_

#include <jni.h>

#include <string>

#include "base/macros.h"

namespace chromecast {

// JNI wrapper for DumpstateWriter.java.
class DumpstateWriter {
 public:
  static void AddDumpValue(const std::string& name, const std::string& value);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(DumpstateWriter);
};

}  // namespace chromecast

#endif  // CHROMECAST_BASE_ANDROID_DUMPSTATE_WRITER_H_
