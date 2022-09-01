// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_HARDWARE_INFO_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_HARDWARE_INFO_DELEGATE_H_

#include <memory>
#include <string>

#include "ash/webui/telemetry_extension_ui/mojom/probe_service.mojom.h"
#include "ash/webui/telemetry_extension_ui/services/probe_service.h"
#include "base/callback.h"

namespace chromeos {

// HardwareInfoDelegate is a helper class to get hardware info such as device
// manufacturer.
class HardwareInfoDelegate {
 public:
  class Factory {
   public:
    static std::unique_ptr<HardwareInfoDelegate> Create();

    static void SetForTesting(Factory* test_factory);

    virtual ~Factory();

   protected:
    virtual std::unique_ptr<HardwareInfoDelegate> CreateInstance() = 0;

   private:
    static Factory* test_factory_;
  };

  using ManufacturerCallback = base::OnceCallback<void(std::string)>;

  HardwareInfoDelegate(const HardwareInfoDelegate&) = delete;
  HardwareInfoDelegate& operator=(const HardwareInfoDelegate&) = delete;
  virtual ~HardwareInfoDelegate();

  virtual void GetManufacturer(ManufacturerCallback done_cb);

 protected:
  HardwareInfoDelegate();

 private:
  void FallbackHandler(ManufacturerCallback done_cb,
                       std::string probe_service_result);

  mojo::Remote<ash::health::mojom::ProbeService> remote_probe_service_;
  ash::ProbeService probe_service_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_EXTENSIONS_TELEMETRY_API_HARDWARE_INFO_DELEGATE_H_
