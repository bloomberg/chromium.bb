// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGTreeScopeResources_h
#define SVGTreeScopeResources_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/AtomicStringHash.h"

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
  explicit SVGTreeScopeResources(TreeScope*);
  ~SVGTreeScopeResources();

  void UpdateResource(const AtomicString& id, LayoutSVGResourceContainer*);
  void UpdateResource(const AtomicString& old_id,
                      const AtomicString& new_id,
                      LayoutSVGResourceContainer*);
  void RemoveResource(const AtomicString& id, LayoutSVGResourceContainer*);
  LayoutSVGResourceContainer* ResourceById(const AtomicString& id) const;

  // Pending resources are such which are referenced by any object in the SVG
  // document, but do NOT exist yet. For instance, dynamically built gradients
  // / patterns / clippers...
  void AddPendingResource(const AtomicString& id, Element&);
  bool IsElementPendingResource(Element&, const AtomicString& id) const;
  void NotifyResourceAvailable(const AtomicString& id);
  void RemoveElementFromPendingResources(Element&);

  void Trace(blink::Visitor*);

 private:
  void ClearHasPendingResourcesIfPossible(Element&);

  using SVGPendingElements = HeapHashSet<Member<Element>>;
  using ResourceMap = HashMap<AtomicString, LayoutSVGResourceContainer*>;

  void RegisterResource(const AtomicString& id, LayoutSVGResourceContainer*);
  void UnregisterResource(ResourceMap::iterator);
  void NotifyPendingClients(const AtomicString& id);

  ResourceMap resources_;
  // Resources that are pending.
  HeapHashMap<AtomicString, Member<SVGPendingElements>> pending_resources_;
  Member<TreeScope> tree_scope_;
};

}  // namespace blink

#endif  // SVGTreeScopeResources_h
