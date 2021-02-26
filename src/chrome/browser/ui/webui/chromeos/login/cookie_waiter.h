// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_COOKIE_WAITER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_COOKIE_WAITER_H_

#include <string>

#include "base/callback.h"
#include "base/timer/timer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

namespace chromeos {

class CookieWaiter : public network::mojom::CookieChangeListener {
 public:
  CookieWaiter(network::mojom::CookieManager* cookie_manager,
               const std::string& cookie_name,
               base::RepeatingClosure on_cookie_change,
               base::OnceClosure on_timeout);
  ~CookieWaiter() override;

  CookieWaiter(const CookieWaiter&) = delete;
  CookieWaiter& operator=(const CookieWaiter&) = delete;

  // network::mojom::CookieChangeListener:
  void OnCookieChange(const net::CookieChangeInfo&) override;

 private:
  base::RepeatingClosure on_cookie_change_;

  mojo::Receiver<network::mojom::CookieChangeListener> cookie_listener_{this};
  base::OneShotTimer waiting_timer_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_COOKIE_WAITER_H_
