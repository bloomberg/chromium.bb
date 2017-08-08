// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FindPropertiesNeedingUpdate_h
#define FindPropertiesNeedingUpdate_h

#if DCHECK_IS_ON()

#include "core/frame/LocalFrameView.h"
#include "core/layout/LayoutObject.h"
#include "core/paint/ObjectPaintProperties.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

// This file contains two scope classes for catching cases where paint
// properties needed an update but were not marked as such. If paint properties
// will change, the object must be marked as needing a paint property update
// using {LocalFrameView, LayoutObject}::SetNeedsPaintPropertyUpdate() or by
// forcing a subtree update (see:
// PaintPropertyTreeBuilderContext::force_subtree_update).
//
// Both scope classes work by recording the paint property state of an object
// before rebuilding properties, forcing the properties to get updated, then
// checking that the updated properties match the original properties.

#define DUMP_PROPERTIES(original, updated)                           \
  "\nOriginal:\n"                                                    \
      << (original ? (original)->ToString().Ascii().data() : "null") \
      << "\nUpdated:\n"                                              \
      << (updated ? (updated)->ToString().Ascii().data() : "null")

#define CHECK_PROPERTY_EQ(thing, original, updated)                        \
  do {                                                                     \
    DCHECK(!!original == !!updated) << "Property was created or deleted "  \
                                    << "without " << thing                 \
                                    << " needing a paint property update." \
                                    << DUMP_PROPERTIES(original, updated); \
    if (!!original && !!updated) {                                         \
      DCHECK(*original == *updated) << "Property was updated without "     \
                                    << thing                               \
                                    << " needing a paint property update." \
                                    << DUMP_PROPERTIES(original, updated); \
    }                                                                      \
  } while (0)

#define DCHECK_FRAMEVIEW_PROPERTY_EQ(original, updated) \
  CHECK_PROPERTY_EQ("the LocalFrameView", original, updated)

class FindFrameViewPropertiesNeedingUpdateScope {
 public:
  FindFrameViewPropertiesNeedingUpdateScope(LocalFrameView* frame_view,
                                            bool force_subtree_update)
      : frame_view_(frame_view),
        needed_paint_property_update_(frame_view->NeedsPaintPropertyUpdate()),
        needed_forced_subtree_update_(force_subtree_update) {
    // No need to check if an update was already needed.
    if (needed_paint_property_update_ || needed_forced_subtree_update_)
      return;

    // Mark the properties as needing an update to ensure they are rebuilt.
    frame_view_->SetOnlyThisNeedsPaintPropertyUpdateForTesting();

    if (auto* pre_translation = frame_view_->PreTranslation())
      original_pre_translation_ = pre_translation->Clone();
    if (auto* content_clip = frame_view_->ContentClip())
      original_content_clip_ = content_clip->Clone();
    if (auto* scroll_node = frame_view_->ScrollNode())
      original_scroll_node_ = scroll_node->Clone();
    if (auto* scroll_translation = frame_view_->ScrollTranslation())
      original_scroll_translation_ = scroll_translation->Clone();
  }

  ~FindFrameViewPropertiesNeedingUpdateScope() {
    // No need to check if an update was already needed.
    if (needed_paint_property_update_ || needed_forced_subtree_update_)
      return;

    // If these checks fail, the paint properties changed unexpectedly. This is
    // due to missing one of these paint property invalidations:
    // 1) The LocalFrameView should have been marked as needing an update with
    //    LocalFrameView::SetNeedsPaintPropertyUpdate().
    // 2) The PrePaintTreeWalk should have had a forced subtree update (see:
    //    PaintPropertyTreeBuilderContext::force_subtree_update).
    DCHECK_FRAMEVIEW_PROPERTY_EQ(original_pre_translation_,
                                 frame_view_->PreTranslation());
    DCHECK_FRAMEVIEW_PROPERTY_EQ(original_content_clip_,
                                 frame_view_->ContentClip());
    DCHECK_FRAMEVIEW_PROPERTY_EQ(original_scroll_node_,
                                 frame_view_->ScrollNode());
    DCHECK_FRAMEVIEW_PROPERTY_EQ(original_scroll_translation_,
                                 frame_view_->ScrollTranslation());

    // Restore original clean bit.
    frame_view_->ClearNeedsPaintPropertyUpdate();
  }

 private:
  Persistent<LocalFrameView> frame_view_;
  bool needed_paint_property_update_;
  bool needed_forced_subtree_update_;
  RefPtr<const TransformPaintPropertyNode> original_pre_translation_;
  RefPtr<const ClipPaintPropertyNode> original_content_clip_;
  RefPtr<const ScrollPaintPropertyNode> original_scroll_node_;
  RefPtr<const TransformPaintPropertyNode> original_scroll_translation_;
};

#define DCHECK_OBJECT_PROPERTY_EQ(object, original, updated)            \
  CHECK_PROPERTY_EQ("the layout object (" << object.DebugName() << ")", \
                    original, updated)

class FindObjectPropertiesNeedingUpdateScope {
 public:
  FindObjectPropertiesNeedingUpdateScope(const LayoutObject& object,
                                         bool force_subtree_update)
      : object_(object),
        needed_paint_property_update_(object.NeedsPaintPropertyUpdate()),
        needed_forced_subtree_update_(force_subtree_update),
        original_paint_offset_(object.PaintOffset()) {
    // No need to check if an update was already needed.
    if (needed_paint_property_update_ || needed_forced_subtree_update_)
      return;

    // Mark the properties as needing an update to ensure they are rebuilt.
    object_.GetMutableForPainting()
        .SetOnlyThisNeedsPaintPropertyUpdateForTesting();

    if (const auto* fragment_data = object_.FirstFragment()) {
      if (const auto* properties = fragment_data->PaintProperties())
        original_properties_ = properties->Clone();

      if (const auto* local_border_box =
              fragment_data->LocalBorderBoxProperties()) {
        original_local_border_box_properties_ =
            WTF::WrapUnique(new PropertyTreeState(*local_border_box));
      }
    }
  }

  ~FindObjectPropertiesNeedingUpdateScope() {
    // Paint offset and paintOffsetTranslation should not change under
    // FindObjectPropertiesNeedingUpdateScope no matter if we needed paint
    // property update.
    DCHECK_OBJECT_PROPERTY_EQ(object_, &original_paint_offset_,
                              &object_.PaintOffset());
    const auto* object_properties =
        object_.FirstFragment() ? object_.FirstFragment()->PaintProperties()
                                : nullptr;
    if (original_properties_ && object_properties) {
      DCHECK_OBJECT_PROPERTY_EQ(object_,
                                original_properties_->PaintOffsetTranslation(),
                                object_properties->PaintOffsetTranslation());
    }

    // No need to check if an update was already needed.
    if (needed_paint_property_update_ || needed_forced_subtree_update_)
      return;

    // If these checks fail, the paint properties changed unexpectedly. This is
    // due to missing one of these paint property invalidations:
    // 1) The LayoutObject should have been marked as needing an update with
    //    LayoutObject::setNeedsPaintPropertyUpdate().
    // 2) The PrePaintTreeWalk should have had a forced subtree update (see:
    //    PaintPropertyTreeBuilderContext::force_subtree_update).
    if (original_properties_ && object_properties) {
      DCHECK_OBJECT_PROPERTY_EQ(object_, original_properties_->Transform(),
                                object_properties->Transform());
      DCHECK_OBJECT_PROPERTY_EQ(object_, original_properties_->Effect(),
                                object_properties->Effect());
      DCHECK_OBJECT_PROPERTY_EQ(object_, original_properties_->Filter(),
                                object_properties->Filter());
      DCHECK_OBJECT_PROPERTY_EQ(object_, original_properties_->Mask(),
                                object_properties->Mask());
      DCHECK_OBJECT_PROPERTY_EQ(object_, original_properties_->MaskClip(),
                                object_properties->MaskClip());
      DCHECK_OBJECT_PROPERTY_EQ(object_, original_properties_->CssClip(),
                                object_properties->CssClip());
      DCHECK_OBJECT_PROPERTY_EQ(object_,
                                original_properties_->CssClipFixedPosition(),
                                object_properties->CssClipFixedPosition());
      DCHECK_OBJECT_PROPERTY_EQ(object_,
                                original_properties_->InnerBorderRadiusClip(),
                                object_properties->InnerBorderRadiusClip());
      DCHECK_OBJECT_PROPERTY_EQ(object_, original_properties_->OverflowClip(),
                                object_properties->OverflowClip());
      DCHECK_OBJECT_PROPERTY_EQ(object_, original_properties_->Perspective(),
                                object_properties->Perspective());
      DCHECK_OBJECT_PROPERTY_EQ(
          object_, original_properties_->SvgLocalToBorderBoxTransform(),
          object_properties->SvgLocalToBorderBoxTransform());
      DCHECK_OBJECT_PROPERTY_EQ(object_, original_properties_->Scroll(),
                                object_properties->Scroll());
      DCHECK_OBJECT_PROPERTY_EQ(object_,
                                original_properties_->ScrollTranslation(),
                                object_properties->ScrollTranslation());
      DCHECK_OBJECT_PROPERTY_EQ(object_,
                                original_properties_->ScrollbarPaintOffset(),
                                object_properties->ScrollbarPaintOffset());
    } else {
      DCHECK_EQ(!!original_properties_, !!object_properties)
          << " Object: " << object_.DebugName();
    }

    const auto* object_border_box =
        object_.FirstFragment()
            ? object_.FirstFragment()->LocalBorderBoxProperties()
            : nullptr;
    if (original_local_border_box_properties_ && object_border_box) {
      DCHECK_OBJECT_PROPERTY_EQ(
          object_, original_local_border_box_properties_->Transform(),
          object_border_box->Transform());
      DCHECK_OBJECT_PROPERTY_EQ(object_,
                                original_local_border_box_properties_->Clip(),
                                object_border_box->Clip());
      DCHECK_OBJECT_PROPERTY_EQ(object_,
                                original_local_border_box_properties_->Effect(),
                                object_border_box->Effect());
    } else {
      DCHECK_EQ(!!original_local_border_box_properties_, !!object_border_box)
          << " Object: " << object_.DebugName();
    }

    // Restore original clean bit.
    object_.GetMutableForPainting().ClearNeedsPaintPropertyUpdateForTesting();
  }

 private:
  const LayoutObject& object_;
  bool needed_paint_property_update_;
  bool needed_forced_subtree_update_;
  LayoutPoint original_paint_offset_;
  std::unique_ptr<const ObjectPaintProperties> original_properties_;
  std::unique_ptr<const PropertyTreeState>
      original_local_border_box_properties_;
};

}  // namespace blink
#endif  // DCHECK_IS_ON()

#endif  // FindPropertiesNeedingUpdate_h
