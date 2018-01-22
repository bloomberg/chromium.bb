// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGTreeScopeResources_h
#define SVGTreeScopeResources_h

#include "base/macros.h"
#include "core/dom/IdTargetObserver.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class Element;
class LayoutSVGResourceContainer;
class SVGElement;
class TreeScope;

// This class keeps track of SVG resources and pending references to such for a
// TreeScope. It's per-TreeScope because that matches the lookup scope of an
// element's id (which is used to identify a resource.)
class SVGTreeScopeResources
    : public GarbageCollectedFinalized<SVGTreeScopeResources> {
 public:
  explicit SVGTreeScopeResources(TreeScope*);
  ~SVGTreeScopeResources();

  class Resource : public IdTargetObserver {
   public:
    Resource(TreeScope&, const AtomicString& id);
    ~Resource() override;

    Element* Target() const { return target_; }
    LayoutSVGResourceContainer* ResourceContainer() const;

    void AddWatch(SVGElement&);
    void RemoveWatch(SVGElement&);

    bool IsEmpty() const;

    void Trace(blink::Visitor*);

    void NotifyResourceClients();

   private:
    void IdTargetChanged() override;

    Member<TreeScope> tree_scope_;
    Member<Element> target_;
    HeapHashSet<Member<SVGElement>> pending_clients_;
  };
  Resource* ResourceForId(const AtomicString& id);
  Resource* ExistingResourceForId(const AtomicString& id) const;

  void RemoveUnreferencedResources();
  void RemoveWatchesForElement(SVGElement&);

  void Trace(blink::Visitor*);

 private:
  HeapHashMap<AtomicString, Member<Resource>> resources_;
  Member<TreeScope> tree_scope_;

  DISALLOW_COPY_AND_ASSIGN(SVGTreeScopeResources);
};

}  // namespace blink

#endif  // SVGTreeScopeResources_h
