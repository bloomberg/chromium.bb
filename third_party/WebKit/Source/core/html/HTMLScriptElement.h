/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef HTMLScriptElement_h
#define HTMLScriptElement_h

#include "core/dom/ScriptLoaderClient.h"
#include "core/html/HTMLElement.h"

namespace blink {

class ScriptLoader;

class HTMLScriptElement final : public HTMLElement, public ScriptLoaderClient {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<HTMLScriptElement> create(Document&, bool wasInsertedByParser, bool alreadyStarted = false);

    String text() { return textFromChildren(); }
    void setText(const String&);

    KURL src() const;

    void setAsync(bool);
    bool async() const;

    ScriptLoader* loader() const { return m_loader.get(); }

private:
    HTMLScriptElement(Document&, bool wasInsertedByParser, bool alreadyStarted);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual void attributeWillChange(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue) override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) override;
    virtual void didNotifySubtreeInsertionsToDocument() override;
    virtual void childrenChanged(const ChildrenChange&) override;
    virtual void didMoveToNewDocument(Document& oldDocument) override;

    virtual bool isURLAttribute(const Attribute&) const override;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const override;
    virtual const QualifiedName& subResourceAttributeName() const override;

    virtual String sourceAttributeValue() const override;
    virtual String charsetAttributeValue() const override;
    virtual String typeAttributeValue() const override;
    virtual String languageAttributeValue() const override;
    virtual String forAttributeValue() const override;
    virtual String eventAttributeValue() const override;
    virtual bool asyncAttributeValue() const override;
    virtual bool deferAttributeValue() const override;
    virtual bool hasSourceAttribute() const override;

    virtual void dispatchLoadEvent() override;

    virtual PassRefPtrWillBeRawPtr<Element> cloneElementWithoutAttributesAndChildren() override;

    OwnPtr<ScriptLoader> m_loader;
};

} // namespace blink

#endif // HTMLScriptElement_h
