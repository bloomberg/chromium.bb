/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2007 Rob Buis <buis@kde.org>
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

#ifndef SVGScriptElement_h
#define SVGScriptElement_h

#include "core/SVGNames.h"
#include "core/dom/ScriptLoaderClient.h"
#include "core/svg/SVGAnimatedBoolean.h"
#include "core/svg/SVGAnimatedString.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGURIReference.h"

namespace blink {

class ScriptLoader;

class SVGScriptElement final
    : public SVGElement
    , public SVGURIReference
    , public ScriptLoaderClient {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<SVGScriptElement> create(Document&, bool wasInsertedByParser);

    ScriptLoader* loader() const { return m_loader.get(); }

#if ENABLE(ASSERT)
    virtual bool isAnimatableAttribute(const QualifiedName&) const override;
#endif

private:
    SVGScriptElement(Document&, bool wasInsertedByParser, bool alreadyStarted);
    virtual ~SVGScriptElement();

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) override;
    virtual void didNotifySubtreeInsertionsToDocument() override;
    virtual void childrenChanged(const ChildrenChange&) override;
    virtual void didMoveToNewDocument(Document& oldDocument) override;

    virtual void svgAttributeChanged(const QualifiedName&) override;
    virtual bool isURLAttribute(const Attribute&) const override;
    virtual bool isStructurallyExternal() const override { return hasSourceAttribute(); }
    virtual void finishParsingChildren() override;

    virtual bool haveLoadedRequiredResources() override;

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
    virtual bool rendererIsNeeded(const RenderStyle&) override { return false; }

    virtual Timer<SVGElement>* svgLoadEventTimer() override { return &m_svgLoadEventTimer; }


    Timer<SVGElement> m_svgLoadEventTimer;
    OwnPtr<ScriptLoader> m_loader;
};

} // namespace blink

#endif // SVGScriptElement_h
