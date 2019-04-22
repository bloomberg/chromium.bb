// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICE_DESCRIPTION_H_
#define COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICE_DESCRIPTION_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string_piece_forward.h"
#include "base/values.h"

namespace cloud_devices {

// Provides parsing, serialization and validation Cloud Device Description or
// Cloud Job Ticket.
// https://developers.google.com/cloud-print/docs/cdd
class CloudDeviceDescription {
 public:
  CloudDeviceDescription();
  ~CloudDeviceDescription();

  bool InitFromString(const std::string& json);
  bool InitFromValue(base::Value value);

  static bool IsValidTicket(const base::Value& value);

  std::string ToString() const;

  base::Value ToValue() &&;

  // Returns item of given type with capability/option.
  // Returns nullptr if missing.
  const base::Value* GetItem(const std::vector<base::StringPiece>& path,
                             base::Value::Type type) const;

  // Creates item with given type for capability/option.
  // Returns nullptr if an intermediate Value in the path is not a dictionary.
  base::Value* CreateItem(const std::vector<base::StringPiece>& path,
                          base::Value::Type type);

 private:
  base::Value root_;

  DISALLOW_COPY_AND_ASSIGN(CloudDeviceDescription);
};

}  // namespace cloud_devices

#endif  // COMPONENTS_CLOUD_DEVICES_COMMON_CLOUD_DEVICE_DESCRIPTION_H_
