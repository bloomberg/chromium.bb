// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SERVICE_H_

#include <memory>
#include "chrome/browser/chromeos/android_sms/connection_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

namespace android_sms {

// Profile Keyed Service responsible for maintaining connection with
// android web messages service worker and initiating pairing.
class AndroidSmsService : public KeyedService,
                          public session_manager::SessionManagerObserver {
 public:
  explicit AndroidSmsService(content::BrowserContext* context);
  ~AndroidSmsService() override;

 private:
  // KeyedService:
  void Shutdown() override;

  // session_manager::SessionManagerObserver
  void OnSessionStateChanged() override;

  content::BrowserContext* browser_context_;
  std::unique_ptr<ConnectionManager> connection_manager_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsService);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_SERVICE_H_
