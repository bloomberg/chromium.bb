/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 * Copyright (C) 2009 Cameron McCormack <cam@mcc.id.au>
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

#include "config.h"

#include "core/svg/SVGElement.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include "XMLNames.h"
#include "bindings/v8/ScriptEventListener.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/parser/BisonCSSParser.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/events/Event.h"
#include "core/html/HTMLElement.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/svg/RenderSVGResourceContainer.h"
#include "core/svg/SVGCursorElement.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGElementRareData.h"
#include "core/svg/SVGGraphicsElement.h"
#include "core/svg/SVGSVGElement.h"
#include "core/svg/SVGTitleElement.h"
#include "core/svg/SVGUseElement.h"

#include "wtf/TemporaryChange.h"

namespace WebCore {

using namespace HTMLNames;
using namespace SVGNames;

void mapAttributeToCSSProperty(HashMap<StringImpl*, CSSPropertyID>* propertyNameToIdMap, const QualifiedName& attrName)
{
    // FIXME: when CSS supports "transform-origin" the special case for transform_originAttr can be removed.
    // FIXME: It's not clear the above is strictly true, as -webkit-transform-origin has non-standard behavior.
    CSSPropertyID propertyId = cssPropertyID(attrName.localName());
    if (!propertyId && attrName == transform_originAttr) {
        propertyId = CSSPropertyWebkitTransformOrigin; // cssPropertyID("-webkit-transform-origin")
    } else if (propertyId == CSSPropertyTransformOrigin) {
        propertyId = CSSPropertyWebkitTransformOrigin;
    }
    ASSERT(propertyId > 0);
    propertyNameToIdMap->set(attrName.localName().impl(), propertyId);
}

SVGElement::SVGElement(const QualifiedName& tagName, Document& document, ConstructionType constructionType)
    : Element(tagName, &document, constructionType)
#if !ASSERT_DISABLED
    , m_inRelativeLengthClientsInvalidation(false)
#endif
    // |m_isContextElement| must be initialized before |m_className|, as SVGAnimatedString tear-off c-tor currently set this to true.
    , m_isContextElement(false)
    , m_hasSVGRareData(false)
    , m_className(SVGAnimatedString::create(this, HTMLNames::classAttr, SVGString::create()))
{
    ScriptWrappable::init(this);
    addToPropertyMap(m_className);
    setHasCustomStyleCallbacks();
}

SVGElement::~SVGElement()
{
    ASSERT(inDocument() || !hasRelativeLengths());

    if (!hasSVGRareData())
        ASSERT(!SVGElementRareData::rareDataMap().contains(this));
    else {
        SVGElementRareData::SVGElementRareDataMap& rareDataMap = SVGElementRareData::rareDataMap();
        SVGElementRareData::SVGElementRareDataMap::iterator it = rareDataMap.find(this);
        ASSERT_WITH_SECURITY_IMPLICATION(it != rareDataMap.end());

        SVGElementRareData* rareData = it->value;
        rareData->destroyAnimatedSMILStyleProperties();
        if (SVGCursorElement* cursorElement = rareData->cursorElement())
            cursorElement->removeClient(this);
        if (CSSCursorImageValue* cursorImageValue = rareData->cursorImageValue())
            cursorImageValue->removeReferencedElement(this);

        delete rareData;

        // The rare data cleanup may have caused other SVG nodes to be deleted,
        // modifying the rare data map. Do not rely on the existing iterator.
        ASSERT(rareDataMap.contains(this));
        rareDataMap.remove(this);
        // Clear HasSVGRareData flag now so that we are in a consistent state when
        // calling rebuildAllElementReferencesForTarget() and
        // removeAllElementReferencesForTarget() below.
        clearHasSVGRareData();
    }
    document().accessSVGExtensions().rebuildAllElementReferencesForTarget(this);
    document().accessSVGExtensions().removeAllElementReferencesForTarget(this);
}

void SVGElement::willRecalcStyle(StyleRecalcChange change)
{
    if (!hasSVGRareData())
        return;
    // If the style changes because of a regular property change (not induced by SMIL animations themselves)
    // reset the "computed style without SMIL style properties", so the base value change gets reflected.
    if (change > NoChange || needsStyleRecalc())
        svgRareData()->setNeedsOverrideComputedStyleUpdate();
}

void SVGElement::buildPendingResourcesIfNeeded()
{
    Document& document = this->document();
    if (!needsPendingResourceHandling() || !inDocument() || isInShadowTree())
        return;

    SVGDocumentExtensions& extensions = document.accessSVGExtensions();
    AtomicString resourceId = getIdAttribute();
    if (!extensions.hasPendingResource(resourceId))
        return;

    // Mark pending resources as pending for removal.
    extensions.markPendingResourcesForRemoval(resourceId);

    // Rebuild pending resources for each client of a pending resource that is being removed.
    while (Element* clientElement = extensions.removeElementFromPendingResourcesForRemoval(resourceId)) {
        ASSERT(clientElement->hasPendingResources());
        if (clientElement->hasPendingResources()) {
            clientElement->buildPendingResource();
            extensions.clearHasPendingResourcesIfPossible(clientElement);
        }
    }
}

bool SVGElement::rendererIsNeeded(const RenderStyle& style)
{
    // http://www.w3.org/TR/SVG/extend.html#PrivateData
    // Prevent anything other than SVG renderers from appearing in our render tree
    // Spec: SVG allows inclusion of elements from foreign namespaces anywhere
    // with the SVG content. In general, the SVG user agent will include the unknown
    // elements in the DOM but will otherwise ignore unknown elements.
    if (!parentOrShadowHostElement() || parentOrShadowHostElement()->isSVGElement())
        return Element::rendererIsNeeded(style);

    return false;
}

SVGElementRareData* SVGElement::svgRareData() const
{
    ASSERT(hasSVGRareData());
    return SVGElementRareData::rareDataFromMap(this);
}

SVGElementRareData* SVGElement::ensureSVGRareData()
{
    if (hasSVGRareData())
        return svgRareData();

    ASSERT(!SVGElementRareData::rareDataMap().contains(this));
    SVGElementRareData* data = new SVGElementRareData;
    SVGElementRareData::rareDataMap().set(this, data);
    setHasSVGRareData();
    return data;
}

bool SVGElement::isOutermostSVGSVGElement() const
{
    if (!isSVGSVGElement(*this))
        return false;

    // Element may not be in the document, pretend we're outermost for viewport(), getCTM(), etc.
    if (!parentNode())
        return true;

    // We act like an outermost SVG element, if we're a direct child of a <foreignObject> element.
    if (isSVGForeignObjectElement(*parentNode()))
        return true;

    // If we're living in a shadow tree, we're a <svg> element that got created as replacement
    // for a <symbol> element or a cloned <svg> element in the referenced tree. In that case
    // we're always an inner <svg> element.
    if (isInShadowTree() && parentOrShadowHostElement() && parentOrShadowHostElement()->isSVGElement())
        return false;

    // This is true whenever this is the outermost SVG, even if there are HTML elements outside it
    return !parentNode()->isSVGElement();
}

void SVGElement::reportAttributeParsingError(SVGParsingError error, const QualifiedName& name, const AtomicString& value)
{
    if (error == NoError)
        return;

    String errorString = "<" + tagName() + "> attribute " + name.toString() + "=\"" + value + "\"";
    SVGDocumentExtensions& extensions = document().accessSVGExtensions();

    if (error == NegativeValueForbiddenError) {
        extensions.reportError("Invalid negative value for " + errorString);
        return;
    }

    if (error == ParsingAttributeFailedError) {
        extensions.reportError("Invalid value for " + errorString);
        return;
    }

    ASSERT_NOT_REACHED();
}

String SVGElement::title() const
{
    // According to spec, we should not return titles when hovering over root <svg> elements (those
    // <title> elements are the title of the document, not a tooltip) so we instantly return.
    if (isOutermostSVGSVGElement())
        return String();

    // Walk up the tree, to find out whether we're inside a <use> shadow tree, to find the right title.
    if (isInShadowTree()) {
        Element* shadowHostElement = toShadowRoot(treeScope().rootNode()).host();
        if (isSVGUseElement(shadowHostElement)) {
            SVGUseElement& useElement = toSVGUseElement(*shadowHostElement);

            // If the <use> title is not empty we found the title to use.
            String useTitle(useElement.title());
            if (!useTitle.isEmpty())
                return useTitle;
        }
    }

    // If we aren't an instance in a <use> or the <use> title was not found, then find the first
    // <title> child of this element.
    // If a title child was found, return the text contents.
    if (Element* titleElement = Traversal<SVGTitleElement>::firstChild(*this))
        return titleElement->innerText();

    // Otherwise return a null/empty string.
    return String();
}

bool SVGElement::instanceUpdatesBlocked() const
{
    return hasSVGRareData() && svgRareData()->instanceUpdatesBlocked();
}

void SVGElement::setInstanceUpdatesBlocked(bool value)
{
    if (hasSVGRareData())
        svgRareData()->setInstanceUpdatesBlocked(value);
}

AffineTransform SVGElement::localCoordinateSpaceTransform(CTMScope) const
{
    // To be overriden by SVGGraphicsElement (or as special case SVGTextElement and SVGPatternElement)
    return AffineTransform();
}

const AtomicString& SVGElement::xmlbase() const
{
    return fastGetAttribute(XMLNames::baseAttr);
}

void SVGElement::setXMLbase(const AtomicString& value)
{
    setAttribute(XMLNames::baseAttr, value);
}

const AtomicString& SVGElement::xmllang() const
{
    return fastGetAttribute(XMLNames::langAttr);
}

void SVGElement::setXMLlang(const AtomicString& value)
{
    setAttribute(XMLNames::langAttr, value);
}

const AtomicString& SVGElement::xmlspace() const
{
    return fastGetAttribute(XMLNames::spaceAttr);
}

void SVGElement::setXMLspace(const AtomicString& value)
{
    setAttribute(XMLNames::spaceAttr, value);
}

Node::InsertionNotificationRequest SVGElement::insertedInto(ContainerNode* rootParent)
{
    Element::insertedInto(rootParent);
    updateRelativeLengthsInformation();
    buildPendingResourcesIfNeeded();
    return InsertionDone;
}

void SVGElement::removedFrom(ContainerNode* rootParent)
{
    bool wasInDocument = rootParent->inDocument();

    if (wasInDocument && hasRelativeLengths()) {
        // The root of the subtree being removed should take itself out from its parent's relative
        // length set. For the other nodes in the subtree we don't need to do anything: they will
        // get their own removedFrom() notification and just clear their sets.
        if (rootParent->isSVGElement() && !parentNode()) {
            ASSERT(toSVGElement(rootParent)->m_elementsWithRelativeLengths.contains(this));
            toSVGElement(rootParent)->updateRelativeLengthsInformation(false, this);
        }

        m_elementsWithRelativeLengths.clear();
    }

    ASSERT_WITH_SECURITY_IMPLICATION(!rootParent->isSVGElement() || !toSVGElement(rootParent)->m_elementsWithRelativeLengths.contains(this));

    Element::removedFrom(rootParent);

    if (wasInDocument) {
        document().accessSVGExtensions().rebuildAllElementReferencesForTarget(this);
        document().accessSVGExtensions().removeAllElementReferencesForTarget(this);
    }

    SVGElementInstance::invalidateAllInstancesOfElement(this);
}

void SVGElement::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    Element::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);

    // Invalidate all SVGElementInstances associated with us.
    if (!changedByParser)
        SVGElementInstance::invalidateAllInstancesOfElement(this);
}

CSSPropertyID SVGElement::cssPropertyIdForSVGAttributeName(const QualifiedName& attrName)
{
    if (!attrName.namespaceURI().isNull())
        return CSSPropertyInvalid;

    static HashMap<StringImpl*, CSSPropertyID>* propertyNameToIdMap = 0;
    if (!propertyNameToIdMap) {
        propertyNameToIdMap = new HashMap<StringImpl*, CSSPropertyID>;
        // This is a list of all base CSS and SVG CSS properties which are exposed as SVG XML attributes
        mapAttributeToCSSProperty(propertyNameToIdMap, alignment_baselineAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, baseline_shiftAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, buffered_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, clipAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, clip_pathAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, clip_ruleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, SVGNames::colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_interpolationAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_interpolation_filtersAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_profileAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, color_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, cursorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, SVGNames::directionAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, displayAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, dominant_baselineAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, enable_backgroundAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, fillAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, fill_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, fill_ruleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, filterAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, flood_colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, flood_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_familyAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_sizeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_stretchAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_styleAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_variantAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, font_weightAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, glyph_orientation_horizontalAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, glyph_orientation_verticalAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, image_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, letter_spacingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, lighting_colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, marker_endAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, marker_midAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, marker_startAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, maskAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, mask_typeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, overflowAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, paint_orderAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, pointer_eventsAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, shape_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stop_colorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stop_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, strokeAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_dasharrayAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_dashoffsetAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_linecapAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_linejoinAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_miterlimitAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_opacityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, stroke_widthAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, text_anchorAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, text_decorationAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, text_renderingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, transform_originAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, unicode_bidiAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, vector_effectAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, visibilityAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, word_spacingAttr);
        mapAttributeToCSSProperty(propertyNameToIdMap, writing_modeAttr);
    }

    return propertyNameToIdMap->get(attrName.localName().impl());
}

void SVGElement::updateRelativeLengthsInformation(bool clientHasRelativeLengths, SVGElement* clientElement)
{
    ASSERT(clientElement);

    // If we're not yet in a document, this function will be called again from insertedInto(). Do nothing now.
    if (!inDocument())
        return;

    // An element wants to notify us that its own relative lengths state changed.
    // Register it in the relative length map, and register us in the parent relative length map.
    // Register the parent in the grandparents map, etc. Repeat procedure until the root of the SVG tree.
    for (ContainerNode* currentNode = this; currentNode && currentNode->isSVGElement(); currentNode = currentNode->parentNode()) {
        SVGElement* currentElement = toSVGElement(currentNode);
        ASSERT(!currentElement->m_inRelativeLengthClientsInvalidation);

        bool hadRelativeLengths = currentElement->hasRelativeLengths();
        if (clientHasRelativeLengths)
            currentElement->m_elementsWithRelativeLengths.add(clientElement);
        else
            currentElement->m_elementsWithRelativeLengths.remove(clientElement);

        // If the relative length state hasn't changed, we can stop propagating the notification.
        if (hadRelativeLengths == currentElement->hasRelativeLengths())
            return;

        clientElement = currentElement;
        clientHasRelativeLengths = clientElement->hasRelativeLengths();
    }

    // Register root SVG elements for top level viewport change notifications.
    if (isSVGSVGElement(*clientElement)) {
        SVGDocumentExtensions& svgExtensions = accessDocumentSVGExtensions();
        if (clientElement->hasRelativeLengths())
            svgExtensions.addSVGRootWithRelativeLengthDescendents(toSVGSVGElement(clientElement));
        else
            svgExtensions.removeSVGRootWithRelativeLengthDescendents(toSVGSVGElement(clientElement));
    }
}

void SVGElement::invalidateRelativeLengthClients(SubtreeLayoutScope* layoutScope)
{
    if (!inDocument())
        return;

    ASSERT(!m_inRelativeLengthClientsInvalidation);
#if !ASSERT_DISABLED
    TemporaryChange<bool> inRelativeLengthClientsInvalidationChange(m_inRelativeLengthClientsInvalidation, true);
#endif

    RenderObject* renderer = this->renderer();
    if (renderer && selfHasRelativeLengths()) {
        if (renderer->isSVGResourceContainer())
            toRenderSVGResourceContainer(renderer)->invalidateCacheAndMarkForLayout(layoutScope);
        else
            renderer->setNeedsLayout(MarkContainingBlockChain, layoutScope);
    }

    HashSet<SVGElement*>::iterator end = m_elementsWithRelativeLengths.end();
    for (HashSet<SVGElement*>::iterator it = m_elementsWithRelativeLengths.begin(); it != end; ++it) {
        if (*it != this)
            (*it)->invalidateRelativeLengthClients(layoutScope);
    }
}

SVGSVGElement* SVGElement::ownerSVGElement() const
{
    ContainerNode* n = parentOrShadowHostNode();
    while (n) {
        if (isSVGSVGElement(*n))
            return toSVGSVGElement(n);

        n = n->parentOrShadowHostNode();
    }

    return 0;
}

SVGElement* SVGElement::viewportElement() const
{
    // This function needs shadow tree support - as RenderSVGContainer uses this function
    // to determine the "overflow" property. <use> on <symbol> wouldn't work otherwhise.
    ContainerNode* n = parentOrShadowHostNode();
    while (n) {
        if (isSVGSVGElement(*n) || isSVGImageElement(*n) || isSVGSymbolElement(*n))
            return toSVGElement(n);

        n = n->parentOrShadowHostNode();
    }

    return 0;
}

SVGDocumentExtensions& SVGElement::accessDocumentSVGExtensions()
{
    // This function is provided for use by SVGAnimatedProperty to avoid
    // global inclusion of core/dom/Document.h in SVG code.
    return document().accessSVGExtensions();
}

void SVGElement::mapInstanceToElement(SVGElementInstance* instance)
{
    ASSERT(instance);

    HashSet<SVGElementInstance*>& instances = ensureSVGRareData()->elementInstances();
    ASSERT(!instances.contains(instance));

    instances.add(instance);
}

void SVGElement::removeInstanceMapping(SVGElementInstance* instance)
{
    ASSERT(instance);
    ASSERT(hasSVGRareData());

    HashSet<SVGElementInstance*>& instances = svgRareData()->elementInstances();
    ASSERT(instances.contains(instance));

    instances.remove(instance);
}

const HashSet<SVGElementInstance*>& SVGElement::instancesForElement() const
{
    if (!hasSVGRareData()) {
        DEFINE_STATIC_LOCAL(HashSet<SVGElementInstance*>, emptyInstances, ());
        return emptyInstances;
    }
    return svgRareData()->elementInstances();
}

bool SVGElement::getBoundingBox(FloatRect& rect)
{
    if (!isSVGGraphicsElement())
        return false;

    rect = toSVGGraphicsElement(this)->getBBox();
    return true;
}

void SVGElement::setCursorElement(SVGCursorElement* cursorElement)
{
    SVGElementRareData* rareData = ensureSVGRareData();
    if (SVGCursorElement* oldCursorElement = rareData->cursorElement()) {
        if (cursorElement == oldCursorElement)
            return;
        oldCursorElement->removeReferencedElement(this);
    }
    rareData->setCursorElement(cursorElement);
}

void SVGElement::cursorElementRemoved()
{
    ASSERT(hasSVGRareData());
    svgRareData()->setCursorElement(0);
}

void SVGElement::setCursorImageValue(CSSCursorImageValue* cursorImageValue)
{
    SVGElementRareData* rareData = ensureSVGRareData();
    if (CSSCursorImageValue* oldCursorImageValue = rareData->cursorImageValue()) {
        if (cursorImageValue == oldCursorImageValue)
            return;
        oldCursorImageValue->removeReferencedElement(this);
    }
    rareData->setCursorImageValue(cursorImageValue);
}

void SVGElement::cursorImageValueRemoved()
{
    ASSERT(hasSVGRareData());
    svgRareData()->setCursorImageValue(0);
}

SVGElement* SVGElement::correspondingElement()
{
    ASSERT(!hasSVGRareData() || !svgRareData()->correspondingElement() || containingShadowRoot());
    return hasSVGRareData() ? svgRareData()->correspondingElement() : 0;
}

void SVGElement::setCorrespondingElement(SVGElement* correspondingElement)
{
    ensureSVGRareData()->setCorrespondingElement(correspondingElement);
}

void SVGElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == HTMLNames::classAttr) {
        // SVG animation has currently requires special storage of values so we set
        // the className here. svgAttributeChanged actually causes the resulting
        // style updates (instead of Element::parseAttribute). We don't
        // tell Element about the change to avoid parsing the class list twice
        SVGParsingError parseError = NoError;
        m_className->setBaseValueAsString(value, parseError);
        reportAttributeParsingError(parseError, name, value);
    } else if (name.matches(XMLNames::langAttr) || name.matches(XMLNames::spaceAttr)) {
    } else {
        // standard events
        const AtomicString& eventName = HTMLElement::eventNameForAttributeName(name);
        if (!eventName.isNull())
            setAttributeEventListener(eventName, createAttributeEventListener(this, name, value));
        else
            Element::parseAttribute(name, value);
    }
}

typedef HashMap<QualifiedName, AnimatedPropertyType> AttributeToPropertyTypeMap;
AnimatedPropertyType SVGElement::animatedPropertyTypeForCSSAttribute(const QualifiedName& attributeName)
{
    DEFINE_STATIC_LOCAL(AttributeToPropertyTypeMap, cssPropertyMap, ());

    if (cssPropertyMap.isEmpty()) {
        // Fill the map for the first use.
        cssPropertyMap.set(alignment_baselineAttr, AnimatedString);
        cssPropertyMap.set(baseline_shiftAttr, AnimatedString);
        cssPropertyMap.set(buffered_renderingAttr, AnimatedString);
        cssPropertyMap.set(clipAttr, AnimatedRect);
        cssPropertyMap.set(clip_pathAttr, AnimatedString);
        cssPropertyMap.set(clip_ruleAttr, AnimatedString);
        cssPropertyMap.set(SVGNames::colorAttr, AnimatedColor);
        cssPropertyMap.set(color_interpolationAttr, AnimatedString);
        cssPropertyMap.set(color_interpolation_filtersAttr, AnimatedString);
        cssPropertyMap.set(color_profileAttr, AnimatedString);
        cssPropertyMap.set(color_renderingAttr, AnimatedString);
        cssPropertyMap.set(cursorAttr, AnimatedString);
        cssPropertyMap.set(displayAttr, AnimatedString);
        cssPropertyMap.set(dominant_baselineAttr, AnimatedString);
        cssPropertyMap.set(fillAttr, AnimatedColor);
        cssPropertyMap.set(fill_opacityAttr, AnimatedNumber);
        cssPropertyMap.set(fill_ruleAttr, AnimatedString);
        cssPropertyMap.set(filterAttr, AnimatedString);
        cssPropertyMap.set(flood_colorAttr, AnimatedColor);
        cssPropertyMap.set(flood_opacityAttr, AnimatedNumber);
        cssPropertyMap.set(font_familyAttr, AnimatedString);
        cssPropertyMap.set(font_sizeAttr, AnimatedLength);
        cssPropertyMap.set(font_stretchAttr, AnimatedString);
        cssPropertyMap.set(font_styleAttr, AnimatedString);
        cssPropertyMap.set(font_variantAttr, AnimatedString);
        cssPropertyMap.set(font_weightAttr, AnimatedString);
        cssPropertyMap.set(image_renderingAttr, AnimatedString);
        cssPropertyMap.set(letter_spacingAttr, AnimatedLength);
        cssPropertyMap.set(lighting_colorAttr, AnimatedColor);
        cssPropertyMap.set(marker_endAttr, AnimatedString);
        cssPropertyMap.set(marker_midAttr, AnimatedString);
        cssPropertyMap.set(marker_startAttr, AnimatedString);
        cssPropertyMap.set(maskAttr, AnimatedString);
        cssPropertyMap.set(mask_typeAttr, AnimatedString);
        cssPropertyMap.set(opacityAttr, AnimatedNumber);
        cssPropertyMap.set(overflowAttr, AnimatedString);
        cssPropertyMap.set(paint_orderAttr, AnimatedString);
        cssPropertyMap.set(pointer_eventsAttr, AnimatedString);
        cssPropertyMap.set(shape_renderingAttr, AnimatedString);
        cssPropertyMap.set(stop_colorAttr, AnimatedColor);
        cssPropertyMap.set(stop_opacityAttr, AnimatedNumber);
        cssPropertyMap.set(strokeAttr, AnimatedColor);
        cssPropertyMap.set(stroke_dasharrayAttr, AnimatedLengthList);
        cssPropertyMap.set(stroke_dashoffsetAttr, AnimatedLength);
        cssPropertyMap.set(stroke_linecapAttr, AnimatedString);
        cssPropertyMap.set(stroke_linejoinAttr, AnimatedString);
        cssPropertyMap.set(stroke_miterlimitAttr, AnimatedNumber);
        cssPropertyMap.set(stroke_opacityAttr, AnimatedNumber);
        cssPropertyMap.set(stroke_widthAttr, AnimatedLength);
        cssPropertyMap.set(text_anchorAttr, AnimatedString);
        cssPropertyMap.set(text_decorationAttr, AnimatedString);
        cssPropertyMap.set(text_renderingAttr, AnimatedString);
        cssPropertyMap.set(vector_effectAttr, AnimatedString);
        cssPropertyMap.set(visibilityAttr, AnimatedString);
        cssPropertyMap.set(word_spacingAttr, AnimatedLength);
    }

    if (cssPropertyMap.contains(attributeName))
        return cssPropertyMap.get(attributeName);

    return AnimatedUnknown;
}

void SVGElement::addToPropertyMap(PassRefPtr<SVGAnimatedPropertyBase> passProperty)
{
    RefPtr<SVGAnimatedPropertyBase> property(passProperty);
    QualifiedName attributeName = property->attributeName();
    m_newAttributeToPropertyMap.set(attributeName, property.release());
}

PassRefPtr<SVGAnimatedPropertyBase> SVGElement::propertyFromAttribute(const QualifiedName& attributeName)
{
    return m_newAttributeToPropertyMap.get(attributeName);
}

bool SVGElement::isAnimatableCSSProperty(const QualifiedName& attrName)
{
    return animatedPropertyTypeForCSSAttribute(attrName) != AnimatedUnknown;
}

bool SVGElement::isPresentationAttribute(const QualifiedName& name) const
{
    return cssPropertyIdForSVGAttributeName(name) > 0;
}

void SVGElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    CSSPropertyID propertyID = cssPropertyIdForSVGAttributeName(name);
    if (propertyID > 0)
        addPropertyToPresentationAttributeStyle(style, propertyID, value);
}

bool SVGElement::haveLoadedRequiredResources()
{
    for (SVGElement* child = Traversal<SVGElement>::firstChild(*this); child; child = Traversal<SVGElement>::nextSibling(*child)) {
        if (!child->haveLoadedRequiredResources())
            return false;
    }
    return true;
}

static inline void collectInstancesForSVGElement(SVGElement* element, HashSet<SVGElementInstance*>& instances)
{
    ASSERT(element);
    if (element->containingShadowRoot())
        return;

    ASSERT(!element->instanceUpdatesBlocked());

    instances = element->instancesForElement();
}

bool SVGElement::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> prpListener, bool useCapture)
{
    RefPtr<EventListener> listener = prpListener;

    // Add event listener to regular DOM element
    if (!Node::addEventListener(eventType, listener, useCapture))
        return false;

    // Add event listener to all shadow tree DOM element instances
    HashSet<SVGElementInstance*> instances;
    collectInstancesForSVGElement(this, instances);
    const HashSet<SVGElementInstance*>::const_iterator end = instances.end();
    for (HashSet<SVGElementInstance*>::const_iterator it = instances.begin(); it != end; ++it) {
        ASSERT((*it)->shadowTreeElement());
        ASSERT((*it)->correspondingElement() == this);

        bool result = (*it)->shadowTreeElement()->Node::addEventListener(eventType, listener, useCapture);
        ASSERT_UNUSED(result, result);
    }

    return true;
}

bool SVGElement::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    HashSet<SVGElementInstance*> instances;
    collectInstancesForSVGElement(this, instances);
    if (instances.isEmpty())
        return Node::removeEventListener(eventType, listener, useCapture);

    // EventTarget::removeEventListener creates a PassRefPtr around the given EventListener
    // object when creating a temporary RegisteredEventListener object used to look up the
    // event listener in a cache. If we want to be able to call removeEventListener() multiple
    // times on different nodes, we have to delay its immediate destruction, which would happen
    // after the first call below.
    RefPtr<EventListener> protector(listener);

    // Remove event listener from regular DOM element
    if (!Node::removeEventListener(eventType, listener, useCapture))
        return false;

    // Remove event listener from all shadow tree DOM element instances
    const HashSet<SVGElementInstance*>::const_iterator end = instances.end();
    for (HashSet<SVGElementInstance*>::const_iterator it = instances.begin(); it != end; ++it) {
        ASSERT((*it)->correspondingElement() == this);

        SVGElement* shadowTreeElement = (*it)->shadowTreeElement();
        ASSERT(shadowTreeElement);

        if (shadowTreeElement->Node::removeEventListener(eventType, listener, useCapture))
            continue;

        // This case can only be hit for event listeners created from markup
        ASSERT(listener->wasCreatedFromMarkup());

        // If the event listener 'listener' has been created from markup and has been fired before
        // then JSLazyEventListener::parseCode() has been called and m_jsFunction of that listener
        // has been created (read: it's not 0 anymore). During shadow tree creation, the event
        // listener DOM attribute has been cloned, and another event listener has been setup in
        // the shadow tree. If that event listener has not been used yet, m_jsFunction is still 0,
        // and tryRemoveEventListener() above will fail. Work around that very seldom problem.
        EventTargetData* data = shadowTreeElement->eventTargetData();
        ASSERT(data);

        data->eventListenerMap.removeFirstEventListenerCreatedFromMarkup(eventType);
    }

    return true;
}

static bool hasLoadListener(Element* element)
{
    if (element->hasEventListeners(EventTypeNames::load))
        return true;

    for (element = element->parentOrShadowHostElement(); element; element = element->parentOrShadowHostElement()) {
        const EventListenerVector& entry = element->getEventListeners(EventTypeNames::load);
        for (size_t i = 0; i < entry.size(); ++i) {
            if (entry[i].useCapture)
                return true;
        }
    }

    return false;
}

void SVGElement::sendSVGLoadEventIfPossible(bool sendParentLoadEvents)
{
    RefPtr<SVGElement> currentTarget = this;
    while (currentTarget && currentTarget->haveLoadedRequiredResources()) {
        RefPtr<Element> parent;
        if (sendParentLoadEvents)
            parent = currentTarget->parentOrShadowHostElement(); // save the next parent to dispatch too incase dispatching the event changes the tree
        if (hasLoadListener(currentTarget.get())
            && (currentTarget->isStructurallyExternal() || isSVGSVGElement(*currentTarget)))
            currentTarget->dispatchEvent(Event::create(EventTypeNames::load));
        currentTarget = (parent && parent->isSVGElement()) ? static_pointer_cast<SVGElement>(parent) : RefPtr<SVGElement>();
        SVGElement* element = currentTarget.get();
        if (!element || !element->isOutermostSVGSVGElement())
            continue;

        // Consider <svg onload="foo()"><image xlink:href="foo.png" externalResourcesRequired="true"/></svg>.
        // If foo.png is not yet loaded, the first SVGLoad event will go to the <svg> element, sent through
        // Document::implicitClose(). Then the SVGLoad event will fire for <image>, once its loaded.
        ASSERT(sendParentLoadEvents);

        // If the load event was not sent yet by Document::implicitClose(), but the <image> from the example
        // above, just appeared, don't send the SVGLoad event to the outermost <svg>, but wait for the document
        // to be "ready to render", first.
        if (!document().loadEventFinished())
            break;
    }
}

void SVGElement::sendSVGLoadEventIfPossibleAsynchronously()
{
    svgLoadEventTimer()->startOneShot(0, FROM_HERE);
}

void SVGElement::svgLoadEventTimerFired(Timer<SVGElement>*)
{
    sendSVGLoadEventIfPossible();
}

Timer<SVGElement>* SVGElement::svgLoadEventTimer()
{
    ASSERT_NOT_REACHED();
    return 0;
}

void SVGElement::finishParsingChildren()
{
    Element::finishParsingChildren();

    // The outermost SVGSVGElement SVGLoad event is fired through Document::dispatchWindowLoadEvent.
    if (isOutermostSVGSVGElement())
        return;

    // finishParsingChildren() is called when the close tag is reached for an element (e.g. </svg>)
    // we send SVGLoad events here if we can, otherwise they'll be sent when any required loads finish
    if (isSVGSVGElement(*this))
        sendSVGLoadEventIfPossible();
}

void SVGElement::attributeChanged(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason)
{
    Element::attributeChanged(name, newValue);

    if (isIdAttributeName(name))
        document().accessSVGExtensions().rebuildAllElementReferencesForTarget(this);

    // Changes to the style attribute are processed lazily (see Element::getAttribute() and related methods),
    // so we don't want changes to the style attribute to result in extra work here.
    if (name != HTMLNames::styleAttr)
        svgAttributeChanged(name);
}

void SVGElement::svgAttributeChanged(const QualifiedName& attrName)
{
    CSSPropertyID propId = SVGElement::cssPropertyIdForSVGAttributeName(attrName);
    if (propId > 0) {
        SVGElementInstance::invalidateAllInstancesOfElement(this);
        return;
    }

    if (attrName == HTMLNames::classAttr) {
        classAttributeChanged(AtomicString(m_className->currentValue()->value()));
        SVGElementInstance::invalidateAllInstancesOfElement(this);
        return;
    }

    if (isIdAttributeName(attrName)) {
        RenderObject* object = renderer();
        // Notify resources about id changes, this is important as we cache resources by id in SVGDocumentExtensions
        if (object && object->isSVGResourceContainer())
            toRenderSVGResourceContainer(object)->idChanged();
        if (inDocument())
            buildPendingResourcesIfNeeded();
        SVGElementInstance::invalidateAllInstancesOfElement(this);
        return;
    }
}

void SVGElement::synchronizeAnimatedSVGAttribute(const QualifiedName& name) const
{
    if (!elementData() || !elementData()->m_animatedSVGAttributesAreDirty)
        return;

    if (name == anyQName()) {
        AttributeToPropertyMap::const_iterator::Values it = m_newAttributeToPropertyMap.values().begin();
        AttributeToPropertyMap::const_iterator::Values end = m_newAttributeToPropertyMap.values().end();
        for (; it != end; ++it) {
            if ((*it)->needsSynchronizeAttribute())
                (*it)->synchronizeAttribute();
        }

        elementData()->m_animatedSVGAttributesAreDirty = false;
    } else {
        RefPtr<SVGAnimatedPropertyBase> property = m_newAttributeToPropertyMap.get(name);
        if (property && property->needsSynchronizeAttribute())
            property->synchronizeAttribute();
    }
}

PassRefPtr<RenderStyle> SVGElement::customStyleForRenderer()
{
    if (!correspondingElement())
        return document().ensureStyleResolver().styleForElement(this);

    RenderStyle* style = 0;
    if (Element* parent = parentOrShadowHostElement()) {
        if (RenderObject* renderer = parent->renderer())
            style = renderer->style();
    }

    return document().ensureStyleResolver().styleForElement(correspondingElement(), style, DisallowStyleSharing);
}

MutableStylePropertySet* SVGElement::animatedSMILStyleProperties() const
{
    if (hasSVGRareData())
        return svgRareData()->animatedSMILStyleProperties();
    return 0;
}

MutableStylePropertySet* SVGElement::ensureAnimatedSMILStyleProperties()
{
    return ensureSVGRareData()->ensureAnimatedSMILStyleProperties();
}

void SVGElement::setUseOverrideComputedStyle(bool value)
{
    if (hasSVGRareData())
        svgRareData()->setUseOverrideComputedStyle(value);
}

RenderStyle* SVGElement::computedStyle(PseudoId pseudoElementSpecifier)
{
    if (!hasSVGRareData() || !svgRareData()->useOverrideComputedStyle())
        return Element::computedStyle(pseudoElementSpecifier);

    RenderStyle* parentStyle = 0;
    if (Element* parent = parentOrShadowHostElement()) {
        if (RenderObject* renderer = parent->renderer())
            parentStyle = renderer->style();
    }

    return svgRareData()->overrideComputedStyle(this, parentStyle);
}

bool SVGElement::hasFocusEventListeners() const
{
    return hasEventListeners(EventTypeNames::focusin) || hasEventListeners(EventTypeNames::focusout);
}

bool SVGElement::isKeyboardFocusable() const
{
    return isFocusable();
}

#ifndef NDEBUG
bool SVGElement::isAnimatableAttribute(const QualifiedName& name) const
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, animatableAttributes, ());

    if (animatableAttributes.isEmpty()) {
        animatableAttributes.add(XLinkNames::hrefAttr);
        animatableAttributes.add(SVGNames::amplitudeAttr);
        animatableAttributes.add(SVGNames::azimuthAttr);
        animatableAttributes.add(SVGNames::baseFrequencyAttr);
        animatableAttributes.add(SVGNames::biasAttr);
        animatableAttributes.add(SVGNames::clipPathUnitsAttr);
        animatableAttributes.add(SVGNames::cxAttr);
        animatableAttributes.add(SVGNames::cyAttr);
        animatableAttributes.add(SVGNames::diffuseConstantAttr);
        animatableAttributes.add(SVGNames::divisorAttr);
        animatableAttributes.add(SVGNames::dxAttr);
        animatableAttributes.add(SVGNames::dyAttr);
        animatableAttributes.add(SVGNames::edgeModeAttr);
        animatableAttributes.add(SVGNames::elevationAttr);
        animatableAttributes.add(SVGNames::exponentAttr);
        animatableAttributes.add(SVGNames::filterResAttr);
        animatableAttributes.add(SVGNames::filterUnitsAttr);
        animatableAttributes.add(SVGNames::fxAttr);
        animatableAttributes.add(SVGNames::fyAttr);
        animatableAttributes.add(SVGNames::gradientTransformAttr);
        animatableAttributes.add(SVGNames::gradientUnitsAttr);
        animatableAttributes.add(SVGNames::heightAttr);
        animatableAttributes.add(SVGNames::in2Attr);
        animatableAttributes.add(SVGNames::inAttr);
        animatableAttributes.add(SVGNames::interceptAttr);
        animatableAttributes.add(SVGNames::k1Attr);
        animatableAttributes.add(SVGNames::k2Attr);
        animatableAttributes.add(SVGNames::k3Attr);
        animatableAttributes.add(SVGNames::k4Attr);
        animatableAttributes.add(SVGNames::kernelMatrixAttr);
        animatableAttributes.add(SVGNames::kernelUnitLengthAttr);
        animatableAttributes.add(SVGNames::lengthAdjustAttr);
        animatableAttributes.add(SVGNames::limitingConeAngleAttr);
        animatableAttributes.add(SVGNames::markerHeightAttr);
        animatableAttributes.add(SVGNames::markerUnitsAttr);
        animatableAttributes.add(SVGNames::markerWidthAttr);
        animatableAttributes.add(SVGNames::maskContentUnitsAttr);
        animatableAttributes.add(SVGNames::maskUnitsAttr);
        animatableAttributes.add(SVGNames::methodAttr);
        animatableAttributes.add(SVGNames::modeAttr);
        animatableAttributes.add(SVGNames::numOctavesAttr);
        animatableAttributes.add(SVGNames::offsetAttr);
        animatableAttributes.add(SVGNames::operatorAttr);
        animatableAttributes.add(SVGNames::orderAttr);
        animatableAttributes.add(SVGNames::orientAttr);
        animatableAttributes.add(SVGNames::pathLengthAttr);
        animatableAttributes.add(SVGNames::patternContentUnitsAttr);
        animatableAttributes.add(SVGNames::patternTransformAttr);
        animatableAttributes.add(SVGNames::patternUnitsAttr);
        animatableAttributes.add(SVGNames::pointsAtXAttr);
        animatableAttributes.add(SVGNames::pointsAtYAttr);
        animatableAttributes.add(SVGNames::pointsAtZAttr);
        animatableAttributes.add(SVGNames::preserveAlphaAttr);
        animatableAttributes.add(SVGNames::preserveAspectRatioAttr);
        animatableAttributes.add(SVGNames::primitiveUnitsAttr);
        animatableAttributes.add(SVGNames::radiusAttr);
        animatableAttributes.add(SVGNames::rAttr);
        animatableAttributes.add(SVGNames::refXAttr);
        animatableAttributes.add(SVGNames::refYAttr);
        animatableAttributes.add(SVGNames::resultAttr);
        animatableAttributes.add(SVGNames::rotateAttr);
        animatableAttributes.add(SVGNames::rxAttr);
        animatableAttributes.add(SVGNames::ryAttr);
        animatableAttributes.add(SVGNames::scaleAttr);
        animatableAttributes.add(SVGNames::seedAttr);
        animatableAttributes.add(SVGNames::slopeAttr);
        animatableAttributes.add(SVGNames::spacingAttr);
        animatableAttributes.add(SVGNames::specularConstantAttr);
        animatableAttributes.add(SVGNames::specularExponentAttr);
        animatableAttributes.add(SVGNames::spreadMethodAttr);
        animatableAttributes.add(SVGNames::startOffsetAttr);
        animatableAttributes.add(SVGNames::stdDeviationAttr);
        animatableAttributes.add(SVGNames::stitchTilesAttr);
        animatableAttributes.add(SVGNames::surfaceScaleAttr);
        animatableAttributes.add(SVGNames::tableValuesAttr);
        animatableAttributes.add(SVGNames::targetAttr);
        animatableAttributes.add(SVGNames::targetXAttr);
        animatableAttributes.add(SVGNames::targetYAttr);
        animatableAttributes.add(SVGNames::transformAttr);
        animatableAttributes.add(SVGNames::typeAttr);
        animatableAttributes.add(SVGNames::valuesAttr);
        animatableAttributes.add(SVGNames::viewBoxAttr);
        animatableAttributes.add(SVGNames::widthAttr);
        animatableAttributes.add(SVGNames::x1Attr);
        animatableAttributes.add(SVGNames::x2Attr);
        animatableAttributes.add(SVGNames::xAttr);
        animatableAttributes.add(SVGNames::xChannelSelectorAttr);
        animatableAttributes.add(SVGNames::y1Attr);
        animatableAttributes.add(SVGNames::y2Attr);
        animatableAttributes.add(SVGNames::yAttr);
        animatableAttributes.add(SVGNames::yChannelSelectorAttr);
        animatableAttributes.add(SVGNames::zAttr);
    }

    if (name == classAttr)
        return true;

    return animatableAttributes.contains(name);
}
#endif
}
