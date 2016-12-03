// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IntersectionGeometry_h
#define IntersectionGeometry_h

#include "platform/Length.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/heap/Handle.h"
#include "wtf/Vector.h"

namespace blink {

class Node;
class Element;
class LayoutObject;

class IntersectionGeometry final
    : public GarbageCollectedFinalized<IntersectionGeometry> {
 public:
  enum ReportRootBounds {
    kShouldReportRootBounds,
    kShouldNotReportRootBounds,
  };

  IntersectionGeometry(Node* root,
                       Element* target,
                       const Vector<Length>& rootMargin,
                       ReportRootBounds shouldReportRootBounds);
  ~IntersectionGeometry();

  void computeGeometry();
  LayoutRect targetRect() const { return m_targetRect; }
  LayoutRect intersectionRect() const { return m_intersectionRect; }
  LayoutRect rootRect() const { return m_rootRect; }
  bool doesIntersect() const { return m_doesIntersect; }

  IntRect intersectionIntRect() const {
    return pixelSnappedIntRect(m_intersectionRect);
  }

  IntRect targetIntRect() const { return pixelSnappedIntRect(m_targetRect); }

  IntRect rootIntRect() const { return pixelSnappedIntRect(m_rootRect); }

  DECLARE_TRACE();

 private:
  void initializeGeometry();
  void initializeTargetRect();
  void initializeRootRect();
  void clipToRoot();
  void mapTargetRectToTargetFrameCoordinates();
  void mapRootRectToRootFrameCoordinates();
  void mapRootRectToTargetFrameCoordinates();
  Element* root() const;
  LayoutObject* getRootLayoutObject() const;
  void applyRootMargin();

  Member<Node> m_root;
  Member<Element> m_target;
  const Vector<Length> m_rootMargin;
  const ReportRootBounds m_shouldReportRootBounds;
  LayoutRect m_targetRect;
  LayoutRect m_intersectionRect;
  LayoutRect m_rootRect;
  bool m_doesIntersect = false;
};

}  // namespace blink

#endif  // IntersectionGeometry_h
