// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/logging.h"

#include <guiddef.h>

#include "base/file_path.h"
#include "base/logging.h"
#include "base/logging_win.h"
#include "remoting/host/branding.h"

// {2db51ca1-4fd8-4b88-b5a2-fb8606b66b02}
const GUID kRemotingHostLogProvider =
    { 0x2db51ca1, 0x4fd8, 0x4b88,
        { 0xb5, 0xa2, 0xfb, 0x86, 0x06, 0xb6, 0x6b, 0x02 } };

namespace remoting {

void InitHostLogging() {
#if defined(NDEBUG)
  // Write logs to the system debug log in release build.
  logging::InitLogging(
      NULL,
      logging::LOG_ONLY_TO_SYSTEM_DEBUG_LOG,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
#else  // !defined(NDEBUG)
  // Write logs to a file in debug build.
  FilePath debug_log = remoting::GetConfigDir().
      Append(FILE_PATH_LITERAL("debug.log"));
  logging::InitLogging(
      debug_log.value().c_str(),
      logging::LOG_ONLY_TO_FILE,
      logging::LOCK_LOG_FILE,
      logging::DELETE_OLD_LOG_FILE,
      logging::DISABLE_DCHECK_FOR_NON_OFFICIAL_RELEASE_BUILDS);
#endif  // !defined(NDEBUG)

  // Enable trace control and transport through event tracing for Windows.
  logging::LogEventProvider::Initialize(kRemotingHostLogProvider);
}

}  // namespace remoting
