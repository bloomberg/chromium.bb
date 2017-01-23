/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef SVGDocumentExtensions_h
#define SVGDocumentExtensions_h

#include "core/layout/svg/SVGResourcesCache.h"
#include "platform/geometry/FloatPoint.h"
#include "platform/heap/Handle.h"
#include "wtf/Forward.h"
#include "wtf/HashSet.h"

namespace blink {

class Document;
class SVGElement;
class SVGSVGElement;
class SubtreeLayoutScope;

class SVGDocumentExtensions
    : public GarbageCollectedFinalized<SVGDocumentExtensions> {
  WTF_MAKE_NONCOPYABLE(SVGDocumentExtensions);

 public:
  explicit SVGDocumentExtensions(Document*);
  ~SVGDocumentExtensions();

  void addTimeContainer(SVGSVGElement*);
  void removeTimeContainer(SVGSVGElement*);

  // Records the SVG element as having a Web Animation on an SVG attribute that
  // needs applying.
  void addWebAnimationsPendingSVGElement(SVGElement&);

  static void serviceOnAnimationFrame(Document&);

  void startAnimations();
  void pauseAnimations();
  void dispatchSVGLoadEventToOutermostSVGElements();

  void reportError(const String&);

  SVGResourcesCache& resourcesCache() { return m_resourcesCache; }

  void addSVGRootWithRelativeLengthDescendents(SVGSVGElement*);
  void removeSVGRootWithRelativeLengthDescendents(SVGSVGElement*);
  bool isSVGRootWithRelativeLengthDescendents(SVGSVGElement*) const;
  void invalidateSVGRootsWithRelativeLengthDescendents(SubtreeLayoutScope*);

  bool zoomAndPanEnabled() const;

  void startPan(const FloatPoint& start);
  void updatePan(const FloatPoint& pos) const;

  static SVGSVGElement* rootElement(const Document&);
  SVGSVGElement* rootElement() const;

  DECLARE_TRACE();

 private:
  Member<Document> m_document;
  HeapHashSet<Member<SVGSVGElement>> m_timeContainers;
  using SVGElementSet = HeapHashSet<Member<SVGElement>>;
  SVGElementSet m_webAnimationsPendingSVGElements;
  SVGResourcesCache m_resourcesCache;
  // Root SVG elements with relative length descendants.
  HeapHashSet<Member<SVGSVGElement>> m_relativeLengthSVGRoots;
  FloatPoint m_translate;
#if DCHECK_IS_ON()
  bool m_inRelativeLengthSVGRootsInvalidation = false;
#endif

 public:
  void serviceAnimations();
};

}  // namespace blink

#endif
