// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_CONNECTION_ESTABLISHER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_CONNECTION_ESTABLISHER_IMPL_H_

#include "base/time/clock.h"
#include "chrome/browser/chromeos/android_sms/connection_establisher.h"

namespace chromeos {

namespace android_sms {

// Concrete ConnectionEstablisher implementation that initiates a background
// connection between the Android Messages for Web service worker and the
// Tachyon server by dispatching a known message string to it.
//
// To allow the service worker to continue running past it's default timeout
// this message is sent using a special long-running dispatch.
class ConnectionEstablisherImpl : public ConnectionEstablisher {
 public:
  explicit ConnectionEstablisherImpl(base::Clock* clock);
  ~ConnectionEstablisherImpl() override;

  void EstablishConnection(
      content::ServiceWorkerContext* service_worker_context,
      ConnectionMode connection_mode) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ConnectionEstablisherImplTest, EstablishConnection);

  void SendStartStreamingMessageIfNotConnected(
      content::ServiceWorkerContext* service_worker_context,
      ConnectionMode connection_mode);

  void OnMessageDispatchResult(bool status);

  static const char kStartStreamingMessage[];
  static const char kResumeStreamingMessage[];
  base::Clock* clock_;
  bool is_connected_ = false;
  base::Time start_connection_message_time_;
  DISALLOW_COPY_AND_ASSIGN(ConnectionEstablisherImpl);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_CONNECTION_ESTABLISHER_IMPL_H_
