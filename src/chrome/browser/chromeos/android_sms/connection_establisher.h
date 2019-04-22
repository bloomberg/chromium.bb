// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_CONNECTION_ESTABLISHER_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_CONNECTION_ESTABLISHER_H_

#include "base/macros.h"
#include "content/public/browser/service_worker_context.h"
#include "url/gurl.h"

namespace chromeos {

namespace android_sms {

// Establishes a background connection from the Android Messages for Web
// service worker to the Tachyon server.
class ConnectionEstablisher {
 public:
  enum class ConnectionMode { kStartConnection, kResumeExistingConnection };

  virtual ~ConnectionEstablisher() = default;

  virtual void EstablishConnection(
      const GURL& url,
      ConnectionMode connection_mode,
      content::ServiceWorkerContext* service_worker_context) = 0;

  virtual void TearDownConnection(
      const GURL& url,
      content::ServiceWorkerContext* service_worker_context) = 0;

 protected:
  ConnectionEstablisher() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ConnectionEstablisher);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_CONNECTION_ESTABLISHER_H_
