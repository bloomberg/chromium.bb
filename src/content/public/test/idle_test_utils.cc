// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/test/idle_test_utils.h"

#include "content/browser/idle/idle_polling_service.h"
#include "content/public/browser/idle_time_provider.h"

namespace content {

ScopedIdleProviderForTest::ScopedIdleProviderForTest(
    std::unique_ptr<IdleTimeProvider> provider) {
  IdlePollingService::GetInstance()->SetProviderForTest(std::move(provider));
}

ScopedIdleProviderForTest::~ScopedIdleProviderForTest() {
  IdlePollingService::GetInstance()->SetProviderForTest(nullptr);
}

}  // namespace content
