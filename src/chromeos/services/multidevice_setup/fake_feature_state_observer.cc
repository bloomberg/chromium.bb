// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/multidevice_setup/fake_feature_state_observer.h"

namespace chromeos {

namespace multidevice_setup {

FakeFeatureStateObserver::FakeFeatureStateObserver() = default;

FakeFeatureStateObserver::~FakeFeatureStateObserver() = default;

mojom::FeatureStateObserverPtr
FakeFeatureStateObserver::GenerateInterfacePtr() {
  mojom::FeatureStateObserverPtr interface_ptr;
  bindings_.AddBinding(this, mojo::MakeRequest(&interface_ptr));
  return interface_ptr;
}

void FakeFeatureStateObserver::OnFeatureStatesChanged(
    const base::flat_map<mojom::Feature, mojom::FeatureState>&
        feature_states_map) {
  feature_state_updates_.emplace_back(feature_states_map);
}

}  // namespace multidevice_setup

}  // namespace chromeos
