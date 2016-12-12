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

class Element;
class LayoutObject;

// Computes the intersection between an ancestor (root) element and a
// descendant (target) element, with overflow and CSS clipping applied, but not
// paint occlusion.
//
// If the root argument to the constructor is null, computes the intersection
// of the target with the top-level frame viewport (AKA the "implicit root").
class IntersectionGeometry {
  STACK_ALLOCATED()
 public:
  IntersectionGeometry(Element* root,
                       Element& target,
                       const Vector<Length>& rootMargin,
                       bool shouldReportRootBounds);
  ~IntersectionGeometry();

  void computeGeometry();

  LayoutObject* root() const { return m_root; }
  LayoutObject* target() const { return m_target; }

  // Client rect in the coordinate system of the frame containing target.
  LayoutRect targetRect() const { return m_targetRect; }

  // Client rect in the coordinate system of the frame containing target.
  LayoutRect intersectionRect() const { return m_intersectionRect; }

  // Client rect in the coordinate system of the frame containing root.
  LayoutRect rootRect() const { return m_rootRect; }

  bool doesIntersect() const { return m_doesIntersect; }

  IntRect intersectionIntRect() const {
    return pixelSnappedIntRect(m_intersectionRect);
  }

  IntRect targetIntRect() const { return pixelSnappedIntRect(m_targetRect); }

  IntRect rootIntRect() const { return pixelSnappedIntRect(m_rootRect); }

 private:
  bool initializeCanComputeGeometry(Element* root, Element& target) const;
  void initializeGeometry();
  void initializeTargetRect();
  void initializeRootRect();
  void clipToRoot();
  void mapTargetRectToTargetFrameCoordinates();
  void mapRootRectToRootFrameCoordinates();
  void mapIntersectionRectToTargetFrameCoordinates();
  void applyRootMargin();

  // Returns true iff it's possible to compute an intersection between root
  // and target.
  bool canComputeGeometry() const { return m_canComputeGeometry; }
  bool rootIsImplicit() const { return m_rootIsImplicit; }
  bool shouldReportRootBounds() const { return m_shouldReportRootBounds; }

  LayoutObject* m_root;
  LayoutObject* m_target;
  const Vector<Length> m_rootMargin;
  LayoutRect m_targetRect;
  LayoutRect m_intersectionRect;
  LayoutRect m_rootRect;
  unsigned m_doesIntersect : 1;
  const unsigned m_shouldReportRootBounds : 1;
  const unsigned m_rootIsImplicit : 1;
  const unsigned m_canComputeGeometry : 1;
};

}  // namespace blink

#endif  // IntersectionGeometry_h
