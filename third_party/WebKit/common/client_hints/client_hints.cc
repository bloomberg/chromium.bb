// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/WebKit/public/common/client_hints/client_hints.h"

#include "base/macros.h"

namespace blink {

const char* const kClientHintsHeaderMapping[] = {"device-memory", "dpr",
                                                 "width", "viewport-width"};

const size_t kClientHintsHeaderMappingCount =
    arraysize(kClientHintsHeaderMapping);

}  // namespace blink
