// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_DISPLAY_SCREEN_MANAGER_STUB_H_
#define SERVICES_UI_DISPLAY_SCREEN_MANAGER_STUB_H_

#include <stdint.h>

#include "base/memory/weak_ptr.h"
#include "services/ui/display/screen_manager.h"
#include "services/ui/display/viewport_metrics.h"

namespace display {

// ScreenManagerStub provides the necessary functionality to configure a fixed
// 1024x768 display for non-ozone platforms.
class ScreenManagerStub : public ScreenManager {
 public:
  ScreenManagerStub();
  ~ScreenManagerStub() override;

 private:
  // Fake creation of a single 1024x768 display.
  void FixedSizeScreenConfiguration();

  // ScreenManager.
  void AddInterfaces(service_manager::InterfaceRegistry* registry) override;
  void Init(ScreenManagerDelegate* delegate) override;
  void RequestCloseDisplay(int64_t display_id) override;
  int64_t GetPrimaryDisplayId() const override;

  // Sample display information.
  int64_t display_id_ = 1;
  ViewportMetrics display_metrics_;

  ScreenManagerDelegate* delegate_ = nullptr;

  base::WeakPtrFactory<ScreenManagerStub> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManagerStub);
};

}  // namespace display

#endif  // SERVICES_UI_DISPLAY_SCREEN_MANAGER_STUB_H_
