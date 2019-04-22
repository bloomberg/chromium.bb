// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_SERVICE_H_
#define UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_SERVICE_H_

#include <windows.media.control.h>

#include "base/component_export.h"

namespace system_media_controls {

class SystemMediaControlsServiceObserver;

// The SystemMediaControlsService connects with Windows's System Media Transport
// Controls, receives media key events, and propagates those events to listening
// SystemMediaControlsServiceObservers. The SystemMediaControlsService tells the
// System Media Transport Controls which actions are currently available. The
// SystemMediaControlsService is also used to keep the System Media Transport
// Controls informed of the current media playback state and metadata.
class COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS) SystemMediaControlsService {
 public:
  // Returns the singleton instance, creating if necessary. If the
  // SystemMediaControlsService fails to connect to the
  // SystemMediaTransportControls, this returns null.
  static SystemMediaControlsService* GetInstance();

  virtual void AddObserver(SystemMediaControlsServiceObserver* observer) = 0;
  virtual void RemoveObserver(SystemMediaControlsServiceObserver* observer) = 0;

  // TODO(steimel): Add other controls.
  // Enable or disable specific controls.
  virtual void SetIsNextEnabled(bool value) = 0;
  virtual void SetIsPreviousEnabled(bool value) = 0;
  virtual void SetIsPlayEnabled(bool value) = 0;
  virtual void SetIsPauseEnabled(bool value) = 0;
  virtual void SetIsStopEnabled(bool value) = 0;

  // Setters for metadata.
  virtual void SetPlaybackStatus(
      ABI::Windows::Media::MediaPlaybackStatus status) = 0;

 protected:
  virtual ~SystemMediaControlsService();
};

}  // namespace system_media_controls

#endif  // UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_SERVICE_H_
