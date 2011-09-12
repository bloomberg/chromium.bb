// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_view.h"

#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"

namespace remoting {

ChromotingView::ChromotingView()
    : frame_width_(0),
      frame_height_(0) {
}

ChromotingView::~ChromotingView() {}

// TODO(garykac): This assumes a single screen. This will need to be adjusted
// to add support for mulitple monitors.
void ChromotingView::GetScreenSize(int* width, int* height) {
  *width = frame_width_;
  *height = frame_height_;
}

}  // namespace remoting
