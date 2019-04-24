// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_UI_DELEGATE_FACTORY_IMPL_H_
#define CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_UI_DELEGATE_FACTORY_IMPL_H_

#include "base/macros.h"
#include "extensions/browser/api/networking_private/networking_private_delegate_factory.h"

namespace extensions {

class NetworkingPrivateUIDelegateFactoryImpl
    : public NetworkingPrivateDelegateFactory::UIDelegateFactory {
 public:
  NetworkingPrivateUIDelegateFactoryImpl();
  ~NetworkingPrivateUIDelegateFactoryImpl() override;

  std::unique_ptr<NetworkingPrivateDelegate::UIDelegate> CreateDelegate()
      override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NetworkingPrivateUIDelegateFactoryImpl);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_NETWORKING_PRIVATE_NETWORKING_PRIVATE_UI_DELEGATE_FACTORY_IMPL_H_
