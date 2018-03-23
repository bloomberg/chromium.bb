// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGResource_h
#define SVGResource_h

#include "base/macros.h"
#include "core/svg/SVGResourceClient.h"
#include "platform/heap/Handle.h"
#include "platform/loader/fetch/ResourceClient.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/HashCountedSet.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class Document;
class DocumentResource;
class Element;
class IdTargetObserver;
class LayoutSVGResourceContainer;
class SVGElement;
class TreeScope;

// A class tracking a reference to an SVG resource (an element that constitutes
// a paint server, mask, clip-path, filter et.c.)
//
// Elements can be referenced using CSS, for example like:
//
//   filter: url(#baz);                             ("local")
//
//    or
//
//   filter: url(foo.com/bar.svg#baz);              ("external")
//
// SVGResource provide a mechanism to persistently reference an element in such
// cases - regardless of if the element reside in the same document (read: tree
// scope) or in an external (resource) document. Loading events related to the
// external documents case are handled by the SVGResource.
//
// For same document references, changes that could affect the 'id' lookup will
// be tracked, to handle elements being added, removed or having their 'id'
// mutated. (This does not apply for the external document case because it's
// assumed they will not mutate after load, due to scripts not being run etc.)
//
// SVGResources are created, and managed, either by SVGTreeScopeResources
// (local) or CSSURIValue (external), and have SVGResourceClients as a means to
// deliver change notifications. Clients that are interested in change
// notifications hence need to register a SVGResourceClient with the
// SVGResource. Most commonly this registration would take place when the
// computed style changes.
//
// The element is bound either when the SVGResource is created (for local
// resources) or after the referenced resource has completed loading (for
// external resources.)
//
// As content is mutated, clients will get notified via the SVGResource.
//
// <event> -> SVG...Element -> SVGResource -> SVGResourceClient(0..N)
//
class SVGResource : public GarbageCollectedFinalized<SVGResource> {
 public:
  virtual ~SVGResource();

  virtual void Load(const Document&) {}

  Element* Target() const { return target_; }
  LayoutSVGResourceContainer* ResourceContainer() const;

  void AddClient(SVGResourceClient&);
  void RemoveClient(SVGResourceClient&);

  bool HasClients() const { return !clients_.IsEmpty(); }

  virtual void Trace(Visitor*);

 protected:
  SVGResource();

  void NotifyElementChanged();

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
  void NotifyContentChanged(InvalidationModeMask);

  void Trace(Visitor*) override;

 private:
  void TargetChanged(const AtomicString& id);

  Member<TreeScope> tree_scope_;
  Member<IdTargetObserver> id_observer_;
  HeapHashSet<Member<SVGElement>> pending_clients_;
};

// External resource reference (see SVGResource.)
class ExternalSVGResource final : public SVGResource, private ResourceClient {
  USING_GARBAGE_COLLECTED_MIXIN(ExternalSVGResource);

 public:
  explicit ExternalSVGResource(const KURL&);

  void Load(const Document&) override;

  void Trace(Visitor*) override;

 private:
  Element* ResolveTarget();

  // ResourceClient implementation
  String DebugName() const override;
  void NotifyFinished(Resource*) override;

  KURL url_;
  Member<DocumentResource> resource_document_;
};

}  // namespace blink

#endif  // SVGResource_h
