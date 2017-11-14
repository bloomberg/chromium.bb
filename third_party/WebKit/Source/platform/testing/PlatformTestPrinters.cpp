// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>
#include "platform/loader/fetch/ResourceError.h"

// Pretty printer for gtest.
// Each corresponding declaration should be placed in the header file of
// the corresponding class (e.g. ResourceError.h) to avoid ODR violations,
// not in e.g. PlatformTestPrinters.h. See https://crbug.com/514330.

namespace blink {

std::ostream& operator<<(std::ostream& os, const ResourceError& error) {
  return os << ", ErrorCode = " << error.ErrorCode()
            << ", FailingURL = " << error.FailingURL()
            << ", LocalizedDescription = " << error.LocalizedDescription()
            << ", IsCancellation = " << error.IsCancellation()
            << ", IsAccessCheck = " << error.IsAccessCheck()
            << ", IsTimeout = " << error.IsTimeout()
            << ", HasCopyInCache = " << error.HasCopyInCache()
            << ", IsCacheMiss = " << error.IsCacheMiss();
}

}  // namespace blink
