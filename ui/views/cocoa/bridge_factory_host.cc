// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/cocoa/bridge_factory_host.h"

#include "mojo/public/cpp/bindings/interface_request.h"

namespace views {

namespace {
// Start the ids at something far from zero to help in debugging.
uint64_t g_next_bridge_factory_host_id_ = 0x1000;
}  // namespace

BridgeFactoryHost::BridgeFactoryHost(
    views_bridge_mac::mojom::BridgeFactoryRequest* request)
    : host_id_(g_next_bridge_factory_host_id_++) {
  *request = mojo::MakeRequest(&bridge_factory_ptr_);
}

BridgeFactoryHost::~BridgeFactoryHost() {
  for (Observer& obs : observers_)
    obs.OnBridgeFactoryHostDestroying(this);
}

views_bridge_mac::mojom::BridgeFactory* BridgeFactoryHost::GetFactory() {
  return bridge_factory_ptr_.get();
}

void BridgeFactoryHost::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void BridgeFactoryHost::RemoveObserver(const Observer* observer) {
  observers_.RemoveObserver(observer);
}

}  // namespace views
