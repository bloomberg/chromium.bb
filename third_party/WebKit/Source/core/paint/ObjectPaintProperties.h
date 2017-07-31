// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPaintProperties_h
#define ObjectPaintProperties_h

#include <memory>
#include "core/CoreExport.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/RefPtr.h"

namespace blink {

// This class stores the paint property nodes created by a LayoutObject. The
// object owns each of the property nodes directly and RefPtrs are only used to
// harden against use-after-free bugs. These paint properties are built/updated
// by PaintPropertyTreeBuilder during the PrePaint lifecycle step.
//
// [update & clear implementation note] This class has Update[property](...) and
// Clear[property]() helper functions for efficiently creating and updating
// properties. The update functions returns a 3-state result to indicate whether
// the value or the existence of the node has changed. They use a create-or-
// update pattern of re-using existing properties for efficiency:
// 1. It avoids extra allocations.
// 2. It preserves existing child->parent pointers.
// The clear functions return true if an existing node is removed. Property
// nodes store parent pointers but not child pointers and these return values
// are important for catching property tree structure changes which require
// updating descendant's parent pointers.
class CORE_EXPORT ObjectPaintProperties {
  WTF_MAKE_NONCOPYABLE(ObjectPaintProperties);
  USING_FAST_MALLOC(ObjectPaintProperties);

 public:
  static std::unique_ptr<ObjectPaintProperties> Create() {
    return WTF::WrapUnique(new ObjectPaintProperties());
  }

  // The hierarchy of the transform subtree created by a LayoutObject is as
  // follows:
  // [ paintOffsetTranslation ]           Normally paint offset is accumulated
  // |                                    without creating a node until we see,
  // |                                    for example, transform or
  // |                                    position:fixed.
  // +---[ transform ]                    The space created by CSS transform.
  //     |                                This is the local border box space.
  //     +---[ perspective ]              The space created by CSS perspective.
  //     |   +---[ svgLocalToBorderBoxTransform ] Additional transform for
  //                                      children of the outermost root SVG.
  //     |              OR                (SVG does not support scrolling.)
  //     |   +---[ scrollTranslation ]    The space created by overflow clip.
  //     +---[ scrollbarPaintOffset ]     TODO(trchen): Remove this once we bake
  //                                      the paint offset into frameRect.  This
  //                                      is equivalent to the local border box
  //                                      space above, with pixel snapped paint
  //                                      offset baked in. It is really
  //                                      redundant, but it is a pain to teach
  //                                      scrollbars to paint with an offset.
  const TransformPaintPropertyNode* PaintOffsetTranslation() const {
    return paint_offset_translation_.Get();
  }
  const TransformPaintPropertyNode* Transform() const {
    return transform_.Get();
  }
  const TransformPaintPropertyNode* Perspective() const {
    return perspective_.Get();
  }
  const TransformPaintPropertyNode* SvgLocalToBorderBoxTransform() const {
    return svg_local_to_border_box_transform_.Get();
  }
  const TransformPaintPropertyNode* ScrollTranslation() const {
    return scroll_translation_.Get();
  }
  const TransformPaintPropertyNode* ScrollbarPaintOffset() const {
    return scrollbar_paint_offset_.Get();
  }

  // The hierarchy of the effect subtree created by a LayoutObject is as
  // follows:
  // [ effect ]
  // |   Isolated group to apply various CSS effects, including opacity,
  // |   mix-blend-mode, and for isolation if a mask needs to be applied or
  // |   backdrop-dependent children are present.
  // +-[ filter ]
  // |     Isolated group for CSS filter.
  // +-[ mask ]
  //       Isolated group for painting the CSS mask. This node will have
  //       SkBlendMode::kDstIn and shall paint last, i.e. after masked contents.
  const EffectPaintPropertyNode* Effect() const { return effect_.Get(); }
  const EffectPaintPropertyNode* Filter() const { return filter_.Get(); }
  const EffectPaintPropertyNode* Mask() const { return mask_.Get(); }

  // The hierarchy of the clip subtree created by a LayoutObject is as follows:
  // [ mask clip ]
  // |   Clip created by CSS mask. It serves two purposes:
  // |   1. Cull painting of the masked subtree. Because anything outside of
  // |      the mask is never visible, it is pointless to paint them.
  // |   2. Raster clip of the masked subtree. Because the mask implemented
  // |      as SkBlendMode::kDstIn, pixels outside of mask's bound will be
  // |      intact when they shall be masked out. This clip ensures no pixels
  // |      leak out.
  // +-[ css clip ]
  //   |   Clip created by CSS clip. CSS clip applies to all descendants, this
  //   |   node only applies to containing block descendants. For descendants
  //   |   not contained by this object, use [ css clip fixed position ].
  //   +-[ inner border radius clip]
  //     |   Clip created by a rounded border with overflow clip. This clip is
  //     |   not inset by scrollbars.
  //     +-[ overflow clip ]
  //           Clip created by overflow clip and is inset by the scrollbar.
  // [ css clip fixed position ]
  //     Clip created by CSS clip. Only exists if the current clip includes
  //     some clip that doesn't apply to our fixed position descendants.
  const ClipPaintPropertyNode* MaskClip() const { return mask_clip_.Get(); }
  const ClipPaintPropertyNode* CssClip() const { return css_clip_.Get(); }
  const ClipPaintPropertyNode* CssClipFixedPosition() const {
    return css_clip_fixed_position_.Get();
  }
  const ClipPaintPropertyNode* InnerBorderRadiusClip() const {
    return inner_border_radius_clip_.Get();
  }
  const ClipPaintPropertyNode* OverflowClip() const {
    return overflow_clip_.Get();
  }

  // This is the complete set of property nodes that can be used to paint the
  // contents of this object. It is similar to the local border box properties
  // but also includes properties (e.g., overflow clip, scroll translation) that
  // apply to an object's contents.
  static std::unique_ptr<PropertyTreeState> ContentsProperties(
      PropertyTreeState* local_border_box_properties,
      ObjectPaintProperties*);

  // The following clear* functions return true if the property tree structure
  // changes (an existing node was deleted), and false otherwise. See the
  // class-level comment ("update & clear implementation note") for details
  // about why this is needed for efficient updates.
  bool ClearPaintOffsetTranslation() {
    return Clear(paint_offset_translation_);
  }
  bool ClearTransform() { return Clear(transform_); }
  bool ClearEffect() { return Clear(effect_); }
  bool ClearFilter() { return Clear(filter_); }
  bool ClearMask() { return Clear(mask_); }
  bool ClearMaskClip() { return Clear(mask_clip_); }
  bool ClearCssClip() { return Clear(css_clip_); }
  bool ClearCssClipFixedPosition() { return Clear(css_clip_fixed_position_); }
  bool ClearInnerBorderRadiusClip() { return Clear(inner_border_radius_clip_); }
  bool ClearOverflowClip() { return Clear(overflow_clip_); }
  bool ClearPerspective() { return Clear(perspective_); }
  bool ClearSvgLocalToBorderBoxTransform() {
    return Clear(svg_local_to_border_box_transform_);
  }
  bool ClearScrollTranslation() { return Clear(scroll_translation_); }
  bool ClearScrollbarPaintOffset() { return Clear(scrollbar_paint_offset_); }

  class UpdateResult {
   public:
    bool ValueChanged() const { return result_ == kValueChanged; }
    bool NewNodeCreated() const { return result_ == kNewNodeCreated; }

   private:
    friend class ObjectPaintProperties;
    enum Result { kUnchanged, kValueChanged, kNewNodeCreated };
    UpdateResult(Result r) : result_(r) {}
    Result result_;
  };

  template <typename... Args>
  UpdateResult UpdatePaintOffsetTranslation(Args&&... args) {
    return Update(paint_offset_translation_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateTransform(Args&&... args) {
    return Update(transform_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdatePerspective(Args&&... args) {
    return Update(perspective_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateSvgLocalToBorderBoxTransform(Args&&... args) {
    DCHECK(!ScrollTranslation()) << "SVG elements cannot scroll so there "
                                    "should never be both a scroll translation "
                                    "and an SVG local to border box transform.";
    return Update(svg_local_to_border_box_transform_,
                  std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateScrollTranslation(Args&&... args) {
    DCHECK(!SvgLocalToBorderBoxTransform())
        << "SVG elements cannot scroll so there should never be both a scroll "
           "translation and an SVG local to border box transform.";
    if (scroll_translation_) {
      return scroll_translation_->UpdateScrollTranslation(
                 std::forward<Args>(args)...)
                 ? UpdateResult::kValueChanged
                 : UpdateResult::kUnchanged;
    }
    scroll_translation_ = TransformPaintPropertyNode::CreateScrollTranslation(
        std::forward<Args>(args)...);
    return UpdateResult::kNewNodeCreated;
  }
  template <typename... Args>
  UpdateResult UpdateScrollbarPaintOffset(Args&&... args) {
    return Update(scrollbar_paint_offset_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateEffect(Args&&... args) {
    return Update(effect_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateFilter(Args&&... args) {
    return Update(filter_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateMask(Args&&... args) {
    return Update(mask_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateMaskClip(Args&&... args) {
    return Update(mask_clip_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateCssClip(Args&&... args) {
    return Update(css_clip_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateCssClipFixedPosition(Args&&... args) {
    return Update(css_clip_fixed_position_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateInnerBorderRadiusClip(Args&&... args) {
    return Update(inner_border_radius_clip_, std::forward<Args>(args)...);
  }
  template <typename... Args>
  UpdateResult UpdateOverflowClip(Args&&... args) {
    return Update(overflow_clip_, std::forward<Args>(args)...);
  }

#if DCHECK_IS_ON()
  // Used by FindPropertiesNeedingUpdate.h for recording the current properties.
  std::unique_ptr<ObjectPaintProperties> Clone() const {
    std::unique_ptr<ObjectPaintProperties> cloned = Create();
    if (paint_offset_translation_)
      cloned->paint_offset_translation_ = paint_offset_translation_->Clone();
    if (transform_)
      cloned->transform_ = transform_->Clone();
    if (effect_)
      cloned->effect_ = effect_->Clone();
    if (filter_)
      cloned->filter_ = filter_->Clone();
    if (mask_)
      cloned->mask_ = mask_->Clone();
    if (mask_clip_)
      cloned->mask_clip_ = mask_clip_->Clone();
    if (css_clip_)
      cloned->css_clip_ = css_clip_->Clone();
    if (css_clip_fixed_position_)
      cloned->css_clip_fixed_position_ = css_clip_fixed_position_->Clone();
    if (inner_border_radius_clip_)
      cloned->inner_border_radius_clip_ = inner_border_radius_clip_->Clone();
    if (overflow_clip_)
      cloned->overflow_clip_ = overflow_clip_->Clone();
    if (perspective_)
      cloned->perspective_ = perspective_->Clone();
    if (svg_local_to_border_box_transform_) {
      cloned->svg_local_to_border_box_transform_ =
          svg_local_to_border_box_transform_->Clone();
    }
    if (scroll_translation_)
      cloned->scroll_translation_ = scroll_translation_->Clone();
    if (scrollbar_paint_offset_)
      cloned->scrollbar_paint_offset_ = scrollbar_paint_offset_->Clone();
    return cloned;
  }
#endif

 private:
  ObjectPaintProperties() {}

  // Return true if the property tree structure changes (an existing node was
  // deleted), and false otherwise. See the class-level comment ("update & clear
  // implementation note") for details about why this is needed for efficiency.
  template <typename PaintPropertyNode>
  bool Clear(RefPtr<PaintPropertyNode>& field) {
    if (field) {
      field = nullptr;
      return true;
    }
    return false;
  }

  // Return true if the property tree structure changes (a new node was
  // created), and false otherwise. See the class-level comment ("update & clear
  // implementation note") for details about why this is needed for efficiency.
  template <typename PaintPropertyNode, typename... Args>
  UpdateResult Update(RefPtr<PaintPropertyNode>& field, Args&&... args) {
    if (field) {
      return field->Update(std::forward<Args>(args)...)
                 ? UpdateResult::kValueChanged
                 : UpdateResult::kUnchanged;
    }
    field = PaintPropertyNode::Create(std::forward<Args>(args)...);
    return UpdateResult::kNewNodeCreated;
  }

  // ATTENTION! Make sure to keep FindPropertiesNeedingUpdate.h in sync when
  // new properites are added!
  RefPtr<TransformPaintPropertyNode> paint_offset_translation_;
  RefPtr<TransformPaintPropertyNode> transform_;
  RefPtr<EffectPaintPropertyNode> effect_;
  RefPtr<EffectPaintPropertyNode> filter_;
  RefPtr<EffectPaintPropertyNode> mask_;
  RefPtr<ClipPaintPropertyNode> mask_clip_;
  RefPtr<ClipPaintPropertyNode> css_clip_;
  RefPtr<ClipPaintPropertyNode> css_clip_fixed_position_;
  RefPtr<ClipPaintPropertyNode> inner_border_radius_clip_;
  RefPtr<ClipPaintPropertyNode> overflow_clip_;
  RefPtr<TransformPaintPropertyNode> perspective_;
  // TODO(pdr): Only LayoutSVGRoot needs this and it should be moved there.
  RefPtr<TransformPaintPropertyNode> svg_local_to_border_box_transform_;
  RefPtr<TransformPaintPropertyNode> scroll_translation_;
  RefPtr<TransformPaintPropertyNode> scrollbar_paint_offset_;
};

}  // namespace blink

#endif  // ObjectPaintProperties_h
