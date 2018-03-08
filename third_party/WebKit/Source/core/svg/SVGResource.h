// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGResource_h
#define SVGResource_h

#include "base/macros.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashCountedSet.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class Element;
class IdTargetObserver;
class LayoutSVGResourceContainer;
class SVGElement;
class SVGResourceClient;
class TreeScope;

// A class tracking a reference to an SVG resource (an element that constitutes
// a paint server, mask, clip-path, filter et.c.)
class SVGResource : public GarbageCollected<SVGResource> {
 public:
  SVGResource(TreeScope&, const AtomicString& id);

  Element* Target() const { return target_; }
  LayoutSVGResourceContainer* ResourceContainer() const;

  void AddClient(SVGResourceClient&);
  void RemoveClient(SVGResourceClient&);

  bool HasClients() const { return !clients_.IsEmpty(); }

  void AddWatch(SVGElement&);
  void RemoveWatch(SVGElement&);

  void Unregister();

  bool IsEmpty() const;

  void Trace(Visitor*);

  void NotifyPendingClients();
  void NotifyContentChanged();

 private:
  void TargetChanged(const AtomicString& id);
  void NotifyElementChanged();

  Member<TreeScope> tree_scope_;
  Member<Element> target_;
  Member<IdTargetObserver> id_observer_;
  HeapHashCountedSet<Member<SVGResourceClient>> clients_;
  HeapHashSet<Member<SVGElement>> pending_clients_;

  DISALLOW_COPY_AND_ASSIGN(SVGResource);
};

}  // namespace blink

#endif  // SVGResource_h
