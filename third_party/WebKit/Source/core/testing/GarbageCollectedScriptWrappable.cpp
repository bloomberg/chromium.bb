// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/testing/GarbageCollectedScriptWrappable.h"

namespace blink {

GarbageCollectedScriptWrappable::GarbageCollectedScriptWrappable(
    const String& string)
    : string_(string) {}

GarbageCollectedScriptWrappable::~GarbageCollectedScriptWrappable() = default;

}  // namespace blink
