// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ObjectPaintProperties_h
#define ObjectPaintProperties_h

#include "core/CoreExport.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/graphics/paint/ClipPaintPropertyNode.h"
#include "platform/graphics/paint/EffectPaintPropertyNode.h"
#include "platform/graphics/paint/PaintChunkProperties.h"
#include "platform/graphics/paint/PropertyTreeState.h"
#include "platform/graphics/paint/ScrollPaintPropertyNode.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/PassRefPtr.h"
#include "wtf/PtrUtil.h"
#include "wtf/RefPtr.h"
#include <memory>

namespace blink {

// This class stores the paint property nodes associated with a LayoutObject.
// The object owns each of the property nodes directly set here (e.g, m_cssClip,
// m_paintOffsetTranslation, etc.) and RefPtrs are only used to harden against
// use-after-free bugs. These paint properties are built/updated by
// PaintPropertyTreeBuilder during the PrePaint lifecycle step.
//
// There are two groups of information stored on ObjectPaintProperties:
// 1. The set of property nodes created locally and owned by this LayoutObject.
// 2. The set of property nodes (inherited, or created locally) that can be used
//    along with LayoutObject::paintOffset to paint the border box of this
//    LayoutObject (see: localBorderBoxProperties).
//
// [update & clear implementation note] This class has update[property](...) and
// clear[property]() helper functions for efficiently creating and updating
// properties. These functions return true if the property tree structure
// changes (e.g., a node is added or removed), and false otherwise. Property
// nodes store parent pointers but not child pointers and these return values
// are important for catching property tree structure changes which require
// updating descendant's parent pointers. The update functions use a
// create-or-update pattern of re-using existing properties for efficiency:
// 1. It avoids extra allocations.
// 2. It preserves existing child->parent pointers.
class CORE_EXPORT ObjectPaintProperties {
  WTF_MAKE_NONCOPYABLE(ObjectPaintProperties);
  USING_FAST_MALLOC(ObjectPaintProperties);

 public:
  static std::unique_ptr<ObjectPaintProperties> create() {
    return WTF::wrapUnique(new ObjectPaintProperties());
  }

  // The hierarchy of the transform subtree created by a LayoutObject is as
  // follows:
  // [ paintOffsetTranslation ]           Normally paint offset is accumulated
  // |                                    without creating a node until we see,
  // |                                    for example, transform or
  // |                                    position:fixed.
  // +---[ transform ]                    The space created by CSS transform.
  //     |                                This is the local border box space,
  //     |                                see: localBorderBoxProperties below.
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
  const TransformPaintPropertyNode* paintOffsetTranslation() const {
    return m_paintOffsetTranslation.get();
  }
  const TransformPaintPropertyNode* transform() const {
    return m_transform.get();
  }
  const TransformPaintPropertyNode* perspective() const {
    return m_perspective.get();
  }
  const TransformPaintPropertyNode* svgLocalToBorderBoxTransform() const {
    return m_svgLocalToBorderBoxTransform.get();
  }
  const TransformPaintPropertyNode* scrollTranslation() const {
    return m_scrollTranslation.get();
  }
  const TransformPaintPropertyNode* scrollbarPaintOffset() const {
    return m_scrollbarPaintOffset.get();
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
  const EffectPaintPropertyNode* effect() const { return m_effect.get(); }
  const EffectPaintPropertyNode* filter() const { return m_filter.get(); }
  const EffectPaintPropertyNode* mask() const { return m_mask.get(); }

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
  const ClipPaintPropertyNode* maskClip() const { return m_maskClip.get(); }
  const ClipPaintPropertyNode* cssClip() const { return m_cssClip.get(); }
  const ClipPaintPropertyNode* cssClipFixedPosition() const {
    return m_cssClipFixedPosition.get();
  }
  const ClipPaintPropertyNode* innerBorderRadiusClip() const {
    return m_innerBorderRadiusClip.get();
  }
  const ClipPaintPropertyNode* overflowClip() const {
    return m_overflowClip.get();
  }

  // This is a complete set of property nodes that should be used as a starting
  // point to paint this layout object. This is cached because some properties
  // inherit from the containing block chain instead of the painting parent and
  // cannot be derived in O(1) during the paint walk. For example:
  // <div style='opacity: 0.3;'/> would have a propertyTreeState.effect()
  // with opacity of 0.3 which was created by the div itself. Note that
  // propertyTreeState.transform() would not be null but would instead point to
  // the transform space setup by div's ancestors.
  const PropertyTreeState* localBorderBoxProperties() const {
    return m_localBorderBoxProperties.get();
  }

  // This is the complete set of property nodes that can be used to paint the
  // contents of this object. It is similar to localBorderBoxProperties but
  // includes properties (e.g., overflow clip, scroll translation) that apply to
  // contents. This is suitable for paint invalidation.
  const PropertyTreeState* contentsProperties() const {
    if (!m_contentsProperties) {
      if (!m_localBorderBoxProperties)
        return nullptr;
      updateContentsProperties();
    } else {
#if DCHECK_IS_ON()
      // Check if the cached m_contentsProperties is valid.
      DCHECK(m_localBorderBoxProperties);
      std::unique_ptr<PropertyTreeState> oldProperties =
          std::move(m_contentsProperties);
      updateContentsProperties();
      DCHECK(*m_contentsProperties == *oldProperties);
#endif
    }
    return m_contentsProperties.get();
  };

  void updateLocalBorderBoxProperties(
      const TransformPaintPropertyNode* transform,
      const ClipPaintPropertyNode* clip,
      const EffectPaintPropertyNode* effect) {
    if (m_localBorderBoxProperties) {
      m_localBorderBoxProperties->setTransform(transform);
      m_localBorderBoxProperties->setClip(clip);
      m_localBorderBoxProperties->setEffect(effect);
    } else {
      m_localBorderBoxProperties = WTF::wrapUnique(
          new PropertyTreeState(PropertyTreeState(transform, clip, effect)));
    }
    m_contentsProperties = nullptr;
  }
  void clearLocalBorderBoxProperties() {
    m_localBorderBoxProperties = nullptr;
    m_contentsProperties = nullptr;
  }

  // The following clear* functions return true if the property tree structure
  // changes (an existing node was deleted), and false otherwise. See the
  // class-level comment ("update & clear implementation note") for details
  // about why this is needed for efficient updates.
  bool clearPaintOffsetTranslation() { return clear(m_paintOffsetTranslation); }
  bool clearTransform() { return clear(m_transform); }
  bool clearEffect() { return clear(m_effect); }
  bool clearFilter() { return clear(m_filter); }
  bool clearMask() { return clear(m_mask); }
  bool clearMaskClip() { return clear(m_maskClip); }
  bool clearCssClip() { return clear(m_cssClip); }
  bool clearCssClipFixedPosition() { return clear(m_cssClipFixedPosition); }
  bool clearInnerBorderRadiusClip() { return clear(m_innerBorderRadiusClip); }
  bool clearOverflowClip() { return clear(m_overflowClip); }
  bool clearPerspective() { return clear(m_perspective); }
  bool clearSvgLocalToBorderBoxTransform() {
    return clear(m_svgLocalToBorderBoxTransform);
  }
  bool clearScrollTranslation() { return clear(m_scrollTranslation); }
  bool clearScrollbarPaintOffset() { return clear(m_scrollbarPaintOffset); }

  // The following update* functions return true if the property tree structure
  // changes (a new node was created), and false otherwise. See the class-level
  // comment ("update & clear implementation note") for details about why this
  // is needed for efficient updates.
  template <typename... Args>
  bool updatePaintOffsetTranslation(Args&&... args) {
    return update(m_paintOffsetTranslation, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateTransform(Args&&... args) {
    return update(m_transform, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updatePerspective(Args&&... args) {
    return update(m_perspective, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateSvgLocalToBorderBoxTransform(Args&&... args) {
    DCHECK(!scrollTranslation()) << "SVG elements cannot scroll so there "
                                    "should never be both a scroll translation "
                                    "and an SVG local to border box transform.";
    return update(m_svgLocalToBorderBoxTransform, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateScrollTranslation(Args&&... args) {
    DCHECK(!svgLocalToBorderBoxTransform())
        << "SVG elements cannot scroll so there should never be both a scroll "
           "translation and an SVG local to border box transform.";
    if (m_scrollTranslation) {
      m_scrollTranslation->updateScrollTranslation(std::forward<Args>(args)...);
      return false;
    }
    m_scrollTranslation = TransformPaintPropertyNode::createScrollTranslation(
        std::forward<Args>(args)...);
    return true;
  }
  template <typename... Args>
  bool updateScrollbarPaintOffset(Args&&... args) {
    return update(m_scrollbarPaintOffset, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateEffect(Args&&... args) {
    return update(m_effect, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateFilter(Args&&... args) {
    return update(m_filter, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateMask(Args&&... args) {
    return update(m_mask, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateMaskClip(Args&&... args) {
    return update(m_maskClip, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateCssClip(Args&&... args) {
    return update(m_cssClip, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateCssClipFixedPosition(Args&&... args) {
    return update(m_cssClipFixedPosition, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateInnerBorderRadiusClip(Args&&... args) {
    return update(m_innerBorderRadiusClip, std::forward<Args>(args)...);
  }
  template <typename... Args>
  bool updateOverflowClip(Args&&... args) {
    return update(m_overflowClip, std::forward<Args>(args)...);
  }

#if DCHECK_IS_ON()
  // Used by FindPropertiesNeedingUpdate.h for recording the current properties.
  std::unique_ptr<ObjectPaintProperties> clone() const {
    std::unique_ptr<ObjectPaintProperties> cloned = create();
    if (m_paintOffsetTranslation)
      cloned->m_paintOffsetTranslation = m_paintOffsetTranslation->clone();
    if (m_transform)
      cloned->m_transform = m_transform->clone();
    if (m_effect)
      cloned->m_effect = m_effect->clone();
    if (m_filter)
      cloned->m_filter = m_filter->clone();
    if (m_mask)
      cloned->m_mask = m_mask->clone();
    if (m_maskClip)
      cloned->m_maskClip = m_maskClip->clone();
    if (m_cssClip)
      cloned->m_cssClip = m_cssClip->clone();
    if (m_cssClipFixedPosition)
      cloned->m_cssClipFixedPosition = m_cssClipFixedPosition->clone();
    if (m_innerBorderRadiusClip)
      cloned->m_innerBorderRadiusClip = m_innerBorderRadiusClip->clone();
    if (m_overflowClip)
      cloned->m_overflowClip = m_overflowClip->clone();
    if (m_perspective)
      cloned->m_perspective = m_perspective->clone();
    if (m_svgLocalToBorderBoxTransform) {
      cloned->m_svgLocalToBorderBoxTransform =
          m_svgLocalToBorderBoxTransform->clone();
    }
    if (m_scrollTranslation)
      cloned->m_scrollTranslation = m_scrollTranslation->clone();
    if (m_scrollbarPaintOffset)
      cloned->m_scrollbarPaintOffset = m_scrollbarPaintOffset->clone();
    if (m_localBorderBoxProperties) {
      cloned->m_localBorderBoxProperties =
          WTF::wrapUnique(new PropertyTreeState(*m_localBorderBoxProperties));
    }
    return cloned;
  }
#endif

 private:
  ObjectPaintProperties() {}

  // Return true if the property tree structure changes (an existing node was
  // deleted), and false otherwise. See the class-level comment ("update & clear
  // implementation note") for details about why this is needed for efficiency.
  template <typename PaintPropertyNode>
  bool clear(RefPtr<PaintPropertyNode>& field) {
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
  bool update(RefPtr<PaintPropertyNode>& field, Args&&... args) {
    if (field) {
      field->update(std::forward<Args>(args)...);
      return false;
    }
    field = PaintPropertyNode::create(std::forward<Args>(args)...);
    return true;
  }

  void updateContentsProperties() const;

  RefPtr<TransformPaintPropertyNode> m_paintOffsetTranslation;
  RefPtr<TransformPaintPropertyNode> m_transform;
  RefPtr<EffectPaintPropertyNode> m_effect;
  RefPtr<EffectPaintPropertyNode> m_filter;
  RefPtr<EffectPaintPropertyNode> m_mask;
  RefPtr<ClipPaintPropertyNode> m_maskClip;
  RefPtr<ClipPaintPropertyNode> m_cssClip;
  RefPtr<ClipPaintPropertyNode> m_cssClipFixedPosition;
  RefPtr<ClipPaintPropertyNode> m_innerBorderRadiusClip;
  RefPtr<ClipPaintPropertyNode> m_overflowClip;
  RefPtr<TransformPaintPropertyNode> m_perspective;
  // TODO(pdr): Only LayoutSVGRoot needs this and it should be moved there.
  RefPtr<TransformPaintPropertyNode> m_svgLocalToBorderBoxTransform;
  RefPtr<TransformPaintPropertyNode> m_scrollTranslation;
  RefPtr<TransformPaintPropertyNode> m_scrollbarPaintOffset;

  std::unique_ptr<PropertyTreeState> m_localBorderBoxProperties;
  mutable std::unique_ptr<PropertyTreeState> m_contentsProperties;
};

}  // namespace blink

#endif  // ObjectPaintProperties_h
