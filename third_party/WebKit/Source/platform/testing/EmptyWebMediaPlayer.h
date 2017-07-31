// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "public/platform/WebMediaPlayer.h"

#ifndef EmptyWebMediaPlayer_h
#define EmptyWebMediaPlayer_h

namespace blink {

// An empty WebMediaPlayer used only for tests. This class defines the methods
// of WebMediaPlayer so that mock WebMediaPlayers don't need to redefine them if
// they don't care their behavior.
class EmptyWebMediaPlayer : public WebMediaPlayer {
 public:
  ~EmptyWebMediaPlayer() override {}

  void Load(LoadType, const WebMediaPlayerSource&, CORSMode) override {}
  void Play() override {}
  void Pause() override {}
  bool SupportsSave() const override { return false; }
  void Seek(double seconds) override {}
  void SetRate(double) override {}
  void SetVolume(double) override {}
  WebTimeRanges Buffered() const override;
  WebTimeRanges Seekable() const override;
  void SetSinkId(const WebString& sink_id,
                 const WebSecurityOrigin&,
                 WebSetSinkIdCallbacks*) override {}
  bool HasVideo() const override { return false; }
  bool HasAudio() const override { return false; }
  WebSize NaturalSize() const override;
  WebSize VisibleRect() const override;
  bool Paused() const override { return false; }
  bool Seeking() const override { return false; }
  double Duration() const override { return 0.0; }
  double CurrentTime() const override { return 0.0; }
  NetworkState GetNetworkState() const override { return kNetworkStateEmpty; }
  ReadyState GetReadyState() const override { return kReadyStateHaveNothing; }
  WebString GetErrorMessage() const override;
  bool DidLoadingProgress() override { return false; }
  bool HasSingleSecurityOrigin() const override { return true; }
  bool DidPassCORSAccessCheck() const override { return true; }
  double MediaTimeForTimeValue(double time_value) const override {
    return time_value;
  };
  unsigned DecodedFrameCount() const override { return 0; }
  unsigned DroppedFrameCount() const override { return 0; }
  size_t AudioDecodedByteCount() const override { return 0; }
  size_t VideoDecodedByteCount() const override { return 0; }
  void Paint(WebCanvas*, const WebRect&, cc::PaintFlags&) override {}
};

}  // namespace blink

#endif  // EmptyWebMediaPlayer_h
