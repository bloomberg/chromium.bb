// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/desktop_environment.h"

#include "base/compiler_specific.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/desktop_environment.h"
#include "remoting/host/event_executor.h"
#include "remoting/host/video_frame_capturer.h"

namespace remoting {

DesktopEnvironment::DesktopEnvironment(
    scoped_ptr<AudioCapturer> audio_capturer,
    scoped_ptr<EventExecutor> event_executor,
    scoped_ptr<VideoFrameCapturer> video_capturer)
    : audio_capturer_(audio_capturer.Pass()),
      event_executor_(event_executor.Pass()),
      video_capturer_(video_capturer.Pass()) {
}

DesktopEnvironment::~DesktopEnvironment() {
  event_executor_.release()->StopAndDelete();
}

void DesktopEnvironment::Start(
    scoped_ptr<protocol::ClipboardStub> client_clipboard) {
  event_executor_->Start(client_clipboard.Pass());
}

}  // namespace remoting
