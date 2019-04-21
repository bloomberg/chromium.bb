// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_NETWORK_SMS_OBSERVER_H_
#define ASH_SYSTEM_NETWORK_SMS_OBSERVER_H_

#include "chromeos/network/network_sms_handler.h"

namespace base {
class DictionaryValue;
}

namespace ash {

// SmsObserver is called when a new sms message is received. Then it shows the
// sms message to the user in the notification center.
class SmsObserver : public chromeos::NetworkSmsHandler::Observer {
 public:
  SmsObserver();
  ~SmsObserver() override;

  // chromeos::NetworkSmsHandler::Observer:
  void MessageReceived(const base::DictionaryValue& message) override;

 private:
  // Used to create notification identifier.
  uint32_t message_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SmsObserver);
};

}  // namespace ash

#endif  // ASH_SYSTEM_NETWORK_SMS_OBSERVER_H_
