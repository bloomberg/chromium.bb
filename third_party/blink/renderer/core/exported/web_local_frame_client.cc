// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_local_frame_client.h"

#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"

namespace blink {

service_manager::InterfaceProvider*
WebLocalFrameClient::GetInterfaceProvider() {
  static service_manager::InterfaceProvider* interface_provider = []() {
    auto* interface_provider = new service_manager::InterfaceProvider();
    service_manager::mojom::InterfaceProviderPtr provider;
    mojo::MakeRequest(&provider);
    interface_provider->Bind(std::move(provider));
    return interface_provider;
  }();

  return interface_provider;
}

BrowserInterfaceBrokerProxy* WebLocalFrameClient::GetBrowserInterfaceBroker() {
  return &GetEmptyBrowserInterfaceBroker();
}
}  // namespace blink
