// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_PLATFORM_SCREEN_H_
#define SERVICES_UI_DISPLAY_PLATFORM_SCREEN_H_

#include <memory>

#include "services/ui/display/platform_screen_delegate.h"

namespace display {

// PlatformScreen provides the necessary functionality to configure all
// attached physical displays.
class PlatformScreen {
 public:
  virtual ~PlatformScreen() {}

  // Creates a PlatformScreen instance.
  static std::unique_ptr<PlatformScreen> Create();

  // Triggers initial display configuration to start. On device this will
  // configuration the connected displays. Off device this will create one or
  // more fake displays and pretend to configure them. A non-null |delegate|
  // must be provided that will receive notifications when displays are added,
  // removed or modified.
  virtual void Init(PlatformScreenDelegate* delegate) = 0;

  virtual int64_t GetPrimaryDisplayId() const = 0;
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_PLATFORM_SCREEN_H_
