// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/federated/fake_federated_client.h"

#include "base/callback.h"

namespace chromeos {

FakeFederatedClient::FakeFederatedClient() = default;

FakeFederatedClient::~FakeFederatedClient() = default;

void FakeFederatedClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    base::OnceCallback<void(bool success)> result_callback) {
  const bool success = true;
  std::move(result_callback).Run(success);
}

}  // namespace chromeos
