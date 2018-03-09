// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGTreeScopeResources_h
#define SVGTreeScopeResources_h

#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashMap.h"
#include "platform/wtf/text/AtomicStringHash.h"

namespace blink {

class LocalSVGResource;
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

  LocalSVGResource* ResourceForId(const AtomicString& id);
  LocalSVGResource* ExistingResourceForId(const AtomicString& id) const;

  void RemoveUnreferencedResources();
  void RemoveWatchesForElement(SVGElement&);

  void Trace(blink::Visitor*);

 private:
  HeapHashMap<AtomicString, Member<LocalSVGResource>> resources_;
  Member<TreeScope> tree_scope_;

  DISALLOW_COPY_AND_ASSIGN(SVGTreeScopeResources);
};

}  // namespace blink

#endif  // SVGTreeScopeResources_h
