// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/NavigatorBluetooth.h"

#include "core/frame/Navigator.h"
#include "modules/bluetooth/Bluetooth.h"

namespace blink {

NavigatorBluetooth& NavigatorBluetooth::From(Navigator& navigator) {
  NavigatorBluetooth* supplement = static_cast<NavigatorBluetooth*>(
      Supplement<Navigator>::From(navigator, SupplementName()));
  if (!supplement) {
    supplement = new NavigatorBluetooth(navigator);
    ProvideTo(navigator, SupplementName(), supplement);
  }
  return *supplement;
}

Bluetooth* NavigatorBluetooth::bluetooth(Navigator& navigator) {
  return NavigatorBluetooth::From(navigator).bluetooth();
}

Bluetooth* NavigatorBluetooth::bluetooth() {
  if (!bluetooth_)
    bluetooth_ = Bluetooth::Create();
  return bluetooth_.Get();
}

void NavigatorBluetooth::Trace(blink::Visitor* visitor) {
  visitor->Trace(bluetooth_);
  Supplement<Navigator>::Trace(visitor);
}

NavigatorBluetooth::NavigatorBluetooth(Navigator& navigator)
    : Supplement<Navigator>(navigator) {}

const char* NavigatorBluetooth::SupplementName() {
  return "NavigatorBluetooth";
}

}  // namespace blink
