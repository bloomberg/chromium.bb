// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_NOTIFIER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_NOTIFIER_H_

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

class Profile;

// Sends cookie-change notifications on the UI thread via
// chrome::NOTIFICATION_COOKIE_CHANGED_FOR_EXTENSIONS for all cookie
// changes associated with the given profile.
class ExtensionCookieNotifier : public network::mojom::CookieChangeListener {
 public:
  explicit ExtensionCookieNotifier(Profile* profile);
  ~ExtensionCookieNotifier() override;

 private:
  void StartListening();
  void OnConnectionError();

  // network::mojom::CookieChangeListener implementation.
  void OnCookieChange(const net::CanonicalCookie& cookie,
                      network::mojom::CookieChangeCause cause) override;

  Profile* profile_;
  mojo::Binding<network::mojom::CookieChangeListener> binding_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionCookieNotifier);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_COOKIE_NOTIFIER_H_
