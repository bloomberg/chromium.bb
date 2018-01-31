// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationError_h
#define PresentationError_h

#include "core/dom/DOMException.h"
#include "public/platform/modules/presentation/presentation.mojom-blink.h"

namespace blink {

// Creates a DOMException using the given PresentationError.
DOMException* CreatePresentationError(const mojom::blink::PresentationError&);

}  // namespace blink

#endif  // PresentationError_h
