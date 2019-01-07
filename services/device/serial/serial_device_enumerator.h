// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_H_
#define SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "services/device/public/mojom/serial.mojom.h"
#include "services/device/serial/serial_io_handler.h"

namespace device {

// Discovers and enumerates serial devices available to the host.
class SerialDeviceEnumerator {
 public:
  using TokenPathMap = std::map<base::UnguessableToken, std::string>;

  static std::unique_ptr<SerialDeviceEnumerator> Create();

  SerialDeviceEnumerator();
  virtual ~SerialDeviceEnumerator();

  virtual std::vector<mojom::SerialPortInfoPtr> GetDevices() = 0;

  base::Optional<std::string> GetPathFromToken(
      const base::UnguessableToken& token);

 protected:
  const base::UnguessableToken& GetTokenFromPath(const std::string& path);

 private:
  TokenPathMap token_path_map_;
};

}  // namespace device

#endif  // SERVICES_DEVICE_SERIAL_SERIAL_DEVICE_ENUMERATOR_H_
