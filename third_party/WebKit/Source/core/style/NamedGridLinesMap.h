// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NamedGridLinesMap_h
#define NamedGridLinesMap_h

#include "platform/wtf/HashMap.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

using NamedGridLinesMap = HashMap<String, Vector<size_t>>;

}  // namespace blink

#endif  // NamedGridLinesMap_h
