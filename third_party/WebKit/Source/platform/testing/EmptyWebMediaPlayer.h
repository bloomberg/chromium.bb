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

  void load(LoadType, const WebMediaPlayerSource&, CORSMode) override {}
  void play() override {}
  void pause() override {}
  bool supportsSave() const override { return false; }
  void seek(double seconds) override {}
  void setRate(double) override {}
  void setVolume(double) override {}
  WebTimeRanges buffered() const override;
  WebTimeRanges seekable() const override;
  void setSinkId(const WebString& sinkId,
                 const WebSecurityOrigin&,
                 WebSetSinkIdCallbacks*) override {}
  bool hasVideo() const override { return false; }
  bool hasAudio() const override { return false; }
  WebSize naturalSize() const override;
  bool paused() const override { return false; }
  bool seeking() const override { return false; }
  double duration() const override { return 0.0; }
  double currentTime() const override { return 0.0; }
  NetworkState getNetworkState() const override { return NetworkStateEmpty; }
  ReadyState getReadyState() const override { return ReadyStateHaveNothing; }
  WebString getErrorMessage() override { return WebString(); }
  bool didLoadingProgress() override { return false; }
  bool hasSingleSecurityOrigin() const override { return true; }
  bool didPassCORSAccessCheck() const override { return true; }
  double mediaTimeForTimeValue(double timeValue) const override {
    return timeValue;
  };
  unsigned decodedFrameCount() const override { return 0; }
  unsigned droppedFrameCount() const override { return 0; }
  size_t audioDecodedByteCount() const override { return 0; }
  size_t videoDecodedByteCount() const override { return 0; }
  void paint(WebCanvas*, const WebRect&, cc::PaintFlags&) override {}
};

}  // namespace blink

#endif  // EmptyWebMediaPlayer_h
