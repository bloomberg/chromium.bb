// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DAEMON_CONTROLLER_COMMON_WIN_H_
#define REMOTING_HOST_DAEMON_CONTROLLER_COMMON_WIN_H_

#include "base/file_path.h"

// Code common to the Windows daemon controller and the Windows elevated
// controller.

namespace remoting {

// The unprivileged configuration file name. The directory for the file is
// returned by remoting::GetConfigDir().
extern const FilePath::CharType* kUnprivilegedConfigFileName;

// The maximum size of the configuration file. "1MB ought to be enough" for any
// reasonable configuration we will ever need. 1MB is low enough to make
// the probability of out of memory situation fairly low. OOM is still possible
// and we will crash if it occurs.
extern const size_t kMaxConfigFileSize;

}  // namespace remoting

#endif  // REMOTING_HOST_DAEMON_CONTROLLER_COMMON_WIN_H_
