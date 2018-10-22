// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/captive_portal/captive_portal_types.h"

#include "base/logging.h"
#include "base/macros.h"

namespace captive_portal {

namespace {

const char* const kCaptivePortalResultNames[] = {
  "InternetConnected",
  "NoResponse",
  "BehindCaptivePortal",
  "NumCaptivePortalResults",
};
static_assert(arraysize(kCaptivePortalResultNames) == RESULT_COUNT + 1,
              "kCaptivePortalResultNames should have "
              "RESULT_COUNT + 1 elements");

}  // namespace


std::string CaptivePortalResultToString(CaptivePortalResult result) {
  DCHECK_GE(result, 0);
  DCHECK_LT(static_cast<unsigned int>(result),
            arraysize(kCaptivePortalResultNames));
  return kCaptivePortalResultNames[result];
}

}  // namespace captive_portal
