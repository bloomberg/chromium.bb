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

#include "core/html/FormAssociatedElement.h"
#include "core/html/HTMLPlugInImageElement.h"

namespace WebCore {

class HTMLFormElement;

class HTMLObjectElement FINAL : public HTMLPlugInImageElement, public FormAssociatedElement {
public:
    static PassRefPtr<HTMLObjectElement> create(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);
    virtual ~HTMLObjectElement();

    bool isDocNamedItem() const { return m_docNamedItem; }

    const String& classId() const { return m_classId; }

    bool containsJavaApplet() const;

    virtual bool useFallbackContent() const { return m_useFallbackContent; }
    virtual void renderFallbackContent() OVERRIDE;

    // Implementations of FormAssociatedElement
    HTMLFormElement* form() const { return FormAssociatedElement::form(); }

    virtual bool isFormControlElement() const { return false; }

    virtual bool isEnumeratable() const { return true; }
    virtual bool appendFormData(FormDataList&, bool);

    virtual bool isObjectElement() const OVERRIDE { return true; }

    // Implementations of constraint validation API.
    // Note that the object elements are always barred from constraint validation.
    virtual String validationMessage() const OVERRIDE { return String(); }
    bool checkValidity() { return true; }
    virtual void setCustomValidity(const String&) OVERRIDE { }

    using Node::ref;
    using Node::deref;

    virtual bool canContainRangeEndPoint() const { return useFallbackContent(); }

private:
    HTMLObjectElement(const QualifiedName&, Document&, HTMLFormElement*, bool createdByParser);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;
    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;

    virtual bool rendererIsNeeded(const RenderStyle&);
    virtual void didMoveToNewDocument(Document* oldDocument) OVERRIDE;

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual bool isURLAttribute(const Attribute&) const OVERRIDE;
    virtual const AtomicString imageSourceURL() const OVERRIDE;

    virtual RenderWidget* existingRenderWidget() const OVERRIDE;

    virtual void addSubresourceAttributeURLs(ListHashSet<KURL>&) const;

    virtual void updateWidget(PluginCreationOption);
    void updateDocNamedItem();

    void reattachFallbackContent();

    bool hasFallbackContent() const;

    // FIXME: This function should not deal with url or serviceType
    // so that we can better share code between <object> and <embed>.
    void parametersForPlugin(Vector<String>& paramNames, Vector<String>& paramValues, String& url, String& serviceType);

    bool shouldAllowQuickTimeClassIdQuirk();
    bool hasValidClassId();

    virtual void refFormAssociatedElement() { ref(); }
    virtual void derefFormAssociatedElement() { deref(); }
    virtual HTMLFormElement* virtualForm() const;

    virtual bool shouldRegisterAsNamedItem() const OVERRIDE { return isDocNamedItem(); }
    virtual bool shouldRegisterAsExtraNamedItem() const OVERRIDE { return isDocNamedItem(); }

    String m_classId;
    bool m_docNamedItem : 1;
    bool m_useFallbackContent : 1;
};

inline HTMLObjectElement* toHTMLObjectElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->hasTagName(HTMLNames::objectTag));
    return static_cast<HTMLObjectElement*>(node);
}

inline const HTMLObjectElement* toHTMLObjectElement(const FormAssociatedElement* element)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!element || !element->isFormControlElement());
    const HTMLObjectElement* objectElement = static_cast<const HTMLObjectElement*>(element);
    // We need to assert after the cast because FormAssociatedElement doesn't
    // have hasTagName.
    ASSERT_WITH_SECURITY_IMPLICATION(!objectElement || objectElement->hasTagName(HTMLNames::objectTag));
    return objectElement;
}

}

#endif
