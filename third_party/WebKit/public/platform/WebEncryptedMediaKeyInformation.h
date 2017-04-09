// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebEncryptedMediaKeyInformation_h
#define WebEncryptedMediaKeyInformation_h

#include "WebCommon.h"
#include "public/platform/WebData.h"

namespace blink {

class BLINK_PLATFORM_EXPORT WebEncryptedMediaKeyInformation {
 public:
  enum class KeyStatus {
    kUsable,
    kExpired,
    kReleased,
    kOutputRestricted,
    kOutputDownscaled,
    kStatusPending,
    kInternalError
  };

  WebEncryptedMediaKeyInformation();
  ~WebEncryptedMediaKeyInformation();

  WebData Id() const;
  void SetId(const WebData&);

  KeyStatus Status() const;
  void SetStatus(KeyStatus);

  uint32_t SystemCode() const;
  void SetSystemCode(uint32_t);

 private:
  WebData id_;
  KeyStatus status_;
  uint32_t system_code_;
};

}  // namespace blink

#endif  // WebEncryptedMediaKeyInformation_h
