// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGResource_h
#define SVGResource_h

#include "base/macros.h"
#include "core/dom/IdTargetObserver.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/HashSet.h"

namespace blink {

class Element;
class LayoutSVGResourceContainer;
class SVGElement;
class TreeScope;

// A class tracking a reference to an SVG resource (an element that constitutes
// a paint server, mask, clip-path, filter et.c.)
class SVGResource : public IdTargetObserver {
 public:
  SVGResource(TreeScope&, const AtomicString& id);
  ~SVGResource() override;

  Element* Target() const { return target_; }
  LayoutSVGResourceContainer* ResourceContainer() const;

  void AddWatch(SVGElement&);
  void RemoveWatch(SVGElement&);

  bool IsEmpty() const;

  void Trace(Visitor*);

  void NotifyResourceClients();

 private:
  void IdTargetChanged() override;

  Member<TreeScope> tree_scope_;
  Member<Element> target_;
  HeapHashSet<Member<SVGElement>> pending_clients_;

  DISALLOW_COPY_AND_ASSIGN(SVGResource);
};

}  // namespace blink

#endif  // SVGResource_h
