// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_OBSERVER_H_
#define UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace now_playing {

// Interface to observe events from the MPRemoteCommandCenter.
class COMPONENT_EXPORT(NOW_PLAYING) RemoteCommandCenterDelegateObserver
    : public base::CheckedObserver {
 public:
  RemoteCommandCenterDelegateObserver();
  ~RemoteCommandCenterDelegateObserver() override;

  virtual void OnNext() {}
  virtual void OnPrevious() {}
  virtual void OnPause() {}
  virtual void OnPlayPause() {}
  virtual void OnStop() {}
  virtual void OnPlay() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoteCommandCenterDelegateObserver);
};

}  // namespace now_playing

#endif  // UI_BASE_NOW_PLAYING_REMOTE_COMMAND_CENTER_DELEGATE_OBSERVER_H_
