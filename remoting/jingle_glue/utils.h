// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_JINGLE_GLUE_UTILS_H_
#define REMOTING_JINGLE_GLUE_UTILS_H_

namespace remoting {

// Convert values from <errno.h> to values from "net/base/net_errors.h"
int MapPosixToChromeError(int err);

}  // namespace remoting

#endif  // REMOTING_JINGLE_GLUE_UTILS_H_
