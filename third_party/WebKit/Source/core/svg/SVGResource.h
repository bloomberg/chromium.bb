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
class SVGResource : public GarbageCollectedFinalized<SVGResource> {
 public:
  virtual ~SVGResource();

  Element* Target() const { return target_; }
  LayoutSVGResourceContainer* ResourceContainer() const;

  void AddClient(SVGResourceClient&);
  void RemoveClient(SVGResourceClient&);

  bool HasClients() const { return !clients_.IsEmpty(); }

  virtual void Trace(Visitor*);

 protected:
  SVGResource();

  Member<Element> target_;
  HeapHashCountedSet<Member<SVGResourceClient>> clients_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SVGResource);
};

// Local resource reference (see SVGResource.)
class LocalSVGResource final : public SVGResource {
 public:
  LocalSVGResource(TreeScope&, const AtomicString& id);

  void AddWatch(SVGElement&);
  void RemoveWatch(SVGElement&);

  void Unregister();

  bool IsEmpty() const;

  void NotifyPendingClients();
  void NotifyContentChanged();

  void Trace(Visitor*) override;

 private:
  void TargetChanged(const AtomicString& id);
  void NotifyElementChanged();

  Member<TreeScope> tree_scope_;
  Member<IdTargetObserver> id_observer_;
  HeapHashSet<Member<SVGElement>> pending_clients_;
};

}  // namespace blink

#endif  // SVGResource_h
