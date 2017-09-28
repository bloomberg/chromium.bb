// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositorMutableProperties_h
#define CompositorMutableProperties_h

namespace blink {

struct CompositorMutableProperty {
  enum : uint32_t { kNone = 0 };
  enum : uint32_t { kOpacity = 1 << 0 };
  enum : uint32_t { kScrollLeft = 1 << 1 };
  enum : uint32_t { kScrollTop = 1 << 2 };
  enum : uint32_t { kTransform = 1 << 3 };
  enum : uint32_t { kTransformRelated = kTransform | kScrollLeft | kScrollTop };

  enum : int { kNumProperties = 4 };
};

}  // namespace blink

#endif  // CompositorMutableProperties_h
