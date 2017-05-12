/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2011 Apple Inc. All rights
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

#include "core/html/HTMLObjectElement.h"

#include "bindings/core/v8/ScriptEventListener.h"
#include "core/HTMLNames.h"
#include "core/dom/Attribute.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/TagCollection.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLMetaElement.h"
#include "core/html/HTMLParamElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/layout/api/LayoutEmbeddedItem.h"
#include "core/plugins/PluginView.h"
#include "platform/network/mime/MIMETypeRegistry.h"

namespace blink {

using namespace HTMLNames;

inline HTMLObjectElement::HTMLObjectElement(Document& document,
                                            bool created_by_parser)
    : HTMLPlugInElement(objectTag,
                        document,
                        created_by_parser,
                        kShouldNotPreferPlugInsForImages),
      use_fallback_content_(false) {}

inline HTMLObjectElement::~HTMLObjectElement() {}

HTMLObjectElement* HTMLObjectElement::Create(Document& document,
                                             bool created_by_parser) {
  HTMLObjectElement* element =
      new HTMLObjectElement(document, created_by_parser);
  element->EnsureUserAgentShadowRoot();
  return element;
}

DEFINE_TRACE(HTMLObjectElement) {
  ListedElement::Trace(visitor);
  HTMLPlugInElement::Trace(visitor);
}

LayoutPart* HTMLObjectElement::ExistingLayoutPart() const {
  // This will return 0 if the layoutObject is not a LayoutPart.
  return GetLayoutPart();
}

bool HTMLObjectElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == borderAttr)
    return true;
  return HTMLPlugInElement::IsPresentationAttribute(name);
}

void HTMLObjectElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableStylePropertySet* style) {
  if (name == borderAttr)
    ApplyBorderAttributeToStyle(value, style);
  else
    HTMLPlugInElement::CollectStyleForPresentationAttribute(name, value, style);
}

void HTMLObjectElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  if (name == formAttr) {
    FormAttributeChanged();
  } else if (name == typeAttr) {
    service_type_ = params.new_value.DeprecatedLower();
    size_t pos = service_type_.Find(";");
    if (pos != kNotFound)
      service_type_ = service_type_.Left(pos);
    // TODO(schenney): crbug.com/572908 What is the right thing to do here?
    // Should we suppress the reload stuff when a persistable widget-type is
    // specified?
    ReloadPluginOnAttributeChange(name);
    if (!GetLayoutObject())
      RequestPluginCreationWithoutLayoutObjectIfPossible();
  } else if (name == dataAttr) {
    url_ = StripLeadingAndTrailingHTMLSpaces(params.new_value);
    if (GetLayoutObject() && IsImageType()) {
      SetNeedsPluginUpdate(true);
      if (!image_loader_)
        image_loader_ = HTMLImageLoader::Create(this);
      image_loader_->UpdateFromElement(ImageLoader::kUpdateIgnorePreviousError);
    } else {
      ReloadPluginOnAttributeChange(name);
    }
  } else if (name == classidAttr) {
    class_id_ = params.new_value;
    ReloadPluginOnAttributeChange(name);
  } else {
    HTMLPlugInElement::ParseAttribute(params);
  }
}

static void MapDataParamToSrc(Vector<String>* param_names,
                              Vector<String>* param_values) {
  // Some plugins don't understand the "data" attribute of the OBJECT tag (i.e.
  // Real and WMP require "src" attribute).
  int src_index = -1, data_index = -1;
  for (unsigned i = 0; i < param_names->size(); ++i) {
    if (DeprecatedEqualIgnoringCase((*param_names)[i], "src"))
      src_index = i;
    else if (DeprecatedEqualIgnoringCase((*param_names)[i], "data"))
      data_index = i;
  }

  if (src_index == -1 && data_index != -1) {
    param_names->push_back("src");
    param_values->push_back((*param_values)[data_index]);
  }
}

// TODO(schenney): crbug.com/572908 This function should not deal with url or
// serviceType!
void HTMLObjectElement::ParametersForPlugin(Vector<String>& param_names,
                                            Vector<String>& param_values,
                                            String& url,
                                            String& service_type) {
  HashSet<StringImpl*, CaseFoldingHash> unique_param_names;
  String url_parameter;

  // Scan the PARAM children and store their name/value pairs.
  // Get the URL and type from the params if we don't already have them.
  for (HTMLParamElement* p = Traversal<HTMLParamElement>::FirstChild(*this); p;
       p = Traversal<HTMLParamElement>::NextSibling(*p)) {
    String name = p->GetName();
    if (name.IsEmpty())
      continue;

    unique_param_names.insert(name.Impl());
    param_names.push_back(p->GetName());
    param_values.push_back(p->Value());

    // TODO(schenney): crbug.com/572908 url adjustment does not belong in this
    // function.
    if (url.IsEmpty() && url_parameter.IsEmpty() &&
        (DeprecatedEqualIgnoringCase(name, "src") ||
         DeprecatedEqualIgnoringCase(name, "movie") ||
         DeprecatedEqualIgnoringCase(name, "code") ||
         DeprecatedEqualIgnoringCase(name, "url")))
      url_parameter = StripLeadingAndTrailingHTMLSpaces(p->Value());
    // TODO(schenney): crbug.com/572908 serviceType calculation does not belong
    // in this function.
    if (service_type.IsEmpty() && DeprecatedEqualIgnoringCase(name, "type")) {
      service_type = p->Value();
      size_t pos = service_type.Find(";");
      if (pos != kNotFound)
        service_type = service_type.Left(pos);
    }
  }

  // When OBJECT is used for an applet via Sun's Java plugin, the CODEBASE
  // attribute in the tag points to the Java plugin itself (an ActiveX
  // component) while the actual applet CODEBASE is in a PARAM tag. See
  // <http://java.sun.com/products/plugin/1.2/docs/tags.html>. This means we
  // have to explicitly suppress the tag's CODEBASE attribute if there is none
  // in a PARAM, else our Java plugin will misinterpret it. [4004531]
  String codebase;
  if (MIMETypeRegistry::IsJavaAppletMIMEType(service_type)) {
    codebase = "codebase";
    unique_param_names.insert(
        codebase.Impl());  // pretend we found it in a PARAM already
  }

  // Turn the attributes of the <object> element into arrays, but don't override
  // <param> values.
  AttributeCollection attributes = this->Attributes();
  for (const Attribute& attribute : attributes) {
    const AtomicString& name = attribute.GetName().LocalName();
    if (!unique_param_names.Contains(name.Impl())) {
      param_names.push_back(name.GetString());
      param_values.push_back(attribute.Value().GetString());
    }
  }

  MapDataParamToSrc(&param_names, &param_values);

  // HTML5 says that an object resource's URL is specified by the object's data
  // attribute, not by a param element. However, for compatibility, allow the
  // resource's URL to be given by a param named "src", "movie", "code" or "url"
  // if we know that resource points to a plugin.
  if (url.IsEmpty() && !url_parameter.IsEmpty()) {
    KURL completed_url = GetDocument().CompleteURL(url_parameter);
    bool use_fallback;
    if (ShouldUsePlugin(completed_url, service_type, false, use_fallback))
      url = url_parameter;
  }
}

bool HTMLObjectElement::HasFallbackContent() const {
  for (Node* child = firstChild(); child; child = child->nextSibling()) {
    // Ignore whitespace-only text, and <param> tags, any other content is
    // fallback content.
    if (child->IsTextNode()) {
      if (!ToText(child)->ContainsOnlyWhitespace())
        return true;
    } else if (!isHTMLParamElement(*child)) {
      return true;
    }
  }
  return false;
}

bool HTMLObjectElement::HasValidClassId() const {
  if (MIMETypeRegistry::IsJavaAppletMIMEType(service_type_) &&
      ClassId().StartsWith("java:", kTextCaseASCIIInsensitive))
    return true;

  // HTML5 says that fallback content should be rendered if a non-empty
  // classid is specified for which the UA can't find a suitable plugin.
  return ClassId().IsEmpty();
}

void HTMLObjectElement::ReloadPluginOnAttributeChange(
    const QualifiedName& name) {
  // Following,
  //   http://www.whatwg.org/specs/web-apps/current-work/#the-object-element
  //   (Enumerated list below "Whenever one of the following conditions occur:")
  //
  // the updating of certain attributes should bring about "redetermination"
  // of what the element contains.
  bool needs_invalidation;
  if (name == typeAttr) {
    needs_invalidation =
        !FastHasAttribute(classidAttr) && !FastHasAttribute(dataAttr);
  } else if (name == dataAttr) {
    needs_invalidation = !FastHasAttribute(classidAttr);
  } else if (name == classidAttr) {
    needs_invalidation = true;
  } else {
    NOTREACHED();
    needs_invalidation = false;
  }
  SetNeedsPluginUpdate(true);
  if (needs_invalidation)
    LazyReattachIfNeeded();
}

// TODO(schenney): crbug.com/572908 This should be unified with
// HTMLEmbedElement::updatePlugin and moved down into HTMLPluginElement.cpp
void HTMLObjectElement::UpdatePluginInternal() {
  DCHECK(!GetLayoutEmbeddedItem().ShowsUnavailablePluginIndicator());
  DCHECK(NeedsPluginUpdate());
  SetNeedsPluginUpdate(false);
  // TODO(schenney): crbug.com/572908 This should ASSERT
  // isFinishedParsingChildren() instead.
  if (!IsFinishedParsingChildren()) {
    DispatchErrorEvent();
    return;
  }

  // TODO(schenney): crbug.com/572908 I'm not sure it's ever possible to get
  // into updateWidget during a removal, but just in case we should avoid
  // loading the frame to prevent security bugs.
  if (!SubframeLoadingDisabler::CanLoadFrame(*this)) {
    DispatchErrorEvent();
    return;
  }

  String url = this->Url();
  String service_type = service_type_;

  // TODO(schenney): crbug.com/572908 These should be joined into a
  // PluginParameters class.
  Vector<String> param_names;
  Vector<String> param_values;
  ParametersForPlugin(param_names, param_values, url, service_type);

  // Note: url is modified above by parametersForPlugin.
  if (!AllowedToLoadFrameURL(url)) {
    DispatchErrorEvent();
    return;
  }

  // TODO(schenney): crbug.com/572908 Is it possible to get here without a
  // layoutObject now that we don't have beforeload events?
  if (!GetLayoutObject())
    return;

  // Overwrites the URL and MIME type of a Flash embed to use an HTML5 embed.
  KURL overriden_url =
      GetDocument().GetFrame()->Loader().Client()->OverrideFlashEmbedWithHTML(
          GetDocument().CompleteURL(url_));
  if (!overriden_url.IsEmpty()) {
    url = url_ = overriden_url.GetString();
    service_type = service_type_ = "text/html";
  }

  if (!HasValidClassId() ||
      !RequestObject(url, service_type, param_names, param_values)) {
    if (!url.IsEmpty())
      DispatchErrorEvent();
    if (HasFallbackContent())
      RenderFallbackContent();
  } else {
    if (IsErrorplaceholder())
      DispatchErrorEvent();
  }
}

Node::InsertionNotificationRequest HTMLObjectElement::InsertedInto(
    ContainerNode* insertion_point) {
  HTMLPlugInElement::InsertedInto(insertion_point);
  ListedElement::InsertedInto(insertion_point);
  return kInsertionDone;
}

void HTMLObjectElement::RemovedFrom(ContainerNode* insertion_point) {
  HTMLPlugInElement::RemovedFrom(insertion_point);
  ListedElement::RemovedFrom(insertion_point);
}

void HTMLObjectElement::ChildrenChanged(const ChildrenChange& change) {
  if (isConnected() && !UseFallbackContent()) {
    SetNeedsPluginUpdate(true);
    LazyReattachIfNeeded();
  }
  HTMLPlugInElement::ChildrenChanged(change);
}

bool HTMLObjectElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == codebaseAttr ||
         attribute.GetName() == dataAttr ||
         (attribute.GetName() == usemapAttr && attribute.Value()[0] != '#') ||
         HTMLPlugInElement::IsURLAttribute(attribute);
}

bool HTMLObjectElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return name == classidAttr || name == dataAttr || name == codebaseAttr ||
         HTMLPlugInElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLObjectElement::SubResourceAttributeName() const {
  return dataAttr;
}

const AtomicString HTMLObjectElement::ImageSourceURL() const {
  return getAttribute(dataAttr);
}

// TODO(schenney): crbug.com/572908 Remove this hack.
void HTMLObjectElement::ReattachFallbackContent() {
  // This can happen inside of attachLayoutTree() in the middle of a recalcStyle
  // so we need to reattach synchronously here.
  if (GetDocument().InStyleRecalc())
    ReattachLayoutTree();
  else
    LazyReattachIfAttached();
}

void HTMLObjectElement::RenderFallbackContent() {
  if (UseFallbackContent())
    return;

  if (!isConnected())
    return;

  // Before we give up and use fallback content, check to see if this is a MIME
  // type issue.
  if (image_loader_ && image_loader_->GetImage() &&
      image_loader_->GetImage()->GetContentStatus() !=
          ResourceStatus::kLoadError) {
    service_type_ = image_loader_->GetImage()->GetResponse().MimeType();
    if (!IsImageType()) {
      // If we don't think we have an image type anymore, then clear the image
      // from the loader.
      image_loader_->ClearImage();
      ReattachFallbackContent();
      return;
    }
  }

  use_fallback_content_ = true;

  // TODO(schenney): crbug.com/572908 Style gets recalculated which is
  // suboptimal.
  ReattachFallbackContent();
}

bool HTMLObjectElement::IsExposed() const {
  // http://www.whatwg.org/specs/web-apps/current-work/#exposed
  for (HTMLObjectElement* ancestor =
           Traversal<HTMLObjectElement>::FirstAncestor(*this);
       ancestor;
       ancestor = Traversal<HTMLObjectElement>::FirstAncestor(*ancestor)) {
    if (ancestor->IsExposed())
      return false;
  }
  for (HTMLElement& element : Traversal<HTMLElement>::DescendantsOf(*this)) {
    if (isHTMLObjectElement(element) || isHTMLEmbedElement(element))
      return false;
  }
  return true;
}

bool HTMLObjectElement::ContainsJavaApplet() const {
  if (MIMETypeRegistry::IsJavaAppletMIMEType(getAttribute(typeAttr)))
    return true;

  for (HTMLElement& child : Traversal<HTMLElement>::ChildrenOf(*this)) {
    if (isHTMLParamElement(child) &&
        DeprecatedEqualIgnoringCase(child.GetNameAttribute(), "type") &&
        MIMETypeRegistry::IsJavaAppletMIMEType(
            child.getAttribute(valueAttr).GetString()))
      return true;
    if (isHTMLObjectElement(child) &&
        toHTMLObjectElement(child).ContainsJavaApplet())
      return true;
  }

  return false;
}

void HTMLObjectElement::DidMoveToNewDocument(Document& old_document) {
  ListedElement::DidMoveToNewDocument(old_document);
  HTMLPlugInElement::DidMoveToNewDocument(old_document);
}

HTMLFormElement* HTMLObjectElement::formOwner() const {
  return ListedElement::Form();
}

bool HTMLObjectElement::IsInteractiveContent() const {
  return FastHasAttribute(usemapAttr);
}

bool HTMLObjectElement::UseFallbackContent() const {
  return HTMLPlugInElement::UseFallbackContent() || use_fallback_content_;
}

bool HTMLObjectElement::WillUseFallbackContentAtLayout() const {
  return !HasValidClassId() && HasFallbackContent();
}

void HTMLObjectElement::AssociateWith(HTMLFormElement* form) {
  AssociateByParser(form);
};

}  // namespace blink
