/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "core/svg/SVGURIReference.h"

#include "core/dom/Document.h"
#include "core/dom/IdTargetObserver.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/svg/SVGElement.h"
#include "core/xlink_names.h"
#include "platform/weborigin/KURL.h"

namespace blink {

namespace {

class SVGElementReferenceObserver : public IdTargetObserver {
 public:
  SVGElementReferenceObserver(TreeScope& tree_scope,
                              const AtomicString& id,
                              WTF::Closure closure)
      : IdTargetObserver(tree_scope.GetIdTargetObserverRegistry(), id),
        closure_(std::move(closure)) {}

 private:
  void IdTargetChanged() override { closure_(); }
  WTF::Closure closure_;
};
}

SVGURIReference::SVGURIReference(SVGElement* element)
    : href_(SVGAnimatedHref::Create(element)) {
  DCHECK(element);
  href_->AddToPropertyMap(element);
}

void SVGURIReference::Trace(blink::Visitor* visitor) {
  visitor->Trace(href_);
}

bool SVGURIReference::IsKnownAttribute(const QualifiedName& attr_name) {
  return SVGAnimatedHref::IsKnownAttribute(attr_name);
}

const AtomicString& SVGURIReference::LegacyHrefString(
    const SVGElement& element) {
  if (element.hasAttribute(SVGNames::hrefAttr))
    return element.getAttribute(SVGNames::hrefAttr);
  return element.getAttribute(XLinkNames::hrefAttr);
}

KURL SVGURIReference::LegacyHrefURL(const Document& document) const {
  return document.CompleteURL(StripLeadingAndTrailingHTMLSpaces(HrefString()));
}

SVGURLReferenceResolver::SVGURLReferenceResolver(const String& url_string,
                                                 const Document& document)
    : relative_url_(url_string),
      document_(&document),
      is_local_(url_string.StartsWith('#')) {}

KURL SVGURLReferenceResolver::AbsoluteUrl() const {
  if (absolute_url_.IsNull())
    absolute_url_ = document_->CompleteURL(relative_url_);
  return absolute_url_;
}

bool SVGURLReferenceResolver::IsLocal() const {
  return is_local_ ||
         EqualIgnoringFragmentIdentifier(AbsoluteUrl(), document_->Url());
}

AtomicString SVGURLReferenceResolver::FragmentIdentifier() const {
  // If this is a "fragment-only" URL, then the reference is always local, so
  // just return what's after the '#' as the fragment.
  if (is_local_)
    return AtomicString(relative_url_.Substring(1));
  return AtomicString(AbsoluteUrl().FragmentIdentifier());
}

AtomicString SVGURIReference::FragmentIdentifierFromIRIString(
    const String& url_string,
    const TreeScope& tree_scope) {
  SVGURLReferenceResolver resolver(url_string, tree_scope.GetDocument());
  if (!resolver.IsLocal())
    return g_empty_atom;
  return resolver.FragmentIdentifier();
}

Element* SVGURIReference::TargetElementFromIRIString(
    const String& url_string,
    const TreeScope& tree_scope,
    AtomicString* fragment_identifier) {
  AtomicString id = FragmentIdentifierFromIRIString(url_string, tree_scope);
  if (id.IsEmpty())
    return nullptr;
  if (fragment_identifier)
    *fragment_identifier = id;
  return tree_scope.getElementById(id);
}

Element* SVGURIReference::ObserveTarget(Member<IdTargetObserver>& observer,
                                        SVGElement& context_element) {
  return ObserveTarget(observer, context_element, HrefString());
}

Element* SVGURIReference::ObserveTarget(Member<IdTargetObserver>& observer,
                                        SVGElement& context_element,
                                        const String& href_string) {
  TreeScope& tree_scope = context_element.GetTreeScope();
  AtomicString id = FragmentIdentifierFromIRIString(href_string, tree_scope);
  return ObserveTarget(observer, tree_scope, id,
                       WTF::Bind(&SVGElement::BuildPendingResource,
                                 WrapWeakPersistent(&context_element)));
}

Element* SVGURIReference::ObserveTarget(Member<IdTargetObserver>& observer,
                                        TreeScope& tree_scope,
                                        const AtomicString& id,
                                        WTF::Closure closure) {
  DCHECK(!observer);
  if (id.IsEmpty())
    return nullptr;
  observer =
      new SVGElementReferenceObserver(tree_scope, id, std::move(closure));
  return tree_scope.getElementById(id);
}

void SVGURIReference::UnobserveTarget(Member<IdTargetObserver>& observer) {
  if (!observer)
    return;
  observer->Unregister();
  observer = nullptr;
}

}  // namespace blink
