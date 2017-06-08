// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OrderedNamedGridLines_h
#define OrderedNamedGridLines_h

#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashTraits.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

using OrderedNamedGridLines =
    HashMap<size_t,
            Vector<String>,
            WTF::IntHash<size_t>,
            WTF::UnsignedWithZeroKeyHashTraits<size_t>>;

}  // namespace blink

#endif  // OrderedNamedGridLines_h
