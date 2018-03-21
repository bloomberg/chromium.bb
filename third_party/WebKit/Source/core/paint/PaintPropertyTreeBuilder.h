// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintPropertyTreeBuilder_h
#define PaintPropertyTreeBuilder_h

#include "base/memory/scoped_refptr.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/wtf/Optional.h"

namespace blink {

class FragmentData;
class LayoutObject;
class LocalFrameView;
class PaintLayer;

// The context for PaintPropertyTreeBuilder.
// It's responsible for bookkeeping tree state in other order, for example, the
// most recent position container seen.
struct PaintPropertyTreeBuilderFragmentContext {
  USING_FAST_MALLOC(PaintPropertyTreeBuilderFragmentContext);

 public:
  // Initializes all property tree nodes to the roots.
  PaintPropertyTreeBuilderFragmentContext();

  // State that propagates on the containing block chain (and so is adjusted
  // when an absolute or fixed position object is encountered).
  struct ContainingBlockContext {
    // The combination of a transform and paint offset describes a linear space.
    // When a layout object recur to its children, the main context is expected
    // to refer the object's border box, then the callee will derive its own
    // border box by translating the space with its own layout location.
    const TransformPaintPropertyNode* transform = nullptr;
    // Corresponds to LayoutObject::PaintOffset, which does not include
    // fragmentation offsets. See FragmentContext for the fragmented version.
    LayoutPoint paint_offset;
    // The PaintLayer corresponding to the origin of |paint_offset|.
    const LayoutObject* paint_offset_root = nullptr;
    // Whether newly created children should flatten their inherited transform
    // (equivalently, draw into the plane of their parent). Should generally
    // be updated whenever |transform| is; flattening only needs to happen
    // to immediate children.
    bool should_flatten_inherited_transform = false;

    // True if making filter a containing block for all descendants would
    // change this context to a different one. This is used only for
    // use-counting.
    bool containing_block_changed_under_filter = false;

    // Rendering context for 3D sorting. See
    // TransformPaintPropertyNode::renderingContextId.
    unsigned rendering_context_id = 0;
    // The clip node describes the accumulated raster clip for the current
    // subtree.  Note that the computed raster region in canvas space for a clip
    // node is independent from the transform and paint offset above. Also the
    // actual raster region may be affected by layerization and occlusion
    // tracking.
    const ClipPaintPropertyNode* clip = nullptr;
    // The scroll node contains information for scrolling such as the parent
    // scroll space, the extent that can be scrolled, etc. Because scroll nodes
    // reference a scroll offset transform, scroll nodes should be updated if
    // the transform tree changes.
    const ScrollPaintPropertyNode* scroll = nullptr;

    // True if any fixed-position children within this context are fixed to the
    // root of the FrameView (and hence above its scroll).
    bool fixed_position_children_fixed_to_root = false;
  };

  ContainingBlockContext current;

  // Separate context for out-of-flow positioned and fixed positioned elements
  // are needed because they don't use DOM parent as their containing block.
  // These additional contexts normally pass through untouched, and are only
  // copied from the main context when the current element serves as the
  // containing block of corresponding positioned descendants.  Overflow clips
  // are also inherited by containing block tree instead of DOM tree, thus they
  // are included in the additional context too.
  ContainingBlockContext absolute_position;

  ContainingBlockContext fixed_position;

  // This is the same as current.paintOffset except when a floating object has
  // non-block ancestors under its containing block. Paint offsets of the
  // non-block ancestors should not be accumulated for the floating object.
  LayoutPoint paint_offset_for_float;

  // The effect hierarchy is applied by the stacking context tree. It is
  // guaranteed that every DOM descendant is also a stacking context descendant.
  // Therefore, we don't need extra bookkeeping for effect nodes and can
  // generate the effect tree from a DOM-order traversal.
  const EffectPaintPropertyNode* current_effect;

  // If the object is a flow thread, this records the clip rect for this
  // fragment.
  Optional<LayoutRect> fragment_clip;

  // If the object is fragmented, this records the logical top of this fragment
  // in the flow thread.
  LayoutUnit logical_top_in_flow_thread;

  // A repeating object paints at multiple places in the flow thread, once in
  // each fragment. The repeated paintings need to add an adjustment to the
  // calculated paint offset to paint at the desired place.
  LayoutSize repeating_paint_offset_adjustment;
};

struct PaintPropertyTreeBuilderContext {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  PaintPropertyTreeBuilderContext() = default;

  Vector<PaintPropertyTreeBuilderFragmentContext, 1> fragments;
  const LayoutObject* container_for_absolute_position = nullptr;
  const LayoutObject* container_for_fixed_position = nullptr;

  // True if a change has forced all properties in a subtree to be updated. This
  // can be set due to paint offset changes or when the structure of the
  // property tree changes (i.e., a node is added or removed).
  bool force_subtree_update = false;

  // Whether a clip paint property node appeared, disappeared, or changed
  // its clip since this variable was last set to false. This is used
  // to find out whether a clip changed since the last transform update.
  // Code ouside of this class resets clip_changed to false when transforms
  // change.
  bool clip_changed = false;

#if DCHECK_IS_ON()
  // When DCHECK_IS_ON() we create PaintPropertyTreeBuilderContext even if not
  // needed. See FindPaintOffsetAndVisualRectNeedingUpdate.h.
  bool is_actually_needed = true;
#endif

  PaintLayer* painting_layer = nullptr;

  // In a fragmented context, some objects (e.g. repeating table headers and
  // footers, fixed-position objects in paged media, and their descendants in
  // paint order) repeatly paint in all fragments after the fragment where the
  // object first appears.
  bool is_repeating_in_fragments = false;

  // True if the current subtree is underneath a LayoutSVGHiddenContainer
  // ancestor.
  bool has_svg_hidden_container_ancestor = false;

  // The physical bounding box of all appearances of the repeating object
  // in the flow thread.
  LayoutRect repeating_bounding_box_in_flow_thread;
};

// |FrameViewPaintPropertyTreeBuilder| and |ObjectPaintPropertyTreeBuilder|
// create paint property tree nodes for special things in the layout tree.
// Special things include but not limit to: overflow clip, transform, fixed-pos,
// animation, mask, filter, ... etc.
// It expects to be invoked for each layout tree node in DOM order during
// InPrePaint phase.

class FrameViewPaintPropertyTreeBuilder {
 public:
  // Update the paint properties for a frame view and ensure the context is up
  // to date.
  static void Update(LocalFrameView&, PaintPropertyTreeBuilderContext&);
};

class ObjectPaintPropertyTreeBuilder {
 public:
  ObjectPaintPropertyTreeBuilder(const LayoutObject& object,
                                 PaintPropertyTreeBuilderContext& context)
      : object_(object), context_(context) {}

  // Update the paint properties that affect this object (e.g., properties like
  // paint offset translation) and ensure the context is up to date. Also
  // handles updating the object's paintOffset.
  // Returns true if any paint property of the object has changed.
  bool UpdateForSelf();

  // Update the paint properties that affect children of this object (e.g.,
  // scroll offset transform) and ensure the context is up to date.
  // Returns true if any paint property of the object has changed.
  bool UpdateForChildren();

 private:
  ALWAYS_INLINE void InitFragmentPaintProperties(
      FragmentData&,
      bool needs_paint_properties,
      const LayoutPoint& pagination_offset = LayoutPoint(),
      LayoutUnit logical_top_in_flow_thread = LayoutUnit());
  ALWAYS_INLINE void InitSingleFragmentFromParent(bool needs_paint_properties);
  ALWAYS_INLINE bool ObjectTypeMightNeedPaintProperties() const;
  ALWAYS_INLINE void UpdateCompositedLayerPaginationOffset();
  ALWAYS_INLINE bool NeedsFragmentation() const;
  ALWAYS_INLINE PaintPropertyTreeBuilderFragmentContext
  ContextForFragment(const Optional<LayoutRect>& fragment_clip,
                     LayoutUnit logical_top_in_flow_thread) const;
  ALWAYS_INLINE void CreateFragmentContexts(bool needs_paint_properties);
  // Returns whether ObjectPaintProperties were allocated or deleted.
  ALWAYS_INLINE bool UpdateFragments();
  ALWAYS_INLINE void UpdatePaintingLayer();
  ALWAYS_INLINE void UpdateRepeatingPaintOffsetAdjustment();
  ALWAYS_INLINE void UpdateRepeatingTableHeaderPaintOffsetAdjustment();
  ALWAYS_INLINE void UpdateRepeatingTableFooterPaintOffsetAdjustment();

  const LayoutObject& object_;
  PaintPropertyTreeBuilderContext& context_;
};

}  // namespace blink

#endif  // PaintPropertyTreeBuilder_h
