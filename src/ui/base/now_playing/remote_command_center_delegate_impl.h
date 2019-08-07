// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_IMPL_H_
#define UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_IMPL_H_

#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/mac/scoped_nsobject.h"
#include "base/observer_list.h"
#include "ui/base/now_playing/remote_command_center_delegate.h"
#include "ui/base/now_playing/remote_command_center_delegate_observer.h"

@class RemoteCommandCenterDelegateNS;

namespace now_playing {

// Implementation of RemoteCommandCenterDelegate that wraps an NSObject which
// interfaces with the MPRemoteCommandCenter.
class COMPONENT_EXPORT(NOW_PLAYING)
    API_AVAILABLE(macos(10.12.2)) RemoteCommandCenterDelegateImpl
    : public RemoteCommandCenterDelegate,
      RemoteCommandCenterDelegateObserver {
 public:
  RemoteCommandCenterDelegateImpl();
  ~RemoteCommandCenterDelegateImpl() override;

  static RemoteCommandCenterDelegateImpl* GetInstance();

  // RemoteCommandCenterDelegate implementation.
  void AddObserver(RemoteCommandCenterDelegateObserver* observer) override;
  void RemoveObserver(RemoteCommandCenterDelegateObserver* observer) override;
  void SetCanPlay(bool enable) override;
  void SetCanPause(bool enable) override;
  void SetCanStop(bool enable) override;
  void SetCanPlayPause(bool enable) override;
  void SetCanGoNextTrack(bool enable) override;
  void SetCanGoPreviousTrack(bool enable) override;

  // RemoteCommandCenterDelegateObserver implementation. Called by
  // |remote_command_center_delegate_ns_|.
  void OnNext() override;
  void OnPrevious() override;
  void OnPause() override;
  void OnPlayPause() override;
  void OnStop() override;
  void OnPlay() override;

 private:
  // Used to track which commands we're already listening for.
  enum class Command {
    kPlay,
    kPause,
    kStop,
    kPlayPause,
    kNextTrack,
    kPreviousTrack,
  };

  static RemoteCommandCenterDelegateImpl* instance_;

  bool ShouldSetCommandActive(Command command, bool will_enable);

  base::scoped_nsobject<RemoteCommandCenterDelegateNS>
      remote_command_center_delegate_ns_;
  base::ObserverList<RemoteCommandCenterDelegateObserver> observers_;
  base::flat_set<Command> active_commands_;

  DISALLOW_COPY_AND_ASSIGN(RemoteCommandCenterDelegateImpl);
};

}  // namespace now_playing

#endif  // UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_IMPL_H_
