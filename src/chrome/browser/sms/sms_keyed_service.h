// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SMS_SMS_KEYED_SERVICE_H_
#define CHROME_BROWSER_SMS_SMS_KEYED_SERVICE_H_

#include <memory>

#include "base/macros.h"
#include "components/keyed_service/core/keyed_service.h"

namespace content {
class SmsService;
}

// A per-profile service that manages per-origin requests and incoming SMS
// messages.
class SmsKeyedService : public KeyedService {
 public:
  SmsKeyedService();
  ~SmsKeyedService() override;

  content::SmsService* Get();

 private:
  std::unique_ptr<content::SmsService> service_;

  DISALLOW_COPY_AND_ASSIGN(SmsKeyedService);
};

#endif  // CHROME_BROWSER_SMS_SMS_KEYED_SERVICE_H_
