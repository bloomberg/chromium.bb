// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FindPaintOffsetAndVisualRectNeedingUpdate_h
#define FindPaintOffsetAndVisualRectNeedingUpdate_h

#if DCHECK_IS_ON()

#include "core/layout/LayoutObject.h"
#include "core/paint/FindPropertiesNeedingUpdate.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

// This file contains scope classes for catching cases where paint offset or
// visual rect needed an update but were not marked as such. If paint offset or
// any visual rect (including visual rect of the object itself, scroll controls,
// caret, selection, etc.) will change, the object must be marked as such by
// LayoutObject::setNeedsPaintOffsetAndVisualRectUpdate() (which is a private
// function called by several public paint-invalidation-flag setting functions).

class FindPaintOffsetNeedingUpdateScope {
 public:
  FindPaintOffsetNeedingUpdateScope(
      const LayoutObject& object,
      const PaintPropertyTreeBuilderContext& context)
      : m_object(object),
        m_context(context),
        m_oldPaintOffset(object.paintOffset()) {
    if (object.paintProperties() &&
        object.paintProperties()->paintOffsetTranslation()) {
      m_oldPaintOffsetTranslation =
          object.paintProperties()->paintOffsetTranslation()->clone();
    }
  }

  ~FindPaintOffsetNeedingUpdateScope() {
    if (m_context.isActuallyNeeded)
      return;
    DCHECK_OBJECT_PROPERTY_EQ(m_object, &m_oldPaintOffset,
                              &m_object.paintOffset());
    const auto* paintOffsetTranslation =
        m_object.paintProperties()
            ? m_object.paintProperties()->paintOffsetTranslation()
            : nullptr;
    DCHECK_OBJECT_PROPERTY_EQ(m_object, m_oldPaintOffsetTranslation.get(),
                              paintOffsetTranslation);
  }

 private:
  const LayoutObject& m_object;
  const PaintPropertyTreeBuilderContext& m_context;
  LayoutPoint m_oldPaintOffset;
  RefPtr<const TransformPaintPropertyNode> m_oldPaintOffsetTranslation;
};

class FindVisualRectNeedingUpdateScopeBase {
 protected:
  FindVisualRectNeedingUpdateScopeBase(const LayoutObject& object,
                                       const PaintInvalidatorContext& context,
                                       const LayoutRect& oldVisualRect)
      : m_object(object),
        m_context(context),
        m_oldVisualRect(oldVisualRect),
        m_neededVisualRectUpdate(context.needsVisualRectUpdate(object)) {
    if (m_neededVisualRectUpdate) {
      DCHECK(!RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled() ||
             (context.m_treeBuilderContext &&
              context.m_treeBuilderContext->isActuallyNeeded));
      return;
    }
    context.m_forceVisualRectUpdateForChecking = true;
    DCHECK(context.needsVisualRectUpdate(object));
  }

  ~FindVisualRectNeedingUpdateScopeBase() {
    m_context.m_forceVisualRectUpdateForChecking = false;
    DCHECK_EQ(m_neededVisualRectUpdate,
              m_context.needsVisualRectUpdate(m_object));
  }

  static LayoutRect inflatedRect(const LayoutRect& r) {
    LayoutRect result = r;
    result.inflate(1);
    return result;
  }

  void checkVisualRect(const LayoutRect& newVisualRect) {
    if (m_neededVisualRectUpdate)
      return;
    DCHECK((m_oldVisualRect.isEmpty() && newVisualRect.isEmpty()) ||
           m_object.enclosingLayer()->subtreeIsInvisible() ||
           m_oldVisualRect == newVisualRect ||
           // The following check is to tolerate the differences caused by
           // pixel snapping that may happen for one rect but not for another
           // while we need neither paint invalidation nor raster invalidation
           // for the change. This may miss some real subpixel changes of visual
           // rects. TODO(wangxianzhu): Look into whether we can tighten this
           // for SPv2.
           inflatedRect(m_oldVisualRect).contains(newVisualRect) ||
           inflatedRect(newVisualRect).contains(m_oldVisualRect))
        << "Visual rect changed without needing update"
        << " object=" << m_object.debugName()
        << " old=" << m_oldVisualRect.toString()
        << " new=" << newVisualRect.toString();
  }

  const LayoutObject& m_object;
  const PaintInvalidatorContext& m_context;
  LayoutRect m_oldVisualRect;
  bool m_neededVisualRectUpdate;
};

// For updates of visual rects (e.g. of scroll controls, caret, selection,etc.)
// contained by an object.
class FindVisualRectNeedingUpdateScope : FindVisualRectNeedingUpdateScopeBase {
 public:
  FindVisualRectNeedingUpdateScope(const LayoutObject& object,
                                   const PaintInvalidatorContext& context,
                                   const LayoutRect& oldVisualRect,
                                   // Must be a reference to a rect that
                                   // outlives this scope.
                                   const LayoutRect& newVisualRect)
      : FindVisualRectNeedingUpdateScopeBase(object, context, oldVisualRect),
        m_newVisualRectRef(newVisualRect) {}

  ~FindVisualRectNeedingUpdateScope() { checkVisualRect(m_newVisualRectRef); }

 private:
  const LayoutRect& m_newVisualRectRef;
};

// For updates of object visual rect and location.
class FindObjectVisualRectNeedingUpdateScope
    : FindVisualRectNeedingUpdateScopeBase {
 public:
  FindObjectVisualRectNeedingUpdateScope(const LayoutObject& object,
                                         const PaintInvalidatorContext& context)
      : FindVisualRectNeedingUpdateScopeBase(object,
                                             context,
                                             object.visualRect()),
        m_oldLocation(ObjectPaintInvalidator(object).locationInBacking()) {}

  ~FindObjectVisualRectNeedingUpdateScope() {
    checkVisualRect(m_object.visualRect());
    checkLocation();
  }

  void checkLocation() {
    if (m_neededVisualRectUpdate)
      return;
    LayoutPoint newLocation =
        ObjectPaintInvalidator(m_object).locationInBacking();
    // Location of LayoutText and non-root SVG is location of the visual rect
    // which have been checked above.
    DCHECK(m_object.isText() || m_object.isSVGChild() ||
           newLocation == m_oldLocation ||
           m_object.enclosingLayer()->subtreeIsInvisible() ||
           // See checkVisualRect for the issue of approximation.
           LayoutRect(-1, -1, 2, 2)
               .contains(LayoutRect(LayoutPoint(newLocation - m_oldLocation),
                                    LayoutSize())))
        << "Location changed without needing update"
        << " object=" << m_object.debugName()
        << " old=" << m_oldLocation.toString()
        << " new=" << newLocation.toString();
  }

 private:
  LayoutPoint m_oldLocation;
};

}  // namespace blink

#endif  // DCHECK_IS_ON()

#endif  // FindPaintOffsetAndVisualRectNeedingUpdate_h
