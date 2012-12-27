// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
#define REMOTING_HOST_DESKTOP_ENVIRONMENT_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"

namespace remoting {

class AudioCapturer;
class EventExecutor;
class VideoFrameCapturer;

namespace protocol {
class ClipboardStub;
}

class DesktopEnvironment {
 public:
  DesktopEnvironment(scoped_ptr<AudioCapturer> audio_capturer,
                     scoped_ptr<EventExecutor> event_executor,
                     scoped_ptr<VideoFrameCapturer> video_capturer);
  virtual ~DesktopEnvironment();

  AudioCapturer* audio_capturer() const { return audio_capturer_.get(); }
  EventExecutor* event_executor() const { return event_executor_.get(); }
  VideoFrameCapturer* video_capturer() const { return video_capturer_.get(); }

  // Starts the desktop environment passing |client_jid| of the attached
  // authenticated session. Registers |client_clipboard| to receive
  // notifications about local clipboard changes. |disconnect_callback| can be
  // invoked by the DesktopEnvironment to request the client session to be
  // disconnected. |disconnect_callback| is invoked on the same thread Start()
  // has been called on.
  virtual void Start(
      scoped_ptr<protocol::ClipboardStub> client_clipboard,
      const std::string& client_jid,
      const base::Closure& disconnect_callback);

 private:
  // Used to capture audio to deliver to clients.
  scoped_ptr<AudioCapturer> audio_capturer_;

  // Executes input and clipboard events received from the client.
  scoped_ptr<EventExecutor> event_executor_;

  // Used to capture video to deliver to clients.
  scoped_ptr<VideoFrameCapturer> video_capturer_;

  DISALLOW_COPY_AND_ASSIGN(DesktopEnvironment);
};

}  // namespace remoting

#endif  // REMOTING_HOST_DESKTOP_ENVIRONMENT_H_
