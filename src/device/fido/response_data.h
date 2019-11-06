// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_RESPONSE_DATA_H_
#define DEVICE_FIDO_RESPONSE_DATA_H_

#include <stdint.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include "base/component_export.h"
#include "base/macros.h"
#include "device/fido/fido_constants.h"

namespace device {

// Base class for AuthenticatorMakeCredentialResponse and
// AuthenticatorGetAssertionResponse.
class COMPONENT_EXPORT(DEVICE_FIDO) ResponseData {
 public:
  virtual ~ResponseData();

  virtual const std::array<uint8_t, kRpIdHashLength>& GetRpIdHash() const = 0;

  std::string GetId() const;

  const std::vector<uint8_t>& raw_credential_id() const {
    return raw_credential_id_;
  }

 protected:
  explicit ResponseData(std::vector<uint8_t> raw_credential_id);
  ResponseData();

  // Moveable.
  ResponseData(ResponseData&& other);
  ResponseData& operator=(ResponseData&& other);

  std::vector<uint8_t> raw_credential_id_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResponseData);
};

}  // namespace device

#endif  // DEVICE_FIDO_RESPONSE_DATA_H_
