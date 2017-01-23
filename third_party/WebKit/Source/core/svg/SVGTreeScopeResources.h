// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGTreeScopeResources_h
#define SVGTreeScopeResources_h

#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/text/AtomicStringHash.h"

namespace blink {

class Element;
class TreeScope;
class LayoutSVGResourceContainer;

// This class keeps track of SVG resources and pending references to such for a
// TreeScope. It's per-TreeScope because that matches the lookup scope of an
// element's id (which is used to identify a resource.)
class SVGTreeScopeResources
    : public GarbageCollectedFinalized<SVGTreeScopeResources> {
  WTF_MAKE_NONCOPYABLE(SVGTreeScopeResources);

 public:
  typedef HeapHashSet<Member<Element>> SVGPendingElements;

  explicit SVGTreeScopeResources(TreeScope*);
  ~SVGTreeScopeResources();

  void addResource(const AtomicString& id, LayoutSVGResourceContainer*);
  void removeResource(const AtomicString& id);
  LayoutSVGResourceContainer* resourceById(const AtomicString& id) const;

  // Pending resources are such which are referenced by any object in the SVG
  // document, but do NOT exist yet. For instance, dynamically built gradients
  // / patterns / clippers...
  void addPendingResource(const AtomicString& id, Element*);
  bool hasPendingResource(const AtomicString& id) const;
  bool isElementPendingResources(Element*) const;
  bool isElementPendingResource(Element*, const AtomicString& id) const;
  void clearHasPendingResourcesIfPossible(Element*);
  void removeElementFromPendingResources(Element*);
  SVGPendingElements* removePendingResource(const AtomicString& id);

  DECLARE_TRACE();

 private:
  HashMap<AtomicString, LayoutSVGResourceContainer*> m_resources;
  // Resources that are pending.
  HeapHashMap<AtomicString, Member<SVGPendingElements>> m_pendingResources;
};

}  // namespace blink

#endif  // SVGTreeScopeResources_h
