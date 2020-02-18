// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_HOST_VIDEO_STATE_DISPATCHER_H_
#define REMOTING_PROTOCOL_HOST_VIDEO_STATE_DISPATCHER_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "remoting/protocol/channel_dispatcher_base.h"
#include "remoting/protocol/video_stats_stub.h"

namespace remoting {
namespace protocol {

class HostVideoStatsDispatcher : public ChannelDispatcherBase,
                                 public VideoStatsStub {
 public:
  explicit HostVideoStatsDispatcher(const std::string& stream_name);
  ~HostVideoStatsDispatcher() override;

  // VideoStatsStub interface.
  void OnVideoFrameStats(uint32_t frame_id,
                         const HostFrameStats& frame_stats) override;

 private:
  void OnIncomingMessage(std::unique_ptr<CompoundBuffer> message) override;

  DISALLOW_COPY_AND_ASSIGN(HostVideoStatsDispatcher);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_HOST_VIDEO_STATE_DISPATCHER_H_
