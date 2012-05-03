// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Interface of a host that receives commands from a Chromoting client.
//
// This interface handles control messages defined in control.proto.

#ifndef REMOTING_PROTOCOL_HOST_STUB_H_
#define REMOTING_PROTOCOL_HOST_STUB_H_

#include "base/basictypes.h"

namespace remoting {
namespace protocol {

class ClientDimensions;
class VideoControl;

class HostStub {
 public:
  HostStub() {}
  virtual ~HostStub() {}

  // Notification of the available client display dimensions.
  // This may be used to resize the host display to match the client area.
  virtual void NotifyClientDimensions(const ClientDimensions& dimensions) = 0;

  // Configures video update properties. Currently only pausing & resuming the
  // video channel is supported.
  virtual void ControlVideo(const VideoControl& video_control) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HostStub);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_STUB_H_
