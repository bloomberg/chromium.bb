// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_BASE_UTIL_H_
#define REMOTING_BASE_UTIL_H_

#include "media/base/video_frame.h"

namespace remoting {

// TODO(sergeyu): Move this to media::VideoFrame.
int GetBytesPerPixel(media::VideoFrame::Format format);

}  // namespace remoting

#endif  // REMOTING_BASE_UTIL_H_
