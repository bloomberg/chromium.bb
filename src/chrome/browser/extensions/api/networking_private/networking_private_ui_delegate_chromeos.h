// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_UI_DELEGATE_CHROMEOS_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_UI_DELEGATE_CHROMEOS_H_

#include "base/macros.h"
#include "extensions/browser/api/networking_private/networking_private_delegate.h"

namespace chromeos {
namespace extensions {

// Chrome OS implementation of NetworkingPrivateDelegate::UIDelegate.
class NetworkingPrivateUIDelegateChromeOS
    : public ::extensions::NetworkingPrivateDelegate::UIDelegate {
 public:
  NetworkingPrivateUIDelegateChromeOS();
  ~NetworkingPrivateUIDelegateChromeOS() override;

  // NetworkingPrivateDelegate::UIDelegate
  void ShowAccountDetails(const std::string& guid) const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateUIDelegateChromeOS);
};

}  // namespace extensions
}  // namespace chromeos

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_UI_DELEGATE_CHROMEOS_H_
