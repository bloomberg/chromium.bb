// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_PLUGIN_MEDIA_SOURCE_VIDEO_RENDERER_H_
#define REMOTING_CLIENT_PLUGIN_MEDIA_SOURCE_VIDEO_RENDERER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "remoting/client/chromoting_stats.h"
#include "remoting/client/video_renderer.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_region.h"

namespace remoting {

// VideoRenderer implementation that packs data into a WebM stream that can be
// passed to <video> tag using MediaSource API.
class MediaSourceVideoRenderer : public VideoRenderer {
 public:
  class Delegate {
   public:
    Delegate() {}
    virtual ~Delegate() {}

    // Called when stream size changes.
    virtual void OnMediaSourceSize(const webrtc::DesktopSize& size,
                                   const webrtc::DesktopVector& dpi) = 0;

    // Called when desktop shape changes.
    virtual void OnMediaSourceShape(const webrtc::DesktopRegion& shape) = 0;

    // Called when the MediaSource needs to be reset (e.g. because screen size
    // has changed).
    virtual void OnMediaSourceReset(const std::string& format) = 0;

    // Called when new data becomes available.
    virtual void OnMediaSourceData(uint8_t* buffer, size_t buffer_size,
                                   bool keyframe) = 0;
  };

  explicit MediaSourceVideoRenderer(Delegate* delegate);
  virtual ~MediaSourceVideoRenderer();

  // VideoRenderer interface.
  virtual void Initialize(const protocol::SessionConfig& config) OVERRIDE;
  virtual ChromotingStats* GetStats() OVERRIDE;
  virtual void ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                  const base::Closure& done) OVERRIDE;

 private:
  // Helper class used to generate WebM stream.
  class VideoWriter;

  Delegate* delegate_;

  std::string format_string_;
  const char* codec_id_;

  scoped_ptr<VideoWriter> writer_;
  webrtc::DesktopVector frame_dpi_;
  webrtc::DesktopRegion desktop_shape_;

  ChromotingStats stats_;
  int64 latest_sequence_number_;

  DISALLOW_COPY_AND_ASSIGN(MediaSourceVideoRenderer);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_MEDIA_SOURCE_VIDEO_RENDERER_H_
