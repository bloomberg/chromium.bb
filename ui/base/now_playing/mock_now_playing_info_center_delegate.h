// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NOW_PLAYING_MOCK_NOW_PLAYING_INFO_CENTER_DELEGATE_H_
#define UI_BASE_NOW_PLAYING_MOCK_NOW_PLAYING_INFO_CENTER_DELEGATE_H_

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/now_playing/now_playing_info_center_delegate.h"

namespace now_playing {

// Mock implementation of NowPlayingInfoCenterDelegate for testing.
class MockNowPlayingInfoCenterDelegate : public NowPlayingInfoCenterDelegate {
 public:
  MockNowPlayingInfoCenterDelegate();
  ~MockNowPlayingInfoCenterDelegate() override;

  // NowPlayingInfoCenterDelegate implementation.
  MOCK_METHOD1(SetPlaybackState, void(PlaybackState state));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockNowPlayingInfoCenterDelegate);
};

}  // namespace now_playing

#endif  // UI_BASE_NOW_PLAYING_MOCK_NOW_PLAYING_INFO_CENTER_DELEGATE_H_
