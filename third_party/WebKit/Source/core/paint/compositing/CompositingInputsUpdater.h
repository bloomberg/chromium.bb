// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingInputsUpdater_h
#define CompositingInputsUpdater_h

#include "core/layout/LayoutGeometryMap.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class PaintLayer;

class CompositingInputsUpdater {
  STACK_ALLOCATED();

 public:
  explicit CompositingInputsUpdater(PaintLayer* root_layer);
  ~CompositingInputsUpdater();

  void Update();

#if DCHECK_IS_ON()
  static void AssertNeedsCompositingInputsUpdateBitsCleared(PaintLayer*);
#endif

 private:
  enum UpdateType {
    kDoNotForceUpdate,
    kForceUpdate,
  };

  struct AncestorInfo {
    AncestorInfo()
        : ancestor_stacking_context(nullptr),
          enclosing_composited_layer(nullptr),
          last_overflow_clip_layer(nullptr),
          last_scrolling_ancestor(nullptr),
          has_ancestor_with_clip_related_property(false),
          has_ancestor_with_clip_path(false) {}

    PaintLayer* ancestor_stacking_context;
    PaintLayer* enclosing_composited_layer;
    PaintLayer* last_overflow_clip_layer;
    // Notice that lastScrollingAncestor isn't the same thing as
    // ancestorScrollingLayer. The former is just the nearest scrolling
    // along the PaintLayer::parent() chain. The latter is the layer that
    // actually controls the scrolling of this layer, which we find on the
    // containing block chain.
    PaintLayer* last_scrolling_ancestor;
    bool has_ancestor_with_clip_related_property;
    bool has_ancestor_with_clip_path;
  };

  void UpdateRecursive(PaintLayer*, UpdateType, AncestorInfo);

  LayoutGeometryMap geometry_map_;
  PaintLayer* root_layer_;
};

}  // namespace blink

#endif  // CompositingInputsUpdater_h
