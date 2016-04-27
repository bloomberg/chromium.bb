// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CustomElement_h
#define CustomElement_h

#include "core/CoreExport.h"
#include "wtf/Allocator.h"
#include "wtf/text/AtomicString.h"

namespace blink {

class CORE_EXPORT CustomElement {
    STATIC_ONLY(CustomElement);
public:
    static bool isPotentialCustomElementName(const AtomicString& name);
};

} // namespace blink

#endif // CustomElement_h
