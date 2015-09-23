// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositedDisplayList_h
#define CompositedDisplayList_h

#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DisplayItemTransformTree.h"

namespace blink {

// The result of running the simple layer compositing algorithm. See:
// https://docs.google.com/document/d/1qF7wpO_lhuxUO6YXKZ3CJuXi0grcb5gKZJBBgnoTd0k/view
class CompositedDisplayList {
public:
    // TODO(pdr): Also add our SimpleLayers here.
    // TODO(pdr): Add the additional property trees (e.g., clip, scroll, etc).
    OwnPtr<const DisplayItemTransformTree> transformTree;
};

} // namespace blink

#endif // CompositedDisplayList_h
