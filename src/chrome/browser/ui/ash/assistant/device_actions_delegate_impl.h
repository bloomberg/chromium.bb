// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_DELEGATE_IMPL_H_
#define CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_DELEGATE_IMPL_H_

#include <string>

#include "chrome/browser/ui/ash/assistant/device_actions_delegate.h"
#include "chromeos/services/assistant/public/mojom/assistant.mojom.h"

class DeviceActionsDelegateImpl : public DeviceActionsDelegate {
 public:
  DeviceActionsDelegateImpl();

  // Disallow copy and assign.
  DeviceActionsDelegateImpl(const DeviceActionsDelegateImpl&) = delete;
  DeviceActionsDelegateImpl& operator=(const DeviceActionsDelegateImpl&) =
      delete;

  ~DeviceActionsDelegateImpl() override;

  chromeos::assistant::mojom::AppStatus GetAndroidAppStatus(
      const std::string& package_name) override;
};

#endif  // CHROME_BROWSER_UI_ASH_ASSISTANT_DEVICE_ACTIONS_DELEGATE_IMPL_H_
