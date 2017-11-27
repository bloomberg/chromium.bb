// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <unistd.h>

#include <thread>

#include "base/android/library_loader/anchor_functions.h"
#include "base/files/file.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "tools/cygprofile/lightweight_cygprofile.h"

#if !defined(ARCH_CPU_ARMEL)
#error Only supported on ARM.
#endif  // !defined(ARCH_CPU_ARMEL)

namespace cygprofile {
namespace {

// Disables the recording of addresses after |kDelayInSeconds| and dumps the
// result to a file.
class DelayedDumper {
 public:
  DelayedDumper() {
    SanityChecks();
    std::thread([]() {
      sleep(kDelayInSeconds);
      auto path = base::StringPrintf(
          "/data/local/tmp/chrome/cyglog/"
          "cygprofile-instrumented-code-hitmap-%d.txt",
          getpid());
      StopAndDumpToFile(base::FilePath(path));
    })
        .detach();
  }

  static constexpr int kDelayInSeconds = 30;
};

// Static initializer on purpose. Will disable instrumentation after
// |kDelayInSeconds|.
DelayedDumper g_dump_later;

}  // namespace
}  // namespace cygprofile
