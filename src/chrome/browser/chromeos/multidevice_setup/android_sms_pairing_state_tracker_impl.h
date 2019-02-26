// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_PAIRING_STATE_TRACKER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_PAIRING_STATE_TRACKER_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "chromeos/services/multidevice_setup/public/cpp/android_sms_pairing_state_tracker.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace net {
class CanonicalCookie;
}  // namespace net

namespace chromeos {

namespace multidevice_setup {

// Concrete AndroidSmsPairingStateTracker implementation.
class AndroidSmsPairingStateTrackerImpl
    : public AndroidSmsPairingStateTracker,
      public network::mojom::CookieChangeListener {
 public:
  explicit AndroidSmsPairingStateTrackerImpl(
      content::BrowserContext* browser_context);
  ~AndroidSmsPairingStateTrackerImpl() override;

  // AndroidSmsPairingStateTracker:
  bool IsAndroidSmsPairingComplete() override;

 private:
  // network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CanonicalCookie& cookie,
                      network::mojom::CookieChangeCause cause) override;

  void FetchMessagesPairingState();
  void OnCookiesRetrieved(const std::vector<net::CanonicalCookie>& cookies);
  network::mojom::CookieManager* GetCookieManager();

  content::BrowserContext* browser_context_;
  mojo::Binding<network::mojom::CookieChangeListener> cookie_listener_binding_;
  bool was_paired_on_last_update_ = false;
  base::WeakPtrFactory<AndroidSmsPairingStateTrackerImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AndroidSmsPairingStateTrackerImpl);
};

}  // namespace multidevice_setup

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_MULTIDEVICE_SETUP_ANDROID_SMS_PAIRING_STATE_TRACKER_IMPL_H_
