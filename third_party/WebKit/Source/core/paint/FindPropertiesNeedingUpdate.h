// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FindPropertiesNeedingUpdate_h
#define FindPropertiesNeedingUpdate_h

#if DCHECK_IS_ON()
namespace blink {

// This file contains two scope classes for catching cases where paint
// properties need an update but where not marked as such. If paint properties
// will change, the object must be marked as needing a paint property update
// using {FrameView, LayoutObject}::setNeedsPaintPropertyUpdate() or by forcing
// a subtree update (see: PaintPropertyTreeBuilderContext::forceSubtreeupdate).
//
// Both scope classes work by recording the paint property state of an object
// before rebuilding properties, forcing the properties to get updated, then
// checking that the updated properties match the original properties.

#define DUMP_PROPERTIES(original, updated)                           \
  "\nOriginal:\n"                                                    \
      << (original ? (original)->toString().ascii().data() : "null") \
      << "\nUpdated:\n"                                              \
      << (updated ? (updated)->toString().ascii().data() : "null")

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
  CHECK_PROPERTY_EQ("the FrameView", original, updated)

class FindFrameViewPropertiesNeedingUpdateScope {
 public:
  FindFrameViewPropertiesNeedingUpdateScope(
      FrameView* frameView,
      PaintPropertyTreeBuilderContext& context)
      : m_frameView(frameView),
        m_neededPaintPropertyUpdate(frameView->needsPaintPropertyUpdate()),
        m_neededForcedSubtreeUpdate(context.forceSubtreeUpdate) {
    // No need to check if an update was already needed.
    if (m_neededPaintPropertyUpdate || m_neededForcedSubtreeUpdate)
      return;

    // Mark the properties as needing an update to ensure they are rebuilt.
    m_frameView->setOnlyThisNeedsPaintPropertyUpdateForTesting();

    if (auto* preTranslation = m_frameView->preTranslation())
      m_originalPreTranslation = preTranslation->clone();
    if (auto* contentClip = m_frameView->contentClip())
      m_originalContentClip = contentClip->clone();
    if (auto* scrollTranslation = m_frameView->scrollTranslation())
      m_originalScrollTranslation = scrollTranslation->clone();
    if (auto* scroll = m_frameView->scroll())
      m_originalScroll = scroll->clone();
  }

  ~FindFrameViewPropertiesNeedingUpdateScope() {
    // No need to check if an update was already needed.
    if (m_neededPaintPropertyUpdate || m_neededForcedSubtreeUpdate)
      return;

    // If these checks fail, the paint properties should not have changed but
    // did. This is due to missing one of these paint property invalidations:
    // 1) The FrameView should have been marked as needing an update with
    //    FrameView::setNeedsPaintPropertyUpdate().
    // 2) The PrePaintTreeWalk should have had a forced subtree update (see:
    //    PaintPropertyTreeBuilderContext::forceSubtreeupdate).
    DCHECK_FRAMEVIEW_PROPERTY_EQ(m_originalPreTranslation,
                                 m_frameView->preTranslation());
    DCHECK_FRAMEVIEW_PROPERTY_EQ(m_originalContentClip,
                                 m_frameView->contentClip());
    DCHECK_FRAMEVIEW_PROPERTY_EQ(m_originalScrollTranslation,
                                 m_frameView->scrollTranslation());
    DCHECK_FRAMEVIEW_PROPERTY_EQ(m_originalScroll, m_frameView->scroll());

    // Restore original clean bit.
    m_frameView->clearNeedsPaintPropertyUpdate();
  }

 private:
  Persistent<FrameView> m_frameView;
  bool m_neededPaintPropertyUpdate;
  bool m_neededForcedSubtreeUpdate;
  RefPtr<TransformPaintPropertyNode> m_originalPreTranslation;
  RefPtr<ClipPaintPropertyNode> m_originalContentClip;
  RefPtr<TransformPaintPropertyNode> m_originalScrollTranslation;
  RefPtr<ScrollPaintPropertyNode> m_originalScroll;
};

#define DCHECK_OBJECT_PROPERTY_EQ(object, original, updated)            \
  CHECK_PROPERTY_EQ("the layout object (" << object.debugName() << ")", \
                    original, updated)

class FindObjectPropertiesNeedingUpdateScope {
 public:
  FindObjectPropertiesNeedingUpdateScope(
      const LayoutObject& object,
      PaintPropertyTreeBuilderContext& context)
      : m_object(object),
        m_neededPaintPropertyUpdate(object.needsPaintPropertyUpdate()),
        m_neededForcedSubtreeUpdate(context.forceSubtreeUpdate) {
    // No need to check if an update was already needed.
    if (m_neededPaintPropertyUpdate || m_neededForcedSubtreeUpdate)
      return;

    // Mark the properties as needing an update to ensure they are rebuilt.
    const_cast<LayoutObject&>(m_object)
        .setOnlyThisNeedsPaintPropertyUpdateForTesting();

    if (const auto* properties = m_object.paintProperties())
      m_originalProperties = properties->clone();
  }

  ~FindObjectPropertiesNeedingUpdateScope() {
    // No need to check if an update was already needed.
    if (m_neededPaintPropertyUpdate || m_neededForcedSubtreeUpdate)
      return;

    // If these checks fail, the paint properties should not have changed but
    // did. This is due to missing one of these paint property invalidations:
    // 1) The LayoutObject should have been marked as needing an update with
    //    LayoutObject::setNeedsPaintPropertyUpdate().
    // 2) The PrePaintTreeWalk should have had a forced subtree update (see:
    //    PaintPropertyTreeBuilderContext::forceSubtreeupdate).
    const auto* objectProperties = m_object.paintProperties();
    if (m_originalProperties && objectProperties) {
      DCHECK_OBJECT_PROPERTY_EQ(m_object,
                                m_originalProperties->paintOffsetTranslation(),
                                objectProperties->paintOffsetTranslation());
      DCHECK_OBJECT_PROPERTY_EQ(m_object, m_originalProperties->transform(),
                                objectProperties->transform());
      DCHECK_OBJECT_PROPERTY_EQ(m_object, m_originalProperties->effect(),
                                objectProperties->effect());
      DCHECK_OBJECT_PROPERTY_EQ(m_object, m_originalProperties->cssClip(),
                                objectProperties->cssClip());
      DCHECK_OBJECT_PROPERTY_EQ(m_object,
                                m_originalProperties->cssClipFixedPosition(),
                                objectProperties->cssClipFixedPosition());
      DCHECK_OBJECT_PROPERTY_EQ(m_object,
                                m_originalProperties->innerBorderRadiusClip(),
                                objectProperties->innerBorderRadiusClip());
      DCHECK_OBJECT_PROPERTY_EQ(m_object, m_originalProperties->overflowClip(),
                                objectProperties->overflowClip());
      DCHECK_OBJECT_PROPERTY_EQ(m_object, m_originalProperties->perspective(),
                                objectProperties->perspective());
      DCHECK_OBJECT_PROPERTY_EQ(
          m_object, m_originalProperties->svgLocalToBorderBoxTransform(),
          objectProperties->svgLocalToBorderBoxTransform());
      DCHECK_OBJECT_PROPERTY_EQ(m_object,
                                m_originalProperties->scrollTranslation(),
                                objectProperties->scrollTranslation());
      DCHECK_OBJECT_PROPERTY_EQ(m_object,
                                m_originalProperties->scrollTranslation(),
                                objectProperties->scrollTranslation());
      DCHECK_OBJECT_PROPERTY_EQ(m_object,
                                m_originalProperties->scrollbarPaintOffset(),
                                objectProperties->scrollbarPaintOffset());
      const auto* originalBorderBox =
          m_originalProperties->localBorderBoxProperties();
      const auto* objectBorderBox =
          objectProperties->localBorderBoxProperties();
      if (originalBorderBox && objectBorderBox) {
        DCHECK_OBJECT_PROPERTY_EQ(m_object, &originalBorderBox->paintOffset,
                                  &objectBorderBox->paintOffset);
        DCHECK_OBJECT_PROPERTY_EQ(
            m_object, originalBorderBox->propertyTreeState.transform(),
            objectBorderBox->propertyTreeState.transform());
        DCHECK_OBJECT_PROPERTY_EQ(m_object,
                                  originalBorderBox->propertyTreeState.clip(),
                                  objectBorderBox->propertyTreeState.clip());
        DCHECK_OBJECT_PROPERTY_EQ(m_object,
                                  originalBorderBox->propertyTreeState.effect(),
                                  objectBorderBox->propertyTreeState.effect());
        DCHECK_OBJECT_PROPERTY_EQ(m_object,
                                  originalBorderBox->propertyTreeState.scroll(),
                                  objectBorderBox->propertyTreeState.scroll());
      } else {
        DCHECK_EQ(!!originalBorderBox, !!objectBorderBox);
      }
    } else {
      DCHECK_EQ(!!m_originalProperties, !!objectProperties);
    }
    // Restore original clean bit.
    const_cast<LayoutObject&>(m_object).clearNeedsPaintPropertyUpdate();
  }

 private:
  const LayoutObject& m_object;
  bool m_neededPaintPropertyUpdate;
  bool m_neededForcedSubtreeUpdate;
  std::unique_ptr<const ObjectPaintProperties> m_originalProperties;
};

}  // namespace blink
#endif  // DCHECK_IS_ON()

#endif  // FindPropertiesNeedingUpdate_h
