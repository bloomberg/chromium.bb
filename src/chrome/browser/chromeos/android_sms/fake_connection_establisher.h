// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FAKE_CONNECTION_ESTABLISHER_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FAKE_CONNECTION_ESTABLISHER_H_

#include <tuple>
#include <vector>

#include "chrome/browser/chromeos/android_sms/connection_establisher.h"

namespace chromeos {

namespace android_sms {

// Test ConnectionEstablisher implementation.
class FakeConnectionEstablisher : public ConnectionEstablisher {
 public:
  FakeConnectionEstablisher();
  ~FakeConnectionEstablisher() override;

  const std::vector<
      std::tuple<GURL, ConnectionMode, content::ServiceWorkerContext*>>&
  establish_connection_calls() const {
    return establish_connection_calls_;
  }

  const std::vector<std::tuple<GURL, content::ServiceWorkerContext*>>&
  tear_down_connection_calls() const {
    return tear_down_connection_calls_;
  }

 private:
  // ConnectionEstablisher:
  void EstablishConnection(
      const GURL& url,
      ConnectionMode connection_mode,
      content::ServiceWorkerContext* service_worker_context) override;

  void TearDownConnection(
      const GURL& url,
      content::ServiceWorkerContext* service_worker_context) override;

  std::vector<std::tuple<GURL, ConnectionMode, content::ServiceWorkerContext*>>
      establish_connection_calls_;
  std::vector<std::tuple<GURL, content::ServiceWorkerContext*>>
      tear_down_connection_calls_;

  DISALLOW_COPY_AND_ASSIGN(FakeConnectionEstablisher);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_FAKE_CONNECTION_ESTABLISHER_H_
