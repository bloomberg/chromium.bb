// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebFeature_h
#define WebFeature_h

// Including the actual file that defines the WebFeature enum, like we do here,
// is heavy on the compiler. Those who do not need the definition, but could do
// with just a forward-declaration, should include WebFeatureForward.h instead.

#include "public/platform/web_feature.mojom-blink.h"

namespace blink {
using WebFeature = mojom::WebFeature;
}  // namespace blink

#endif  // WebFeature_h
