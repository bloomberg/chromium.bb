// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/features.h"

namespace blink {
namespace features {

// Enable LayoutNG.
const base::Feature kLayoutNG{"LayoutNG", base::FEATURE_DISABLED_BY_DEFAULT};

// Enable mojo Blob URL interface and better blob URL lifetime management.
// Can be enabled independently of NetworkService.
const base::Feature kMojoBlobURLs{"MojoBlobURLs",
                                  base::FEATURE_DISABLED_BY_DEFAULT};

// Enable new service worker glue for NetworkService. Can be
// enabled independently of NetworkService.
const base::Feature kServiceWorkerServicification{
    "ServiceWorkerServicification", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace blink
