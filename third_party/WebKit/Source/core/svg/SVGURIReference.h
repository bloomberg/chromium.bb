/*
 * Copyright (C) 2004, 2005, 2008, 2009 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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

#ifndef SVGURIReference_h
#define SVGURIReference_h

#include <memory>
#include "core/CoreExport.h"
#include "core/svg/SVGAnimatedHref.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Functional.h"

namespace blink {

class Document;
class Element;
class IdTargetObserver;

class CORE_EXPORT SVGURIReference : public GarbageCollectedMixin {
 public:
  virtual ~SVGURIReference() {}

  bool IsKnownAttribute(const QualifiedName&);

  // Use this for accesses to 'href' or 'xlink:href' (in that order) for
  // elements where both are allowed and don't necessarily inherit from
  // SVGURIReference.
  static const AtomicString& LegacyHrefString(const SVGElement&);

  // Like above, but for elements that inherit from SVGURIReference. Resolves
  // against the base URL of the passed Document.
  KURL LegacyHrefURL(const Document&) const;

  static AtomicString FragmentIdentifierFromIRIString(const String&,
                                                      const TreeScope&);
  static Element* TargetElementFromIRIString(const String&,
                                             const TreeScope&,
                                             AtomicString* = nullptr);

  const String& HrefString() const { return href_->CurrentValue()->Value(); }

  // Create an 'id' observer for the href associated with this SVGURIReference
  // and its corresponding SVGElement (which should be passed as
  // |contextElement|.) Will call buildPendingResource() on |contextElement|
  // when changes to the 'id' are noticed.
  Element* ObserveTarget(Member<IdTargetObserver>&, SVGElement&);
  // Create an 'id' observer for any id denoted by |hrefString|, calling
  // buildPendingResource() on |contextElement| on changes.
  static Element* ObserveTarget(Member<IdTargetObserver>&,
                                SVGElement&,
                                const String& href_string);
  // Create an 'id' observer for |id| in the specified TreeScope. On changes,
  // the passed Closure will be called.
  static Element* ObserveTarget(Member<IdTargetObserver>&,
                                TreeScope&,
                                const AtomicString& id,
                                WTF::Closure);
  // Unregister and destroy the observer.
  static void UnobserveTarget(Member<IdTargetObserver>&);

  // JS API
  SVGAnimatedHref* href() const { return href_.Get(); }

  virtual void Trace(blink::Visitor*);

 protected:
  explicit SVGURIReference(SVGElement*);

 private:
  Member<SVGAnimatedHref> href_;
};

// Helper class used to resolve fragment references. Handles the 'local url
// flag' per https://drafts.csswg.org/css-values/#local-urls .
class SVGURLReferenceResolver {
  STACK_ALLOCATED();

 public:
  SVGURLReferenceResolver(const String& url_string, const Document&);

  bool IsLocal() const;
  KURL AbsoluteUrl() const;
  AtomicString FragmentIdentifier() const;

 private:
  const String& relative_url_;
  Member<const Document> document_;
  mutable KURL absolute_url_;
  bool is_local_;
};

}  // namespace blink

#endif  // SVGURIReference_h
