// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_OBSERVER_H_
#define UI_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_OBSERVER_H_

#include "ui/chromeos/ui_chromeos_export.h"

namespace ui {
namespace network_icon {

// Observer interface class for animating network icons.
class UI_CHROMEOS_EXPORT AnimationObserver {
 public:
  // Called when the image has changed due to animation. The callback should
  // trigger a call to GetImageForNetwork() to retrieve the image.
  virtual void NetworkIconChanged() = 0;

 protected:
  virtual ~AnimationObserver() {}
};

}  // namespace network_icon
}  // namespace ui

#endif  // UI_CHROMEOS_NETWORK_NETWORK_ICON_ANIMATION_OBSERVER_H_
