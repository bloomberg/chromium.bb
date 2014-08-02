// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_VIDEO_FRAME_RECORDER_HOST_EXTENSION_H_
#define REMOTING_HOST_VIDEO_FRAME_RECORDER_HOST_EXTENSION_H_

#include "base/memory/scoped_ptr.h"
#include "remoting/host/host_extension.h"

namespace remoting {

// Extends Chromoting sessions with the ability to record raw frames and
// download them to the client. This can be used to obtain representative
// sequences of frames to run tests against.
class VideoFrameRecorderHostExtension : public HostExtension {
 public:
  VideoFrameRecorderHostExtension() {}
  virtual ~VideoFrameRecorderHostExtension() {}

  // Sets the maximum number of bytes that each session may record.
  void SetMaxContentBytes(int64_t max_content_bytes);

  // remoting::HostExtension interface.
  virtual std::string capability() const OVERRIDE;
  virtual scoped_ptr<HostExtensionSession> CreateExtensionSession(
      ClientSessionControl* client_session,
      protocol::ClientStub* client_stub) OVERRIDE;

 private:
  int64_t max_content_bytes_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameRecorderHostExtension);
};

}  // namespace remoting

#endif  // REMOTING_HOST_VIDEO_FRAME_RECORDER_HOST_EXTENSION_H_
