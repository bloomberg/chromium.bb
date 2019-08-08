// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SMS_SMS_PROVIDER_DESKTOP_H_
#define CONTENT_BROWSER_SMS_SMS_PROVIDER_DESKTOP_H_

#include "content/browser/sms/sms_provider.h"

namespace content {

class SmsProviderDesktop : public SmsProvider {
 public:
  SmsProviderDesktop() = default;
  ~SmsProviderDesktop() override = default;
  void Retrieve() override;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SMS_SMS_PROVIDER_DESKTOP_H_
