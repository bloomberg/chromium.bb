// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_TREE_WALK_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_TREE_WALK_H_

#include "base/dcheck_is_on.h"
#include "third_party/blink/renderer/core/paint/paint_invalidator.h"
#include "third_party/blink/renderer/core/paint/paint_property_tree_builder.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class LayoutObject;
class LocalFrameView;
class NGFragmentChildIterator;

// This class walks the whole layout tree, beginning from the root
// LocalFrameView, across frame boundaries. Helper classes are called for each
// tree node to perform actual actions.  It expects to be invoked in InPrePaint
// phase.
class CORE_EXPORT PrePaintTreeWalk final {
  STACK_ALLOCATED();

 public:
  PrePaintTreeWalk() = default;
  void WalkTree(LocalFrameView& root_frame);

  static bool ObjectRequiresPrePaint(const LayoutObject&);
  static bool ObjectRequiresTreeBuilderContext(const LayoutObject&);

  struct PrePaintTreeWalkContext {
    STACK_ALLOCATED();

   public:
    PrePaintTreeWalkContext() {
      tree_builder_context.emplace();
    }
    PrePaintTreeWalkContext(const PrePaintTreeWalkContext& parent_context,
                            bool needs_tree_builder_context)
        : paint_invalidator_context(parent_context.paint_invalidator_context),
          ancestor_scroll_container_paint_layer(
              parent_context.ancestor_scroll_container_paint_layer),
          inside_blocking_touch_event_handler(
              parent_context.inside_blocking_touch_event_handler),
          effective_allowed_touch_action_changed(
              parent_context.effective_allowed_touch_action_changed),
          inside_blocking_wheel_event_handler(
              parent_context.inside_blocking_wheel_event_handler),
          blocking_wheel_event_handler_changed(
              parent_context.blocking_wheel_event_handler_changed),
          clip_changed(parent_context.clip_changed),
          paint_invalidation_container(
              parent_context.paint_invalidation_container),
          paint_invalidation_container_for_stacked_contents(
              parent_context
                  .paint_invalidation_container_for_stacked_contents) {
      if (needs_tree_builder_context || DCHECK_IS_ON()) {
        DCHECK(parent_context.tree_builder_context);
        tree_builder_context.emplace(*parent_context.tree_builder_context);
      }
#if DCHECK_IS_ON()
      if (needs_tree_builder_context)
        DCHECK(parent_context.tree_builder_context->is_actually_needed);
      tree_builder_context->is_actually_needed = needs_tree_builder_context;
#endif
    }

    absl::optional<PaintPropertyTreeBuilderContext> tree_builder_context;

    PaintInvalidatorContext paint_invalidator_context;

    bool NeedsTreeBuilderContext() const {
#if DCHECK_IS_ON()
      DCHECK(tree_builder_context);
      return tree_builder_context->is_actually_needed;
#else
      return tree_builder_context.has_value();
#endif
    }

    // The ancestor in the PaintLayer tree which is a scroll container. Note
    // that it is tree ancestor, not containing block or stacking ancestor.
    PaintLayer* ancestor_scroll_container_paint_layer = nullptr;

    // Whether there is a blocking touch event handler on any ancestor.
    bool inside_blocking_touch_event_handler = false;

    // When the effective allowed touch action changes on an ancestor, the
    // entire subtree may need to update.
    bool effective_allowed_touch_action_changed = false;

    // Whether there is a blocking wheel event handler on any ancestor.
    bool inside_blocking_wheel_event_handler = false;

    // When the blocking wheel event handlers change on an ancestor, the entire
    // subtree may need to update.
    bool blocking_wheel_event_handler_changed = false;

    // This is set to true once we see tree_builder_context->clip_changed is
    // true. It will be propagated to descendant contexts even if we don't
    // create tree_builder_context. Used only when CullRectUpdate is not
    // enabled.
    bool clip_changed = false;

    const LayoutBoxModelObject* paint_invalidation_container = nullptr;
    const LayoutBoxModelObject*
        paint_invalidation_container_for_stacked_contents = nullptr;
  };

  static bool ContextRequiresChildPrePaint(const PrePaintTreeWalkContext&);
  static bool ContextRequiresChildTreeBuilderContext(
      const PrePaintTreeWalkContext&);

#if DCHECK_IS_ON()
  void CheckTreeBuilderContextState(const LayoutObject&,
                                    const PrePaintTreeWalkContext&);
#endif

  void Walk(LocalFrameView&, const PrePaintTreeWalkContext& parent_context);

  // This is to minimize stack frame usage during recursion. Modern compilers
  // (MSVC in particular) can inline across compilation units, resulting in
  // very big stack frames. Splitting the heavy lifting to a separate function
  // makes sure the stack frame is freed prior to making a recursive call.
  // See https://crbug.com/781301 .
  NOINLINE void WalkInternal(const LayoutObject&,
                             PrePaintTreeWalkContext&,
                             const NGFragmentChildIterator*);
  void WalkNGChildren(const LayoutObject* parent,
                      PrePaintTreeWalkContext& parent_context,
                      NGFragmentChildIterator*);
  void WalkLegacyChildren(const LayoutObject&, PrePaintTreeWalkContext&);
  void WalkChildren(const LayoutObject*,
                    PrePaintTreeWalkContext&,
                    const NGFragmentChildIterator*);
  void Walk(const LayoutObject&,
            const PrePaintTreeWalkContext& parent_context,
            const NGFragmentChildIterator*);

  bool NeedsTreeBuilderContextUpdate(const LocalFrameView&,
                                     const PrePaintTreeWalkContext&);
  void UpdateAuxiliaryObjectProperties(const LayoutObject&,
                                       PrePaintTreeWalkContext&);
  // Updates |LayoutObject::InsideBlockingTouchEventHandler|. Also ensures
  // |PrePaintTreeWalkContext.effective_allowed_touch_action_changed| is set
  // which will ensure the subtree is updated too.
  void UpdateEffectiveAllowedTouchAction(const LayoutObject&,
                                         PrePaintTreeWalkContext&);
  // Updates |LayoutObject::InsideBlockingWheelEventHandler|. Also ensures
  // |PrePaintTreeWalkContext.blocking_wheel_event_handler_changed| is set
  // which will ensure the subtree is updated too.
  void UpdateBlockingWheelEventHandler(const LayoutObject&,
                                       PrePaintTreeWalkContext&);
  void InvalidatePaintForHitTesting(const LayoutObject&,
                                    PrePaintTreeWalkContext&);

  void UpdatePaintInvalidationContainer(const LayoutObject& object,
                                        const PaintLayer* painting_layer,
                                        PrePaintTreeWalkContext& context,
                                        bool is_ng_painting);

  PaintInvalidator paint_invalidator_;

  // TODO(https://crbug.com/841364): Remove is_wheel_event_regions_enabled
  // argument once kWheelEventRegions feature flag is removed.
  bool is_wheel_event_regions_enabled_ = false;

  bool needs_invalidate_chrome_client_ = false;

  FRIEND_TEST_ALL_PREFIXES(PrePaintTreeWalkTest, ClipRects);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PRE_PAINT_TREE_WALK_H_
