// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_provider.h"

#include "base/logging.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IOSChromeScopedTestingChromeBrowserProvider::
    IOSChromeScopedTestingChromeBrowserProvider(
        std::unique_ptr<ios::ChromeBrowserProvider> chrome_browser_provider)
    : chrome_browser_provider_(std::move(chrome_browser_provider)),
      original_chrome_browser_provider_(ios::GetChromeBrowserProvider()) {
  ios::SetChromeBrowserProvider(chrome_browser_provider_.get());
}

IOSChromeScopedTestingChromeBrowserProvider::
    ~IOSChromeScopedTestingChromeBrowserProvider() {
  DCHECK_EQ(chrome_browser_provider_.get(), ios::GetChromeBrowserProvider());
  ios::SetChromeBrowserProvider(original_chrome_browser_provider_);
}
