// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/public/cpp/fake_activation_delegate.h"

namespace chromeos {

namespace cellular_setup {

FakeActivationDelegate::FakeActivationDelegate() = default;

FakeActivationDelegate::~FakeActivationDelegate() = default;

mojom::ActivationDelegatePtr FakeActivationDelegate::GenerateInterfacePtr() {
  mojom::ActivationDelegatePtr interface_ptr;
  bindings_.AddBinding(this, mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void FakeActivationDelegate::DisconnectBindings() {
  bindings_.CloseAllBindings();
}

void FakeActivationDelegate::OnActivationStarted(
    mojom::CellularMetadataPtr cellular_metadata) {
  cellular_metadata_list_.push_back(std::move(cellular_metadata));
}

void FakeActivationDelegate::OnActivationFinished(
    mojom::ActivationResult activation_result) {
  activation_results_.push_back(activation_result);
}

}  // namespace cellular_setup

}  // namespace chromeos
