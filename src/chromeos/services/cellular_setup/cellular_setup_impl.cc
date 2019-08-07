// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/cellular_setup/cellular_setup_impl.h"

#include "base/memory/ptr_util.h"
#include "base/no_destructor.h"

namespace chromeos {

namespace cellular_setup {

namespace {

CellularSetupImpl::Factory* g_test_factory = nullptr;

}  // namespace

// static
std::unique_ptr<CellularSetupBase> CellularSetupImpl::Factory::Create() {
  if (g_test_factory)
    return g_test_factory->BuildInstance();

  return base::WrapUnique(new CellularSetupImpl());
}

// static
void CellularSetupImpl::Factory::SetFactoryForTesting(Factory* test_factory) {
  g_test_factory = test_factory;
}

CellularSetupImpl::Factory::~Factory() = default;

CellularSetupImpl::CellularSetupImpl() = default;

CellularSetupImpl::~CellularSetupImpl() = default;

void CellularSetupImpl::StartActivation(
    mojom::ActivationDelegatePtr delegate,
    StartActivationCallback callback) {
  // TODO(khorimoto): Actually return a CarrierPortalObserver instead of
  // passing null.
  std::move(callback).Run(nullptr /* observer */);
}

}  // namespace cellular_setup

}  // namespace chromeos
