/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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
 *
 */

#ifndef HTMLObjectElement_h
#define HTMLObjectElement_h

#include "core/CoreExport.h"
#include "core/html/FormAssociated.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/html/ListedElement.h"

namespace blink {

class HTMLFormElement;

// Inheritance of ListedElement was used for NPAPI form association, but
// is still kept here so that legacy APIs such as form attribute can keep
// working according to the spec.  See:
// https://html.spec.whatwg.org/multipage/embedded-content.html#the-object-element
class CORE_EXPORT HTMLObjectElement final : public HTMLPlugInElement,
                                            public ListedElement,
                                            public FormAssociated {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(HTMLObjectElement);

 public:
  static HTMLObjectElement* Create(Document&, bool created_by_parser);
  ~HTMLObjectElement() override;
  DECLARE_VIRTUAL_TRACE();

  const String& ClassId() const { return class_id_; }

  HTMLFormElement* formOwner() const override;

  bool ContainsJavaApplet() const;

  bool HasFallbackContent() const override;
  bool UseFallbackContent() const override;
  bool CanRenderFallbackContent() const override { return true; }
  void RenderFallbackContent() override;

  bool IsFormControlElement() const override { return false; }

  bool IsEnumeratable() const override { return true; }
  bool IsInteractiveContent() const override;

  // Implementations of constraint validation API.
  // Note that the object elements are always barred from constraint validation.
  String validationMessage() const override { return String(); }
  bool checkValidity() { return true; }
  bool reportValidity() { return true; }
  void setCustomValidity(const String&) override {}

  bool CanContainRangeEndPoint() const override { return UseFallbackContent(); }

  bool IsExposed() const;

  bool WillUseFallbackContentAtLayout() const;

  FormAssociated* ToFormAssociatedOrNull() override { return this; };
  void AssociateWith(HTMLFormElement*) override;

 private:
  HTMLObjectElement(Document&, bool created_by_parser);

  void ParseAttribute(const AttributeModificationParams&) override;
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(const QualifiedName&,
                                            const AtomicString&,
                                            MutableStylePropertySet*) override;

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;

  void DidMoveToNewDocument(Document& old_document) override;

  void ChildrenChanged(const ChildrenChange&) override;

  bool IsURLAttribute(const Attribute&) const override;
  bool HasLegalLinkAttribute(const QualifiedName&) const override;
  const QualifiedName& SubResourceAttributeName() const override;
  const AtomicString ImageSourceURL() const override;

  LayoutEmbeddedContent* ExistingLayoutEmbeddedContent() const override;

  void UpdatePluginInternal() override;
  void UpdateDocNamedItem();

  void ReattachFallbackContent();

  // FIXME: This function should not deal with url or serviceType
  // so that we can better share code between <object> and <embed>.
  void ParametersForPlugin(Vector<String>& param_names,
                           Vector<String>& param_values,
                           String& url,
                           String& service_type);

  bool HasValidClassId() const;

  void ReloadPluginOnAttributeChange(const QualifiedName&);

  bool ShouldRegisterAsNamedItem() const override { return true; }
  bool ShouldRegisterAsExtraNamedItem() const override { return true; }

  String class_id_;
  bool use_fallback_content_ : 1;
};

// Intentionally left unimplemented, template specialization needs to be
// provided for specific return types.
template <typename T>
inline const T& ToElement(const ListedElement&);
template <typename T>
inline const T* ToElement(const ListedElement*);

// Make toHTMLObjectElement() accept a ListedElement as input instead of
// a Node.
template <>
inline const HTMLObjectElement* ToElement<HTMLObjectElement>(
    const ListedElement* element) {
  SECURITY_DCHECK(!element || !element->IsFormControlElement());
  const HTMLObjectElement* object_element =
      static_cast<const HTMLObjectElement*>(element);
  // We need to assert after the cast because ListedElement doesn't
  // have hasTagName.
  SECURITY_DCHECK(!object_element ||
                  object_element->HasTagName(HTMLNames::objectTag));
  return object_element;
}

template <>
inline const HTMLObjectElement& ToElement<HTMLObjectElement>(
    const ListedElement& element) {
  SECURITY_DCHECK(!element.IsFormControlElement());
  const HTMLObjectElement& object_element =
      static_cast<const HTMLObjectElement&>(element);
  // We need to assert after the cast because ListedElement doesn't
  // have hasTagName.
  SECURITY_DCHECK(object_element.HasTagName(HTMLNames::objectTag));
  return object_element;
}

}  // namespace blink

#endif  // HTMLObjectElement_h
