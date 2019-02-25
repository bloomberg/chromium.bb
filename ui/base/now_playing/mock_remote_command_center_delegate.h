// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NOW_PLAYING_MOCK_REMOTE_COMMAND_CENTER_DELEGATE_H_
#define UI_BASE_NOW_PLAYING_MOCK_REMOTE_COMMAND_CENTER_DELEGATE_H_

#include "base/macros.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/now_playing/remote_command_center_delegate.h"

namespace now_playing {

class RemoteCommandCenterDelegateObserver;

// Mock implementation of RemoteCommandCenterDelegate for testing.
class MockRemoteCommandCenterDelegate : public RemoteCommandCenterDelegate {
 public:
  MockRemoteCommandCenterDelegate();
  ~MockRemoteCommandCenterDelegate();

  // RemoteCommandCenterDelegate implementation.
  MOCK_METHOD1(AddObserver,
               void(RemoteCommandCenterDelegateObserver* observer));
  MOCK_METHOD1(RemoveObserver,
               void(RemoteCommandCenterDelegateObserver* observer));
  MOCK_METHOD1(SetCanPlay, void(bool enable));
  MOCK_METHOD1(SetCanPause, void(bool enable));
  MOCK_METHOD1(SetCanStop, void(bool enable));
  MOCK_METHOD1(SetCanPlayPause, void(bool enable));
  MOCK_METHOD1(SetCanGoNextTrack, void(bool enable));
  MOCK_METHOD1(SetCanGoPreviousTrack, void(bool enable));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRemoteCommandCenterDelegate);
};

}  // namespace now_playing

#endif  // UI_BASE_NOW_PLAYING_MOCK_REMOTE_COMMAND_CENTER_DELEGATE_H_
