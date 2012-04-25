// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/daemon_controller_common_win.h"

// Code common to the Windows daemon controller and the Windows elevated
// controller.

namespace remoting {

const FilePath::CharType* kUnprivilegedConfigFileName =
    FILE_PATH_LITERAL("host_unprivileged.json");

const size_t kMaxConfigFileSize = 1024 * 1024;

}  // namespace remoting
