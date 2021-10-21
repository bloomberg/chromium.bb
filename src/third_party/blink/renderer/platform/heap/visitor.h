// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_VISITOR_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_VISITOR_H_

#include "third_party/blink/renderer/platform/heap/garbage_collected.h"
#include "v8/include/cppgc/visitor.h"

namespace blink {

using Visitor = cppgc::Visitor;

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_HEAP_VISITOR_H_
