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
    PaintLayer* enclosing_composited_layer = nullptr;
    PaintLayer* last_overflow_clip_layer = nullptr;

    PaintLayer* clip_chain_parent_for_absolute = nullptr;
    PaintLayer* clip_chain_parent_for_fixed = nullptr;
    // These flags are set if we encountered a stacking context
    // that will make descendants to inherit more clip than desired,
    // so we have to setup an alternative clip parent instead.
    PaintLayer* escape_clip_to = nullptr;
    PaintLayer* escape_clip_to_for_absolute = nullptr;
    PaintLayer* escape_clip_to_for_fixed = nullptr;

    PaintLayer* scrolling_ancestor = nullptr;
    PaintLayer* scrolling_ancestor_for_absolute = nullptr;
    PaintLayer* scrolling_ancestor_for_fixed = nullptr;
    // These flags are set to true if a non-stacking context scroller
    // is encountered, so that a descendant element won't inherit scroll
    // translation from its compositing ancestor directly thus having to
    // setup an alternative scroll parent instead.
    bool needs_reparent_scroll = false;
    bool needs_reparent_scroll_for_absolute = false;
    bool needs_reparent_scroll_for_fixed = false;
  };

  void UpdateRecursive(PaintLayer*, UpdateType, AncestorInfo);
  void UpdateAncestorDependentCompositingInputs(PaintLayer*,
                                                const AncestorInfo&);

  LayoutGeometryMap geometry_map_;
  PaintLayer* root_layer_;
};

}  // namespace blink

#endif  // CompositingInputsUpdater_h
