// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EventUtil_h
#define EventUtil_h

#include "core/event_type_names.h"

namespace blink {

namespace EventUtil {

bool IsPointerEventType(const AtomicString& event_type);

bool IsDOMMutationEventType(const AtomicString& event_type);

}  // namespace eventUtil

}  // namespace blink

#endif  // EventUtil_h
