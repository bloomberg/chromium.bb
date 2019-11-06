// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_IPC_CONSTANTS_H_
#define REMOTING_HOST_IPC_CONSTANTS_H_

#include "base/files/file_path.h"

namespace remoting {

// Name of the host process binary.
extern const base::FilePath::CharType kHostBinaryName[];

// Name of the desktop process binary.
extern const base::FilePath::CharType kDesktopBinaryName[];

// Returns the full path to an installed |binary| in |full_path|.
bool GetInstalledBinaryPath(const base::FilePath::StringType& binary,
                            base::FilePath* full_path);

}  // namespace remoting

#endif  // REMOTING_HOST_IPC_CONSTANTS_H_
