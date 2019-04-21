// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/fake_affiliated_invalidation_service_provider.h"

namespace policy {

FakeAffiliatedInvalidationServiceProvider::
FakeAffiliatedInvalidationServiceProvider() {
}

void FakeAffiliatedInvalidationServiceProvider::RegisterConsumer(
    Consumer* consumer) {
}

void FakeAffiliatedInvalidationServiceProvider::UnregisterConsumer(
    Consumer* consumer) {
}

void FakeAffiliatedInvalidationServiceProvider::Shutdown() {
}

}  // namespace policy
