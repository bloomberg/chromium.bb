// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/cups_proxy/fake_cups_proxy_client.h"

#include <utility>

#include "base/callback.h"

namespace chromeos {

FakeCupsProxyClient::FakeCupsProxyClient() = default;
FakeCupsProxyClient::~FakeCupsProxyClient() = default;

void FakeCupsProxyClient::BootstrapMojoConnection(
    base::ScopedFD fd,
    base::OnceCallback<void(bool success)> result_callback) {
  const bool success = true;
  std::move(result_callback).Run(success);
}

}  // namespace chromeos
