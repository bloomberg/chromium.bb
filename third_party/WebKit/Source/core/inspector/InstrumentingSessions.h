// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InstrumentingSessions_h
#define InstrumentingSessions_h

#include "platform/heap/Handle.h"
#include "wtf/HashSet.h"

namespace blink {

class InspectorSession;
using InstrumentingSessions = HeapHashSet<Member<InspectorSession>>;

} // namespace blink

#endif // !defined(InstrumentingSessions_h)
