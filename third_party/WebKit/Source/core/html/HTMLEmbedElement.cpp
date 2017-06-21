/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2011 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "core/html/HTMLEmbedElement.h"

#include "core/CSSPropertyNames.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/LocalFrameClient.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLObjectElement.h"
#include "core/html/PluginDocument.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/LayoutEmbeddedContent.h"
#include "core/layout/api/LayoutEmbeddedItem.h"

namespace blink {

using namespace HTMLNames;

inline HTMLEmbedElement::HTMLEmbedElement(Document& document,
                                          bool created_by_parser)
    : HTMLPlugInElement(embedTag,
                        document,
                        created_by_parser,
                        kShouldPreferPlugInsForImages) {}

HTMLEmbedElement* HTMLEmbedElement::Create(Document& document,
                                           bool created_by_parser) {
  HTMLEmbedElement* element = new HTMLEmbedElement(document, created_by_parser);
  element->EnsureUserAgentShadowRoot();
  return element;
}

static inline LayoutEmbeddedContent* FindPartLayoutObject(const Node* n) {
  if (!n->GetLayoutObject())
    n = Traversal<HTMLObjectElement>::FirstAncestor(*n);

  if (n && n->GetLayoutObject() &&
      n->GetLayoutObject()->IsLayoutEmbeddedContent())
    return ToLayoutEmbeddedContent(n->GetLayoutObject());

  return nullptr;
}

LayoutEmbeddedContent* HTMLEmbedElement::ExistingLayoutEmbeddedContent() const {
  return FindPartLayoutObject(this);
}

bool HTMLEmbedElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == hiddenAttr)
    return true;
  return HTMLPlugInElement::IsPresentationAttribute(name);
}

void HTMLEmbedElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == hiddenAttr) {
    if (DeprecatedEqualIgnoringCase(value, "yes") ||
        DeprecatedEqualIgnoringCase(value, "true")) {
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyWidth, 0, CSSPrimitiveValue::UnitType::kPixels);
      AddPropertyToPresentationAttributeStyle(
          style, CSSPropertyHeight, 0, CSSPrimitiveValue::UnitType::kPixels);
    }
  } else {
    HTMLPlugInElement::CollectStyleForPresentationAttribute(name, value, style);
  }
}

void HTMLEmbedElement::ParseAttribute(
    const AttributeModificationParams& params) {
  if (params.name == typeAttr) {
    service_type_ = params.new_value.DeprecatedLower();
    size_t pos = service_type_.Find(";");
    if (pos != kNotFound)
      service_type_ = service_type_.Left(pos);
    if (GetLayoutObject()) {
      SetNeedsPluginUpdate(true);
      GetLayoutObject()->SetNeedsLayoutAndFullPaintInvalidation(
          "Embed type changed");
    } else {
      RequestPluginCreationWithoutLayoutObjectIfPossible();
    }
  } else if (params.name == codeAttr) {
    // TODO(schenney): Remove this branch? It's not in the spec and we're not in
    // the HTMLAppletElement hierarchy.
    url_ = StripLeadingAndTrailingHTMLSpaces(params.new_value);
  } else if (params.name == srcAttr) {
    url_ = StripLeadingAndTrailingHTMLSpaces(params.new_value);
    if (GetLayoutObject() && IsImageType()) {
      if (!image_loader_)
        image_loader_ = HTMLImageLoader::Create(this);
      image_loader_->UpdateFromElement(ImageLoader::kUpdateIgnorePreviousError);
    } else if (GetLayoutObject()) {
      // Check if this Embed can transition from potentially-active to active
      if (FastHasAttribute(typeAttr)) {
        SetNeedsPluginUpdate(true);
        LazyReattachIfNeeded();
      }
    } else {
      RequestPluginCreationWithoutLayoutObjectIfPossible();
    }
  } else {
    HTMLPlugInElement::ParseAttribute(params);
  }
}

void HTMLEmbedElement::ParametersForPlugin(Vector<String>& param_names,
                                           Vector<String>& param_values) {
  AttributeCollection attributes = this->Attributes();
  for (const Attribute& attribute : attributes) {
    param_names.push_back(attribute.LocalName().GetString());
    param_values.push_back(attribute.Value().GetString());
  }
}

// FIXME: This should be unified with HTMLObjectElement::updatePlugin and
// moved down into HTMLPluginElement.cpp
void HTMLEmbedElement::UpdatePluginInternal() {
  DCHECK(!GetLayoutEmbeddedItem().ShowsUnavailablePluginIndicator());
  DCHECK(NeedsPluginUpdate());
  SetNeedsPluginUpdate(false);

  if (url_.IsEmpty() && service_type_.IsEmpty())
    return;

  // Note these pass m_url and m_serviceType to allow better code sharing with
  // <object> which modifies url and serviceType before calling these.
  if (!AllowedToLoadFrameURL(url_))
    return;

  // FIXME: These should be joined into a PluginParameters class.
  Vector<String> param_names;
  Vector<String> param_values;
  ParametersForPlugin(param_names, param_values);

  // FIXME: Can we not have layoutObject here now that beforeload events are
  // gone?
  if (!GetLayoutObject())
    return;

  // Overwrites the URL and MIME type of a Flash embed to use an HTML5 embed.
  KURL overriden_url =
      GetDocument().GetFrame()->Loader().Client()->OverrideFlashEmbedWithHTML(
          GetDocument().CompleteURL(url_));
  if (!overriden_url.IsEmpty()) {
    url_ = overriden_url.GetString();
    service_type_ = "text/html";
  }

  RequestObject(param_names, param_values);
}

bool HTMLEmbedElement::LayoutObjectIsNeeded(const ComputedStyle& style) {
  if (IsImageType())
    return HTMLPlugInElement::LayoutObjectIsNeeded(style);

  // https://html.spec.whatwg.org/multipage/embedded-content.html#the-embed-element
  // While any of the following conditions are occurring, any plugin
  // instantiated for the element must be removed, and the embed element
  // represents nothing:

  // * The element has neither a src attribute nor a type attribute.
  if (!FastHasAttribute(srcAttr) && !FastHasAttribute(typeAttr))
    return false;

  // * The element has a media element ancestor.
  // -> It's realized by LayoutMedia::isChildAllowed.

  // * The element has an ancestor object element that is not showing its
  //   fallback content.
  ContainerNode* p = parentNode();
  if (isHTMLObjectElement(p)) {
    DCHECK(p->GetLayoutObject());
    if (!toHTMLObjectElement(p)->WillUseFallbackContentAtLayout() &&
        !toHTMLObjectElement(p)->UseFallbackContent()) {
      DCHECK(!p->GetLayoutObject()->IsEmbeddedObject());
      return false;
    }
  }
  return HTMLPlugInElement::LayoutObjectIsNeeded(style);
}

bool HTMLEmbedElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == srcAttr ||
         HTMLPlugInElement::IsURLAttribute(attribute);
}

const QualifiedName& HTMLEmbedElement::SubResourceAttributeName() const {
  return srcAttr;
}

bool HTMLEmbedElement::IsInteractiveContent() const {
  return true;
}

bool HTMLEmbedElement::IsExposed() const {
  // http://www.whatwg.org/specs/web-apps/current-work/#exposed
  for (HTMLObjectElement* object =
           Traversal<HTMLObjectElement>::FirstAncestor(*this);
       object; object = Traversal<HTMLObjectElement>::FirstAncestor(*object)) {
    if (object->IsExposed())
      return false;
  }
  return true;
}

}  // namespace blink
