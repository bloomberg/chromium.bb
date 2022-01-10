// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CONSTANTS_H_
#define REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CONSTANTS_H_

#include "build/build_config.h"

namespace remoting {

extern const char kRemoteOpenUrlDataChannelName[];

#if defined(OS_WIN)

// The ProgID of the URL forwarder.
extern const wchar_t kUrlForwarderProgId[];

// The ProgID for undecided default browser, which launches OpenWith.exe.
extern const wchar_t kUndecidedProgId[];

#endif  // defined (OS_WIN)

}  // namespace remoting

#endif  // REMOTING_HOST_REMOTE_OPEN_URL_REMOTE_OPEN_URL_CONSTANTS_H_
