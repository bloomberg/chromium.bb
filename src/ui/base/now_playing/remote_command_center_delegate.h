// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_H_
#define UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_H_

#include <memory>

#include "base/component_export.h"

namespace now_playing {

class RemoteCommandCenterDelegateObserver;

// The RemoteCommandCenterDelegate connects with Mac OS's MPRemoteCommandCenter,
// receives remote control events, and propagates those events to listening
// RemoteCommandCenterDelegateObservers. The RemoteCommandCenterDelegate also
// tells the MPRemoteCommandCenter which actions are currently available.
// https://developer.apple.com/documentation/mediaplayer/mpremotecommandcenter
class COMPONENT_EXPORT(NOW_PLAYING) RemoteCommandCenterDelegate {
 public:
  virtual ~RemoteCommandCenterDelegate();

  // Creates and returns an instance of RemoteCommandCenterDelegate. Returns
  // null if the current version of Mac OS does not support
  // MPRemoteCommandCenter.
  static std::unique_ptr<RemoteCommandCenterDelegate> Create();

  // Returns the singleton instance.
  static RemoteCommandCenterDelegate* GetInstance();

  // Observers receive remote commands from the OS (e.g. media key presses).
  virtual void AddObserver(RemoteCommandCenterDelegateObserver* observer) = 0;
  virtual void RemoveObserver(
      RemoteCommandCenterDelegateObserver* observer) = 0;

  // Used for toggling whether specific media actions are available.
  // TODO(steimel): Support all remote command actions.
  virtual void SetCanPlay(bool enable) = 0;
  virtual void SetCanPause(bool enable) = 0;
  virtual void SetCanStop(bool enable) = 0;
  virtual void SetCanPlayPause(bool enable) = 0;
  virtual void SetCanGoNextTrack(bool enable) = 0;
  virtual void SetCanGoPreviousTrack(bool enable) = 0;
};

}  // namespace now_playing

#endif  // UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_H_
