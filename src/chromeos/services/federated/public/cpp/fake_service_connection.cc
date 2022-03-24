// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/federated/public/cpp/fake_service_connection.h"

namespace chromeos {
namespace federated {

FakeServiceConnectionImpl::FakeServiceConnectionImpl() = default;
FakeServiceConnectionImpl::~FakeServiceConnectionImpl() = default;

void FakeServiceConnectionImpl::BindReceiver(
    mojo::PendingReceiver<mojom::FederatedService> receiver) {
  Clone(std::move(receiver));
}

void FakeServiceConnectionImpl::Clone(
    mojo::PendingReceiver<mojom::FederatedService> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void FakeServiceConnectionImpl::ReportExample(const std::string& client_name,
                                              mojom::ExamplePtr example) {
  LOG(INFO) << "In FakeServiceConnectionImpl::ReportExample, does nothing";
  return;
}

}  // namespace federated
}  // namespace chromeos
