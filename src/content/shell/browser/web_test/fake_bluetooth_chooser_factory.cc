// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/web_test/fake_bluetooth_chooser_factory.h"

#include "content/shell/browser/web_test/fake_bluetooth_chooser.h"

namespace content {

FakeBluetoothChooserFactory::~FakeBluetoothChooserFactory() {}

void FakeBluetoothChooserFactory::CreateFakeBluetoothChooser(
    mojom::FakeBluetoothChooserRequest request,
    mojom::FakeBluetoothChooserClientAssociatedPtrInfo client_ptr_info) {
  DCHECK(!next_fake_bluetooth_chooser_);
  next_fake_bluetooth_chooser_ = std::make_unique<FakeBluetoothChooser>(
      std::move(request), std::move(client_ptr_info));
}

std::unique_ptr<FakeBluetoothChooser>
FakeBluetoothChooserFactory::GetNextFakeBluetoothChooser() {
  return std::move(next_fake_bluetooth_chooser_);
}

FakeBluetoothChooserFactory::FakeBluetoothChooserFactory(
    mojom::FakeBluetoothChooserFactoryRequest request)
    : binding_(this, std::move(request)) {}

}  // namespace content
