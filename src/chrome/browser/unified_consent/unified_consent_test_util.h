// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_TEST_UTIL_H_
#define CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_TEST_UTIL_H_

#include <memory>

namespace content {
class BrowserContext;
}
class KeyedService;

// Helper function to be used with KeyedService::SetTestingFactory().
std::unique_ptr<KeyedService> BuildUnifiedConsentServiceForTesting(
    content::BrowserContext* context);

#endif  // CHROME_BROWSER_UNIFIED_CONSENT_UNIFIED_CONSENT_TEST_UTIL_H_
