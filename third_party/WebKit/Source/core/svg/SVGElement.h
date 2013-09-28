/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef SVGElement_h
#define SVGElement_h

#include "core/dom/Element.h"
#include "core/platform/Timer.h"
#include "core/svg/SVGAnimatedString.h"
#include "core/svg/SVGLangSpace.h"
#include "core/svg/SVGLocatable.h"
#include "core/svg/SVGParsingError.h"
#include "core/svg/properties/SVGAnimatedPropertyMacros.h"
#include "core/svg/properties/SVGPropertyInfo.h"
#include "wtf/HashMap.h"

namespace WebCore {

class AffineTransform;
class CSSCursorImageValue;
class Document;
class SubtreeLayoutScope;
class SVGAttributeToPropertyMap;
class SVGCursorElement;
class SVGDocumentExtensions;
class SVGElementInstance;
class SVGElementRareData;
class SVGSVGElement;

void mapAttributeToCSSProperty(HashMap<StringImpl*, CSSPropertyID>* propertyNameToIdMap, const QualifiedName& attrName);

class SVGElement : public Element, public SVGLangSpace {
public:
    virtual ~SVGElement();

    bool isOutermostSVGSVGElement() const;

    virtual String title() const;
    bool hasRelativeLengths() const { return !m_elementsWithRelativeLengths.isEmpty(); }
    virtual bool supportsMarkers() const { return false; }
    PassRefPtr<CSSValue> getPresentationAttribute(const String& name);
    bool isKnownAttribute(const QualifiedName&);
    static bool isAnimatableCSSProperty(const QualifiedName&);
    virtual AffineTransform localCoordinateSpaceTransform(SVGLocatable::CTMScope) const;
    virtual bool needsPendingResourceHandling() const { return true; }

    bool instanceUpdatesBlocked() const;
    void setInstanceUpdatesBlocked(bool);

    String xmlbase() const;
    void setXmlbase(const String&);

    SVGSVGElement* ownerSVGElement() const;
    SVGElement* viewportElement() const;

    SVGDocumentExtensions* accessDocumentSVGExtensions();

    virtual bool isSVGGraphicsElement() const { return false; }
    virtual bool isSVGSVGElement() const { return false; }
    virtual bool isFilterEffect() const { return false; }
    virtual bool isGradientStop() const { return false; }
    virtual bool isTextContent() const { return false; }

    // For SVGTests
    virtual bool isValid() const { return true; }

    virtual void svgAttributeChanged(const QualifiedName&);

    virtual void animatedPropertyTypeForAttribute(const QualifiedName&, Vector<AnimatedPropertyType>&);

    void sendSVGLoadEventIfPossible(bool sendParentLoadEvents = false);
    void sendSVGLoadEventIfPossibleAsynchronously();
    void svgLoadEventTimerFired(Timer<SVGElement>*);
    virtual Timer<SVGElement>* svgLoadEventTimer();

    virtual AffineTransform* supplementalTransform() { return 0; }

    void invalidateSVGAttributes() { ensureUniqueElementData()->m_animatedSVGAttributesAreDirty = true; }

    const HashSet<SVGElementInstance*>& instancesForElement() const;

    bool getBoundingBox(FloatRect&, SVGLocatable::StyleUpdateStrategy = SVGLocatable::AllowStyleUpdate);

    void setCursorElement(SVGCursorElement*);
    void cursorElementRemoved();
    void setCursorImageValue(CSSCursorImageValue*);
    void cursorImageValueRemoved();

    SVGElement* correspondingElement();
    void setCorrespondingElement(SVGElement*);

    void synchronizeAnimatedSVGAttribute(const QualifiedName&) const;

    virtual PassRefPtr<RenderStyle> customStyleForRenderer() OVERRIDE;

    static void synchronizeRequiredFeatures(SVGElement* contextElement);
    static void synchronizeRequiredExtensions(SVGElement* contextElement);
    static void synchronizeSystemLanguage(SVGElement* contextElement);

    virtual void synchronizeRequiredFeatures() { }
    virtual void synchronizeRequiredExtensions() { }
    virtual void synchronizeSystemLanguage() { }

#ifndef NDEBUG
    bool isAnimatableAttribute(const QualifiedName&) const;
#endif

    MutableStylePropertySet* animatedSMILStyleProperties() const;
    MutableStylePropertySet* ensureAnimatedSMILStyleProperties();
    void setUseOverrideComputedStyle(bool);

    virtual bool haveLoadedRequiredResources();

    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture) OVERRIDE;
    virtual bool removeEventListener(const AtomicString& eventType, EventListener*, bool useCapture) OVERRIDE;

    virtual bool shouldMoveToFlowThread(RenderStyle*) const OVERRIDE;

    void invalidateRelativeLengthClients(SubtreeLayoutScope* = 0);

protected:
    SVGElement(const QualifiedName&, Document&, ConstructionType = CreateSVGElement);

    virtual void parseAttribute(const QualifiedName&, const AtomicString&) OVERRIDE;

    virtual void finishParsingChildren();
    virtual void attributeChanged(const QualifiedName&, const AtomicString&, AttributeModificationReason = ModifiedDirectly) OVERRIDE;
    virtual bool childShouldCreateRenderer(const Node& child) const OVERRIDE;

    virtual bool isPresentationAttribute(const QualifiedName&) const OVERRIDE;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) OVERRIDE;
    virtual bool rendererIsNeeded(const RenderStyle&) OVERRIDE;

    virtual InsertionNotificationRequest insertedInto(ContainerNode*) OVERRIDE;
    virtual void removedFrom(ContainerNode*) OVERRIDE;
    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    static CSSPropertyID cssPropertyIdForSVGAttributeName(const QualifiedName&);
    void updateRelativeLengthsInformation() { updateRelativeLengthsInformation(selfHasRelativeLengths(), this); }
    void updateRelativeLengthsInformation(bool hasRelativeLengths, SVGElement*);

    virtual bool selfHasRelativeLengths() const { return false; }

    SVGElementRareData* svgRareData() const;
    SVGElementRareData* ensureSVGRareData();

    void reportAttributeParsingError(SVGParsingError, const QualifiedName&, const AtomicString&);
    bool hasFocusEventListeners() const;

private:
    friend class SVGElementInstance;

    // FIXME: Author shadows should be allowed
    // https://bugs.webkit.org/show_bug.cgi?id=77938
    virtual bool areAuthorShadowsAllowed() const OVERRIDE { return false; }

    RenderStyle* computedStyle(PseudoId = NOPSEUDO);
    virtual RenderStyle* virtualComputedStyle(PseudoId pseudoElementSpecifier = NOPSEUDO) { return computedStyle(pseudoElementSpecifier); }
    virtual void willRecalcStyle(StyleRecalcChange) OVERRIDE;
    virtual bool isKeyboardFocusable() const OVERRIDE;

    void buildPendingResourcesIfNeeded();

    virtual bool isSupported(StringImpl* feature, StringImpl* version) const;

    void mapInstanceToElement(SVGElementInstance*);
    void removeInstanceMapping(SVGElementInstance*);

    HashSet<SVGElement*> m_elementsWithRelativeLengths;

    BEGIN_DECLARE_ANIMATED_PROPERTIES(SVGElement)
        DECLARE_ANIMATED_STRING(ClassName, className)
    END_DECLARE_ANIMATED_PROPERTIES

#if !ASSERT_DISABLED
    bool m_inRelativeLengthClientsInvalidation;
#endif
};

struct SVGAttributeHashTranslator {
    static unsigned hash(const QualifiedName& key)
    {
        if (key.hasPrefix()) {
            QualifiedNameComponents components = { nullAtom.impl(), key.localName().impl(), key.namespaceURI().impl() };
            return hashComponents(components);
        }
        return DefaultHash<QualifiedName>::Hash::hash(key);
    }
    static bool equal(const QualifiedName& a, const QualifiedName& b) { return a.matches(b); }
};

inline SVGElement* toSVGElement(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isSVGElement());
    return static_cast<SVGElement*>(node);
}

inline const SVGElement* toSVGElement(const Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isSVGElement());
    return static_cast<const SVGElement*>(node);
}

}

#endif
