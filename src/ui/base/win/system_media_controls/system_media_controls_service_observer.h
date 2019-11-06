// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_SERVICE_OBSERVER_H_
#define UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_SERVICE_OBSERVER_H_

#include "base/component_export.h"
#include "base/observer_list_types.h"

namespace system_media_controls {

// Interface to observe events on the SystemMediaControlsService.
class COMPONENT_EXPORT(SYSTEM_MEDIA_CONTROLS) SystemMediaControlsServiceObserver
    : public base::CheckedObserver {
 public:
  virtual void OnNext() = 0;
  virtual void OnPrevious() = 0;
  virtual void OnPause() = 0;
  virtual void OnStop() = 0;
  virtual void OnPlay() = 0;

 protected:
  ~SystemMediaControlsServiceObserver() override;
};

}  // namespace system_media_controls

#endif  // UI_BASE_WIN_SYSTEM_MEDIA_CONTROLS_SYSTEM_MEDIA_CONTROLS_SERVICE_OBSERVER_H_
