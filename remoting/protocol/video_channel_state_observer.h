// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_
#define REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_

#include "base/time/time.h"

namespace remoting {
namespace protocol {

class VideoChannelStateObserver {
 public:
  virtual void OnKeyFrameRequested() = 0;
  virtual void OnChannelParameters(int packet_loss, base::TimeDelta rtt) = 0;
  virtual void OnTargetBitrateChanged(int bitrate_kbps) = 0;

 protected:
  virtual ~VideoChannelStateObserver() {}
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_VIDEO_CHANNEL_STATE_OBSERVER_H_
