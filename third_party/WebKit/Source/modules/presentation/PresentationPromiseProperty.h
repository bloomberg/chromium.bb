// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationPromiseProperty_h
#define PresentationPromiseProperty_h

#include "bindings/core/v8/ScriptPromiseProperty.h"
#include "core/dom/DOMException.h"

namespace blink {

class PresentationAvailability;
class PresentationRequest;

using PresentationAvailabilityProperty =
    ScriptPromiseProperty<Member<PresentationRequest>,
                          Member<PresentationAvailability>,
                          Member<DOMException>>;

}  // namespace blink

#endif  // PresentationPromiseProperty_h
