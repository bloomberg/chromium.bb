// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/linux/services/credentials.h"

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "sandbox/linux/tests/unit_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sandbox {

// Give dynamic tools a simple thing to test.
TEST(Credentials, CreateAndDestroy) {
  {
    Credentials cred1;
    (void) cred1;
  }
  scoped_ptr<Credentials> cred2(new Credentials);
}

SANDBOX_TEST(Credentials, DropAllCaps) {
  Credentials creds;
  creds.DropAllCapabilities();
  SANDBOX_ASSERT(!creds.HasAnyCapability());
}

SANDBOX_TEST(Credentials, GetCurrentCapString) {
  Credentials creds;
  creds.DropAllCapabilities();
  const char kNoCapabilityText[] = "=";
  SANDBOX_ASSERT(*creds.GetCurrentCapString() == kNoCapabilityText);
}

}  // namespace sandbox.
