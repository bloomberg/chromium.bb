// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PointerProperties_h
#define PointerProperties_h

namespace blink {

// Bit field values indicating available pointer types.
enum PointerType {
  kPointerTypeNone = 1 << 0,
  kPointerTypeCoarse = 1 << 1,
  kPointerTypeFine = 1 << 2
};

// Bit field values indicating available hover types.
enum HoverType { kHoverTypeNone = 1 << 0, kHoverTypeHover = 1 << 1 };
}

#endif
