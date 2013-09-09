/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2007 David Smith (catfish.man@gmail.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012, 2013 Apple Inc. All rights reserved.
 *           (C) 2007 Eric Seidel (eric@webkit.org)
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
#include "core/dom/Element.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "SVGNames.h"
#include "XMLNames.h"
#include "bindings/v8/ExceptionState.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/animation/DocumentTimeline.h"
#include "core/animation/css/CSSAnimations.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSValuePool.h"
#include "core/css/PropertySetCSSStyleDeclaration.h"
#include "core/css/StylePropertySet.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/Attr.h"
#include "core/dom/Attribute.h"
#include "core/dom/ClientRect.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/CustomElement.h"
#include "core/dom/CustomElementRegistrationContext.h"
#include "core/dom/DatasetDOMStringMap.h"
#include "core/dom/Document.h"
#include "core/dom/DocumentSharedObjectPool.h"
#include "core/dom/ElementRareData.h"
#include "core/dom/EventDispatcher.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/FocusEvent.h"
#include "core/dom/FullscreenElementStack.h"
#include "core/dom/MutationObserverInterestGroup.h"
#include "core/dom/MutationRecord.h"
#include "core/dom/NamedNodeMap.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/NodeRenderingContext.h"
#include "core/dom/PseudoElement.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/dom/SelectorQuery.h"
#include "core/dom/Text.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/TextIterator.h"
#include "core/editing/htmlediting.h"
#include "core/html/ClassList.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDocument.h"
#include "core/html/HTMLElement.h"
#include "core/html/HTMLFormControlsCollection.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/html/HTMLLabelElement.h"
#include "core/html/HTMLOptionsCollection.h"
#include "core/html/HTMLTableRowsCollection.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/PointerLockController.h"
#include "core/rendering/FlowThreadController.h"
#include "core/rendering/RenderRegion.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/RenderWidget.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "core/svg/SVGElement.h"
#include "wtf/BitVector.h"
#include "wtf/HashFunctions.h"
#include "wtf/text/CString.h"
#include "wtf/text/TextPosition.h"

namespace WebCore {

using namespace HTMLNames;
using namespace XMLNames;

static inline bool shouldIgnoreAttributeCase(const Element* e)
{
    return e && e->document().isHTMLDocument() && e->isHTMLElement();
}

class StyleResolverParentPusher {
public:
    StyleResolverParentPusher(Element* parent)
        : m_parent(parent)
        , m_pushedStyleResolver(0)
    {
    }
    void push()
    {
        if (m_pushedStyleResolver)
            return;
        m_pushedStyleResolver = m_parent->document().styleResolver();
        m_pushedStyleResolver->pushParentElement(m_parent);
    }
    ~StyleResolverParentPusher()
    {

        if (!m_pushedStyleResolver)
            return;

        // This tells us that our pushed style selector is in a bad state,
        // so we should just bail out in that scenario.
        ASSERT(m_pushedStyleResolver == m_parent->document().styleResolver());
        if (m_pushedStyleResolver != m_parent->document().styleResolver())
            return;

        m_pushedStyleResolver->popParentElement(m_parent);
    }

private:
    Element* m_parent;
    StyleResolver* m_pushedStyleResolver;
};

typedef Vector<RefPtr<Attr> > AttrNodeList;
typedef HashMap<Element*, OwnPtr<AttrNodeList> > AttrNodeListMap;

static AttrNodeListMap& attrNodeListMap()
{
    DEFINE_STATIC_LOCAL(AttrNodeListMap, map, ());
    return map;
}

static AttrNodeList* attrNodeListForElement(Element* element)
{
    if (!element->hasSyntheticAttrChildNodes())
        return 0;
    ASSERT(attrNodeListMap().contains(element));
    return attrNodeListMap().get(element);
}

static AttrNodeList* ensureAttrNodeListForElement(Element* element)
{
    if (element->hasSyntheticAttrChildNodes()) {
        ASSERT(attrNodeListMap().contains(element));
        return attrNodeListMap().get(element);
    }
    ASSERT(!attrNodeListMap().contains(element));
    element->setHasSyntheticAttrChildNodes(true);
    AttrNodeListMap::AddResult result = attrNodeListMap().add(element, adoptPtr(new AttrNodeList));
    return result.iterator->value.get();
}

static void removeAttrNodeListForElement(Element* element)
{
    ASSERT(element->hasSyntheticAttrChildNodes());
    ASSERT(attrNodeListMap().contains(element));
    attrNodeListMap().remove(element);
    element->setHasSyntheticAttrChildNodes(false);
}

static Attr* findAttrNodeInList(AttrNodeList* attrNodeList, const QualifiedName& name)
{
    for (unsigned i = 0; i < attrNodeList->size(); ++i) {
        if (attrNodeList->at(i)->qualifiedName() == name)
            return attrNodeList->at(i).get();
    }
    return 0;
}

PassRefPtr<Element> Element::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new Element(tagName, document, CreateElement));
}

Element::~Element()
{
#ifndef NDEBUG
    if (document().renderer()) {
        // When the document is not destroyed, an element that was part of a named flow
        // content nodes should have been removed from the content nodes collection
        // and the inNamedFlow flag reset.
        ASSERT(!inNamedFlow());
    }
#endif

    if (PropertySetCSSStyleDeclaration* cssomWrapper = inlineStyleCSSOMWrapper())
        cssomWrapper->clearParentElement();

    if (hasRareData()) {
        ElementRareData* data = elementRareData();
        data->setPseudoElement(BEFORE, 0);
        data->setPseudoElement(AFTER, 0);
        data->setPseudoElement(BACKDROP, 0);
        data->clearShadow();

        if (RuntimeEnabledFeatures::webAnimationsCSSEnabled()) {
            if (ActiveAnimations* activeAnimations = data->activeAnimations())
                activeAnimations->cssAnimations()->cancel();
        }
    }

    if (isCustomElement())
        CustomElement::wasDestroyed(this);

    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    if (hasPendingResources()) {
        document().accessSVGExtensions()->removeElementFromPendingResources(this);
        ASSERT(!hasPendingResources());
    }
}

inline ElementRareData* Element::elementRareData() const
{
    ASSERT(hasRareData());
    return static_cast<ElementRareData*>(rareData());
}

inline ElementRareData* Element::ensureElementRareData()
{
    return static_cast<ElementRareData*>(ensureRareData());
}

void Element::clearTabIndexExplicitlyIfNeeded()
{
    if (hasRareData())
        elementRareData()->clearTabIndexExplicitly();
}

void Element::setTabIndexExplicitly(short tabIndex)
{
    ensureElementRareData()->setTabIndexExplicitly(tabIndex);
}

bool Element::supportsFocus() const
{
    return hasRareData() && elementRareData()->tabIndexSetExplicitly();
}

short Element::tabIndex() const
{
    return hasRareData() ? elementRareData()->tabIndex() : 0;
}

bool Element::rendererIsFocusable() const
{
    // Elements in canvas fallback content are not rendered, but they are allowed to be
    // focusable as long as their canvas is displayed and visible.
    if (isInCanvasSubtree()) {
        const Element* e = this;
        while (e && !e->hasLocalName(canvasTag))
            e = e->parentElement();
        ASSERT(e);
        return e->renderer() && e->renderer()->style()->visibility() == VISIBLE;
    }

    // FIXME: These asserts should be in Node::isFocusable, but there are some
    // callsites like Document::setFocusedElement that would currently fail on
    // them. See crbug.com/251163
    if (renderer()) {
        ASSERT(!renderer()->needsLayout());
    } else {
        // We can't just use needsStyleRecalc() because if the node is in a
        // display:none tree it might say it needs style recalc but the whole
        // document is actually up to date.
        ASSERT(!document().childNeedsStyleRecalc());
    }

    // FIXME: Even if we are not visible, we might have a child that is visible.
    // Hyatt wants to fix that some day with a "has visible content" flag or the like.
    if (!renderer() || renderer()->style()->visibility() != VISIBLE)
        return false;

    return true;
}

DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, blur);
DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, error);
DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, focus);
DEFINE_VIRTUAL_ATTRIBUTE_EVENT_LISTENER(Element, load);

PassRefPtr<Node> Element::cloneNode(bool deep)
{
    return deep ? cloneElementWithChildren() : cloneElementWithoutChildren();
}

PassRefPtr<Element> Element::cloneElementWithChildren()
{
    RefPtr<Element> clone = cloneElementWithoutChildren();
    cloneChildNodes(clone.get());
    return clone.release();
}

PassRefPtr<Element> Element::cloneElementWithoutChildren()
{
    RefPtr<Element> clone = cloneElementWithoutAttributesAndChildren();
    // This will catch HTML elements in the wrong namespace that are not correctly copied.
    // This is a sanity check as HTML overloads some of the DOM methods.
    ASSERT(isHTMLElement() == clone->isHTMLElement());

    clone->cloneDataFromElement(*this);
    return clone.release();
}

PassRefPtr<Element> Element::cloneElementWithoutAttributesAndChildren()
{
    return document().createElement(tagQName(), false);
}

PassRefPtr<Attr> Element::detachAttribute(size_t index)
{
    ASSERT(elementData());
    const Attribute* attribute = elementData()->attributeItem(index);
    RefPtr<Attr> attrNode = attrIfExists(attribute->name());
    if (attrNode)
        detachAttrNodeAtIndex(attrNode.get(), index);
    else {
        attrNode = Attr::create(document(), attribute->name(), attribute->value());
        removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
    }
    return attrNode.release();
}

void Element::detachAttrNodeAtIndex(Attr* attr, size_t index)
{
    ASSERT(attr);
    ASSERT(elementData());

    const Attribute* attribute = elementData()->attributeItem(index);
    ASSERT(attribute);
    ASSERT(attribute->name() == attr->qualifiedName());
    detachAttrNodeFromElementWithValue(attr, attribute->value());
    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::removeAttribute(const QualifiedName& name)
{
    if (!elementData())
        return;

    size_t index = elementData()->getAttributeItemIndex(name);
    if (index == notFound)
        return;

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::setBooleanAttribute(const QualifiedName& name, bool value)
{
    if (value)
        setAttribute(name, emptyAtom);
    else
        removeAttribute(name);
}

NamedNodeMap* Element::attributes() const
{
    ElementRareData* rareData = const_cast<Element*>(this)->ensureElementRareData();
    if (NamedNodeMap* attributeMap = rareData->attributeMap())
        return attributeMap;

    rareData->setAttributeMap(NamedNodeMap::create(const_cast<Element*>(this)));
    return rareData->attributeMap();
}

ActiveAnimations* Element::activeAnimations() const
{
    if (hasActiveAnimations())
        return elementRareData()->activeAnimations();
    return 0;
}

ActiveAnimations* Element::ensureActiveAnimations()
{
    ElementRareData* rareData = ensureElementRareData();
    if (!elementRareData()->activeAnimations())
        rareData->setActiveAnimations(adoptPtr(new ActiveAnimations()));
    return rareData->activeAnimations();
}

bool Element::hasActiveAnimations() const
{
    if (!RuntimeEnabledFeatures::webAnimationsEnabled())
        return false;

    if (!hasRareData())
        return false;

    ActiveAnimations* activeAnimations = elementRareData()->activeAnimations();
    return activeAnimations && !activeAnimations->isEmpty();
}

Node::NodeType Element::nodeType() const
{
    return ELEMENT_NODE;
}

bool Element::hasAttribute(const QualifiedName& name) const
{
    return hasAttributeNS(name.namespaceURI(), name.localName());
}

void Element::synchronizeAllAttributes() const
{
    if (!elementData())
        return;
    if (elementData()->m_styleAttributeIsDirty) {
        ASSERT(isStyledElement());
        synchronizeStyleAttributeInternal();
    }
    if (elementData()->m_animatedSVGAttributesAreDirty) {
        ASSERT(isSVGElement());
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(anyQName());
    }
}

inline void Element::synchronizeAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return;
    if (UNLIKELY(name == styleAttr && elementData()->m_styleAttributeIsDirty)) {
        ASSERT(isStyledElement());
        synchronizeStyleAttributeInternal();
        return;
    }
    if (UNLIKELY(elementData()->m_animatedSVGAttributesAreDirty)) {
        ASSERT(isSVGElement());
        toSVGElement(this)->synchronizeAnimatedSVGAttribute(name);
    }
}

inline void Element::synchronizeAttribute(const AtomicString& localName) const
{
    // This version of synchronizeAttribute() is streamlined for the case where you don't have a full QualifiedName,
    // e.g when called from DOM API.
    if (!elementData())
        return;
    if (elementData()->m_styleAttributeIsDirty && equalPossiblyIgnoringCase(localName, styleAttr.localName(), shouldIgnoreAttributeCase(this))) {
        ASSERT(isStyledElement());
        synchronizeStyleAttributeInternal();
        return;
    }
    if (elementData()->m_animatedSVGAttributesAreDirty) {
        // We're not passing a namespace argument on purpose. SVGNames::*Attr are defined w/o namespaces as well.
        ASSERT(isSVGElement());
        static_cast<const SVGElement*>(this)->synchronizeAnimatedSVGAttribute(QualifiedName(nullAtom, localName, nullAtom));
    }
}

const AtomicString& Element::getAttribute(const QualifiedName& name) const
{
    if (!elementData())
        return nullAtom;
    synchronizeAttribute(name);
    if (const Attribute* attribute = getAttributeItem(name))
        return attribute->value();
    return nullAtom;
}

void Element::scrollIntoView(bool alignToTop)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    LayoutRect bounds = boundingBox();
    // Align to the top / bottom and to the closest edge.
    if (alignToTop)
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignTopAlways);
    else
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignBottomAlways);
}

void Element::scrollIntoViewIfNeeded(bool centerIfNeeded)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    LayoutRect bounds = boundingBox();
    if (centerIfNeeded)
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignCenterIfNeeded, ScrollAlignment::alignCenterIfNeeded);
    else
        renderer()->scrollRectToVisible(bounds, ScrollAlignment::alignToEdgeIfNeeded, ScrollAlignment::alignToEdgeIfNeeded);
}

void Element::scrollByUnits(int units, ScrollGranularity granularity)
{
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return;

    if (!renderer()->hasOverflowClip())
        return;

    ScrollDirection direction = ScrollDown;
    if (units < 0) {
        direction = ScrollUp;
        units = -units;
    }
    Node* stopNode = this;
    toRenderBox(renderer())->scroll(direction, granularity, units, &stopNode);
}

void Element::scrollByLines(int lines)
{
    scrollByUnits(lines, ScrollByLine);
}

void Element::scrollByPages(int pages)
{
    scrollByUnits(pages, ScrollByPage);
}

static float localZoomForRenderer(RenderObject* renderer)
{
    // FIXME: This does the wrong thing if two opposing zooms are in effect and canceled each
    // other out, but the alternative is that we'd have to crawl up the whole render tree every
    // time (or store an additional bit in the RenderStyle to indicate that a zoom was specified).
    float zoomFactor = 1;
    if (renderer->style()->effectiveZoom() != 1) {
        // Need to find the nearest enclosing RenderObject that set up
        // a differing zoom, and then we divide our result by it to eliminate the zoom.
        RenderObject* prev = renderer;
        for (RenderObject* curr = prev->parent(); curr; curr = curr->parent()) {
            if (curr->style()->effectiveZoom() != prev->style()->effectiveZoom()) {
                zoomFactor = prev->style()->zoom();
                break;
            }
            prev = curr;
        }
        if (prev->isRenderView())
            zoomFactor = prev->style()->zoom();
    }
    return zoomFactor;
}

static int adjustForLocalZoom(LayoutUnit value, RenderObject* renderer)
{
    float zoomFactor = localZoomForRenderer(renderer);
    if (zoomFactor == 1)
        return value;
    return lroundf(value / zoomFactor);
}

int Element::offsetLeft()
{
    document().partialUpdateLayoutIgnorePendingStylesheets(this);
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustForLocalZoom(renderer->pixelSnappedOffsetLeft(), renderer);
    return 0;
}

int Element::offsetTop()
{
    document().partialUpdateLayoutIgnorePendingStylesheets(this);
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustForLocalZoom(renderer->pixelSnappedOffsetTop(), renderer);
    return 0;
}

int Element::offsetWidth()
{
    document().updateStyleForNodeIfNeeded(this);

    if (RenderBox* renderer = renderBox()) {
        if (!renderer->requiresLayoutToDetermineWidth())
            return adjustLayoutUnitForAbsoluteZoom(renderer->fixedOffsetWidth(), renderer).round();
    }

    document().partialUpdateLayoutIgnorePendingStylesheets(this);
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedOffsetWidth(), renderer).round();
    return 0;
}

int Element::offsetHeight()
{
    document().partialUpdateLayoutIgnorePendingStylesheets(this);
    if (RenderBoxModelObject* renderer = renderBoxModelObject())
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedOffsetHeight(), renderer).round();
    return 0;
}

Element* Element::bindingsOffsetParent()
{
    Element* element = offsetParent();
    if (!element || !element->isInShadowTree())
        return element;
    return element->containingShadowRoot()->shouldExposeToBindings() ? element : 0;
}

Element* Element::offsetParent()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderObject* renderer = this->renderer())
        return renderer->offsetParent();
    return 0;
}

int Element::clientLeft()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (RenderBox* renderer = renderBox())
        return adjustForAbsoluteZoom(roundToInt(renderer->clientLeft()), renderer);
    return 0;
}

int Element::clientTop()
{
    document().updateLayoutIgnorePendingStylesheets();

    if (RenderBox* renderer = renderBox())
        return adjustForAbsoluteZoom(roundToInt(renderer->clientTop()), renderer);
    return 0;
}

int Element::clientWidth()
{
    document().updateLayoutIgnorePendingStylesheets();

    // When in strict mode, clientWidth for the document element should return the width of the containing frame.
    // When in quirks mode, clientWidth for the body element should return the width of the containing frame.
    bool inQuirksMode = document().inQuirksMode();
    if ((!inQuirksMode && document().documentElement() == this)
        || (inQuirksMode && isHTMLElement() && document().body() == this)) {
        if (FrameView* view = document().view()) {
            if (RenderView* renderView = document().renderView())
                return adjustForAbsoluteZoom(view->layoutWidth(), renderView);
        }
    }

    if (RenderBox* renderer = renderBox())
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedClientWidth(), renderer).round();
    return 0;
}

int Element::clientHeight()
{
    document().updateLayoutIgnorePendingStylesheets();

    // When in strict mode, clientHeight for the document element should return the height of the containing frame.
    // When in quirks mode, clientHeight for the body element should return the height of the containing frame.
    bool inQuirksMode = document().inQuirksMode();

    if ((!inQuirksMode && document().documentElement() == this)
        || (inQuirksMode && isHTMLElement() && document().body() == this)) {
        if (FrameView* view = document().view()) {
            if (RenderView* renderView = document().renderView())
                return adjustForAbsoluteZoom(view->layoutHeight(), renderView);
        }
    }

    if (RenderBox* renderer = renderBox())
        return adjustLayoutUnitForAbsoluteZoom(renderer->pixelSnappedClientHeight(), renderer).round();
    return 0;
}

int Element::scrollLeft()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollLeft(), rend);
    return 0;
}

int Element::scrollTop()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollTop(), rend);
    return 0;
}

void Element::setScrollLeft(int newLeft)
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        rend->setScrollLeft(static_cast<int>(newLeft * rend->style()->effectiveZoom()));
}

void Element::setScrollTop(int newTop)
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        rend->setScrollTop(static_cast<int>(newTop * rend->style()->effectiveZoom()));
}

int Element::scrollWidth()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollWidth(), rend);
    return 0;
}

int Element::scrollHeight()
{
    document().updateLayoutIgnorePendingStylesheets();
    if (RenderBox* rend = renderBox())
        return adjustForAbsoluteZoom(rend->scrollHeight(), rend);
    return 0;
}

IntRect Element::boundsInRootViewSpace()
{
    document().updateLayoutIgnorePendingStylesheets();

    FrameView* view = document().view();
    if (!view)
        return IntRect();

    Vector<FloatQuad> quads;
    if (isSVGElement() && renderer()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement* svgElement = toSVGElement(this);
        FloatRect localRect;
        if (svgElement->getBoundingBox(localRect))
            quads.append(renderer()->localToAbsoluteQuad(localRect));
    } else {
        // Get the bounding rectangle from the box model.
        if (renderBoxModelObject())
            renderBoxModelObject()->absoluteQuads(quads);
    }

    if (quads.isEmpty())
        return IntRect();

    IntRect result = quads[0].enclosingBoundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].enclosingBoundingBox());

    result = view->contentsToRootView(result);
    return result;
}

PassRefPtr<ClientRectList> Element::getClientRects()
{
    document().updateLayoutIgnorePendingStylesheets();

    RenderBoxModelObject* renderBoxModelObject = this->renderBoxModelObject();
    if (!renderBoxModelObject)
        return ClientRectList::create();

    // FIXME: Handle SVG elements.
    // FIXME: Handle table/inline-table with a caption.

    Vector<FloatQuad> quads;
    renderBoxModelObject->absoluteQuads(quads);
    document().adjustFloatQuadsForScrollAndAbsoluteZoom(quads, renderBoxModelObject);
    return ClientRectList::create(quads);
}

PassRefPtr<ClientRect> Element::getBoundingClientRect()
{
    document().updateLayoutIgnorePendingStylesheets();

    Vector<FloatQuad> quads;
    if (isSVGElement() && renderer() && !renderer()->isSVGRoot()) {
        // Get the bounding rectangle from the SVG model.
        SVGElement* svgElement = toSVGElement(this);
        FloatRect localRect;
        if (svgElement->getBoundingBox(localRect))
            quads.append(renderer()->localToAbsoluteQuad(localRect));
    } else {
        // Get the bounding rectangle from the box model.
        if (renderBoxModelObject())
            renderBoxModelObject()->absoluteQuads(quads);
    }

    if (quads.isEmpty())
        return ClientRect::create();

    FloatRect result = quads[0].boundingBox();
    for (size_t i = 1; i < quads.size(); ++i)
        result.unite(quads[i].boundingBox());

    document().adjustFloatRectForScrollAndAbsoluteZoom(result, renderer());
    return ClientRect::create(result);
}

IntRect Element::screenRect() const
{
    if (!renderer())
        return IntRect();
    // FIXME: this should probably respect transforms
    return document().view()->contentsToScreen(renderer()->absoluteBoundingBoxRectIgnoringTransforms());
}

const AtomicString& Element::getAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return nullAtom;
    synchronizeAttribute(localName);
    if (const Attribute* attribute = elementData()->getAttributeItem(localName, shouldIgnoreAttributeCase(this)))
        return attribute->value();
    return nullAtom;
}

const AtomicString& Element::getAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    return getAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

void Element::setAttribute(const AtomicString& localName, const AtomicString& value, ExceptionState& es)
{
    if (!Document::isValidName(localName)) {
        es.throwDOMException(InvalidCharacterError);
        return;
    }

    synchronizeAttribute(localName);
    const AtomicString& caseAdjustedLocalName = shouldIgnoreAttributeCase(this) ? localName.lower() : localName;

    size_t index = elementData() ? elementData()->getAttributeItemIndex(caseAdjustedLocalName, false) : notFound;
    const QualifiedName& qName = index != notFound ? attributeItem(index)->name() : QualifiedName(nullAtom, caseAdjustedLocalName, nullAtom);
    setAttributeInternal(index, qName, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setAttribute(const QualifiedName& name, const AtomicString& value)
{
    synchronizeAttribute(name);
    size_t index = elementData() ? elementData()->getAttributeItemIndex(name) : notFound;
    setAttributeInternal(index, name, value, NotInSynchronizationOfLazyAttribute);
}

void Element::setSynchronizedLazyAttribute(const QualifiedName& name, const AtomicString& value)
{
    size_t index = elementData() ? elementData()->getAttributeItemIndex(name) : notFound;
    setAttributeInternal(index, name, value, InSynchronizationOfLazyAttribute);
}

inline void Element::setAttributeInternal(size_t index, const QualifiedName& name, const AtomicString& newValue, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (newValue.isNull()) {
        if (index != notFound)
            removeAttributeInternal(index, inSynchronizationOfLazyAttribute);
        return;
    }

    if (index == notFound) {
        addAttributeInternal(name, newValue, inSynchronizationOfLazyAttribute);
        return;
    }

    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(name, attributeItem(index)->value(), newValue);

    if (newValue != attributeItem(index)->value()) {
        // If there is an Attr node hooked to this attribute, the Attr::setValue() call below
        // will write into the ElementData.
        // FIXME: Refactor this so it makes some sense.
        if (RefPtr<Attr> attrNode = inSynchronizationOfLazyAttribute ? 0 : attrIfExists(name))
            attrNode->setValue(newValue);
        else
            ensureUniqueElementData()->attributeItem(index)->setValue(newValue);
    }

    if (!inSynchronizationOfLazyAttribute)
        didModifyAttribute(name, newValue);
}

static inline AtomicString makeIdForStyleResolution(const AtomicString& value, bool inQuirksMode)
{
    if (inQuirksMode)
        return value.lower();
    return value;
}

static bool checkNeedsStyleInvalidationForIdChange(const AtomicString& oldId, const AtomicString& newId, const RuleFeatureSet& features)
{
    ASSERT(newId != oldId);
    if (!oldId.isEmpty() && features.hasSelectorForId(oldId))
        return true;
    if (!newId.isEmpty() && features.hasSelectorForId(newId))
        return true;
    return false;
}

void Element::attributeChanged(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason reason)
{
    if (ElementShadow* parentElementShadow = shadowOfParentForDistribution(this)) {
        if (shouldInvalidateDistributionWhenAttributeChanged(parentElementShadow, name, newValue))
            parentElementShadow->setNeedsDistributionRecalc();
    }

    parseAttribute(name, newValue);

    document().incDOMTreeVersion();

    StyleResolver* styleResolver = document().styleResolverIfExists();
    bool testShouldInvalidateStyle = attached() && styleResolver && styleChangeType() < SubtreeStyleChange;
    bool shouldInvalidateStyle = false;

    if (isStyledElement() && name == styleAttr) {
        styleAttributeChanged(newValue, reason);
    } else if (isStyledElement() && isPresentationAttribute(name)) {
        elementData()->m_presentationAttributeStyleIsDirty = true;
        setNeedsStyleRecalc(LocalStyleChange);
    }

    if (isIdAttributeName(name)) {
        AtomicString oldId = elementData()->idForStyleResolution();
        AtomicString newId = makeIdForStyleResolution(newValue, document().inQuirksMode());
        if (newId != oldId) {
            elementData()->setIdForStyleResolution(newId);
            shouldInvalidateStyle = testShouldInvalidateStyle && checkNeedsStyleInvalidationForIdChange(oldId, newId, styleResolver->ruleFeatureSet());
        }
    } else if (name == classAttr) {
        classAttributeChanged(newValue);
    } else if (name == HTMLNames::nameAttr) {
        setHasName(!newValue.isNull());
    } else if (name == HTMLNames::pseudoAttr) {
        shouldInvalidateStyle |= testShouldInvalidateStyle && isInShadowTree();
    }

    invalidateNodeListCachesInAncestors(&name, this);

    // If there is currently no StyleResolver, we can't be sure that this attribute change won't affect style.
    shouldInvalidateStyle |= !styleResolver;

    if (shouldInvalidateStyle)
        setNeedsStyleRecalc();

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->handleAttributeChanged(name, this);
}

inline void Element::attributeChangedFromParserOrByCloning(const QualifiedName& name, const AtomicString& newValue, AttributeModificationReason reason)
{
    if (name == isAttr)
        CustomElementRegistrationContext::setTypeExtension(this, newValue, reason == ModifiedDirectly ? CustomElementRegistrationContext::CreatedByParser : CustomElementRegistrationContext::NotCreatedByParser);
    attributeChanged(name, newValue, reason);
}

template <typename CharacterType>
static inline bool classStringHasClassName(const CharacterType* characters, unsigned length)
{
    ASSERT(length > 0);

    unsigned i = 0;
    do {
        if (isNotHTMLSpace(characters[i]))
            break;
        ++i;
    } while (i < length);

    return i < length;
}

static inline bool classStringHasClassName(const AtomicString& newClassString)
{
    unsigned length = newClassString.length();

    if (!length)
        return false;

    if (newClassString.is8Bit())
        return classStringHasClassName(newClassString.characters8(), length);
    return classStringHasClassName(newClassString.characters16(), length);
}

template<typename Checker>
static bool checkSelectorForClassChange(const SpaceSplitString& changedClasses, const Checker& checker)
{
    unsigned changedSize = changedClasses.size();
    for (unsigned i = 0; i < changedSize; ++i) {
        if (checker.hasSelectorForClass(changedClasses[i]))
            return true;
    }
    return false;
}

template<typename Checker>
static bool checkSelectorForClassChange(const SpaceSplitString& oldClasses, const SpaceSplitString& newClasses, const Checker& checker)
{
    unsigned oldSize = oldClasses.size();
    if (!oldSize)
        return checkSelectorForClassChange(newClasses, checker);
    BitVector remainingClassBits;
    remainingClassBits.ensureSize(oldSize);
    // Class vectors tend to be very short. This is faster than using a hash table.
    unsigned newSize = newClasses.size();
    for (unsigned i = 0; i < newSize; ++i) {
        for (unsigned j = 0; j < oldSize; ++j) {
            if (newClasses[i] == oldClasses[j]) {
                remainingClassBits.quickSet(j);
                continue;
            }
        }
        if (checker.hasSelectorForClass(newClasses[i]))
            return true;
    }
    for (unsigned i = 0; i < oldSize; ++i) {
        // If the bit is not set the the corresponding class has been removed.
        if (remainingClassBits.quickGet(i))
            continue;
        if (checker.hasSelectorForClass(oldClasses[i]))
            return true;
    }
    return false;
}

void Element::classAttributeChanged(const AtomicString& newClassString)
{
    StyleResolver* styleResolver = document().styleResolverIfExists();
    bool testShouldInvalidateStyle = attached() && styleResolver && styleChangeType() < SubtreeStyleChange;
    bool shouldInvalidateStyle = false;

    if (classStringHasClassName(newClassString)) {
        const bool shouldFoldCase = document().inQuirksMode();
        const SpaceSplitString oldClasses = elementData()->classNames();
        elementData()->setClass(newClassString, shouldFoldCase);
        const SpaceSplitString& newClasses = elementData()->classNames();
        shouldInvalidateStyle = testShouldInvalidateStyle && checkSelectorForClassChange(oldClasses, newClasses, styleResolver->ruleFeatureSet());
    } else {
        const SpaceSplitString& oldClasses = elementData()->classNames();
        shouldInvalidateStyle = testShouldInvalidateStyle && checkSelectorForClassChange(oldClasses, styleResolver->ruleFeatureSet());
        elementData()->clearClass();
    }

    if (hasRareData())
        elementRareData()->clearClassListValueForQuirksMode();

    if (shouldInvalidateStyle)
        setNeedsStyleRecalc();
}

bool Element::shouldInvalidateDistributionWhenAttributeChanged(ElementShadow* elementShadow, const QualifiedName& name, const AtomicString& newValue)
{
    ASSERT(elementShadow);
    const SelectRuleFeatureSet& featureSet = elementShadow->ensureSelectFeatureSet();

    if (isIdAttributeName(name)) {
        AtomicString oldId = elementData()->idForStyleResolution();
        AtomicString newId = makeIdForStyleResolution(newValue, document().inQuirksMode());
        if (newId != oldId) {
            if (!oldId.isEmpty() && featureSet.hasSelectorForId(oldId))
                return true;
            if (!newId.isEmpty() && featureSet.hasSelectorForId(newId))
                return true;
        }
    }

    if (name == HTMLNames::classAttr) {
        const AtomicString& newClassString = newValue;
        if (classStringHasClassName(newClassString)) {
            const bool shouldFoldCase = document().inQuirksMode();
            const SpaceSplitString& oldClasses = elementData()->classNames();
            const SpaceSplitString newClasses(newClassString, shouldFoldCase);
            if (checkSelectorForClassChange(oldClasses, newClasses, featureSet))
                return true;
        } else {
            const SpaceSplitString& oldClasses = elementData()->classNames();
            if (checkSelectorForClassChange(oldClasses, featureSet))
                return true;
        }
    }

    return featureSet.hasSelectorForAttribute(name.localName());
}

// Returns true is the given attribute is an event handler.
// We consider an event handler any attribute that begins with "on".
// It is a simple solution that has the advantage of not requiring any
// code or configuration change if a new event handler is defined.

static inline bool isEventHandlerAttribute(const Attribute& attribute)
{
    return attribute.name().namespaceURI().isNull() && attribute.name().localName().startsWith("on");
}

bool Element::isJavaScriptURLAttribute(const Attribute& attribute) const
{
    return isURLAttribute(attribute) && protocolIsJavaScript(stripLeadingAndTrailingHTMLSpaces(attribute.value()));
}

void Element::stripScriptingAttributes(Vector<Attribute>& attributeVector) const
{
    size_t destination = 0;
    for (size_t source = 0; source < attributeVector.size(); ++source) {
        if (isEventHandlerAttribute(attributeVector[source])
            || isJavaScriptURLAttribute(attributeVector[source])
            || isHTMLContentAttribute(attributeVector[source]))
            continue;

        if (source != destination)
            attributeVector[destination] = attributeVector[source];

        ++destination;
    }
    attributeVector.shrink(destination);
}

void Element::parserSetAttributes(const Vector<Attribute>& attributeVector)
{
    ASSERT(!inDocument());
    ASSERT(!parentNode());
    ASSERT(!m_elementData);

    if (attributeVector.isEmpty())
        return;

    if (document().sharedObjectPool())
        m_elementData = document().sharedObjectPool()->cachedShareableElementDataWithAttributes(attributeVector);
    else
        m_elementData = ShareableElementData::createWithAttributes(attributeVector);

    // Use attributeVector instead of m_elementData because attributeChanged might modify m_elementData.
    for (unsigned i = 0; i < attributeVector.size(); ++i)
        attributeChangedFromParserOrByCloning(attributeVector[i].name(), attributeVector[i].value(), ModifiedDirectly);
}

bool Element::hasAttributes() const
{
    synchronizeAllAttributes();
    return elementData() && elementData()->length();
}

bool Element::hasEquivalentAttributes(const Element* other) const
{
    synchronizeAllAttributes();
    other->synchronizeAllAttributes();
    if (elementData() == other->elementData())
        return true;
    if (elementData())
        return elementData()->isEquivalent(other->elementData());
    if (other->elementData())
        return other->elementData()->isEquivalent(elementData());
    return true;
}

String Element::nodeName() const
{
    return m_tagName.toString();
}

String Element::nodeNamePreservingCase() const
{
    return m_tagName.toString();
}

void Element::setPrefix(const AtomicString& prefix, ExceptionState& es)
{
    checkSetPrefix(prefix, es);
    if (es.hadException())
        return;

    m_tagName.setPrefix(prefix.isEmpty() ? AtomicString() : prefix);
}

KURL Element::baseURI() const
{
    const AtomicString& baseAttribute = getAttribute(baseAttr);
    KURL base(KURL(), baseAttribute);
    if (!base.protocol().isEmpty())
        return base;

    ContainerNode* parent = parentNode();
    if (!parent)
        return base;

    const KURL& parentBase = parent->baseURI();
    if (parentBase.isNull())
        return base;

    return KURL(parentBase, baseAttribute);
}

const AtomicString Element::imageSourceURL() const
{
    return getAttribute(srcAttr);
}

bool Element::rendererIsNeeded(const RenderStyle& style)
{
    return style.display() != NONE;
}

RenderObject* Element::createRenderer(RenderStyle* style)
{
    return RenderObject::createObject(this, style);
}

Node::InsertionNotificationRequest Element::insertedInto(ContainerNode* insertionPoint)
{
    // need to do superclass processing first so inDocument() is true
    // by the time we reach updateId
    ContainerNode::insertedInto(insertionPoint);

    if (containsFullScreenElement() && parentElement() && !parentElement()->containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(true);

    if (Element* before = pseudoElement(BEFORE))
        before->insertedInto(insertionPoint);

    if (Element* after = pseudoElement(AFTER))
        after->insertedInto(insertionPoint);

    if (Element* backdrop = pseudoElement(BACKDROP))
        backdrop->insertedInto(insertionPoint);

    if (!insertionPoint->isInTreeScope())
        return InsertionDone;

    if (hasRareData())
        elementRareData()->clearClassListValueForQuirksMode();

    if (isUpgradedCustomElement() && inDocument())
        CustomElement::didEnterDocument(this, document());

    TreeScope& scope = insertionPoint->treeScope();
    if (&scope != &treeScope())
        return InsertionDone;

    const AtomicString& idValue = getIdAttribute();
    if (!idValue.isNull())
        updateId(scope, nullAtom, idValue);

    const AtomicString& nameValue = getNameAttribute();
    if (!nameValue.isNull())
        updateName(nullAtom, nameValue);

    if (hasTagName(labelTag)) {
        if (scope.shouldCacheLabelsByForAttribute())
            updateLabel(scope, nullAtom, fastGetAttribute(forAttr));
    }

    if (parentElement() && parentElement()->isInCanvasSubtree())
        setIsInCanvasSubtree(true);

    return InsertionDone;
}

void Element::removedFrom(ContainerNode* insertionPoint)
{
    bool wasInDocument = insertionPoint->inDocument();

    if (Element* before = pseudoElement(BEFORE))
        before->removedFrom(insertionPoint);

    if (Element* after = pseudoElement(AFTER))
        after->removedFrom(insertionPoint);

    if (Element* backdrop = pseudoElement(BACKDROP))
        backdrop->removedFrom(insertionPoint);
    document().removeFromTopLayer(this);

    if (containsFullScreenElement())
        setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(false);

    if (document().page())
        document().page()->pointerLockController().elementRemoved(this);

    setSavedLayerScrollOffset(IntSize());

    if (insertionPoint->isInTreeScope() && &treeScope() == &document()) {
        const AtomicString& idValue = getIdAttribute();
        if (!idValue.isNull())
            updateId(insertionPoint->treeScope(), idValue, nullAtom);

        const AtomicString& nameValue = getNameAttribute();
        if (!nameValue.isNull())
            updateName(nameValue, nullAtom);

        if (hasTagName(labelTag)) {
            TreeScope& treeScope = insertionPoint->treeScope();
            if (treeScope.shouldCacheLabelsByForAttribute())
                updateLabel(treeScope, fastGetAttribute(forAttr), nullAtom);
        }
    }

    ContainerNode::removedFrom(insertionPoint);
    if (wasInDocument) {
        if (hasPendingResources())
            document().accessSVGExtensions()->removeElementFromPendingResources(this);

        if (isUpgradedCustomElement())
            CustomElement::didLeaveDocument(this, insertionPoint->document());
    }

    if (hasRareData())
        elementRareData()->setIsInCanvasSubtree(false);
}

void Element::attach(const AttachContext& context)
{
    ASSERT(document().inStyleRecalc());

    PostAttachCallbackDisabler callbackDisabler(this);
    StyleResolverParentPusher parentPusher(this);
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;

    // We've already been through detach when doing a lazyAttach, but we might
    // need to clear any state that's been added since then.
    if (hasRareData() && styleChangeType() == LazyAttachStyleChange) {
        ElementRareData* data = elementRareData();
        data->clearComputedStyle();
        data->resetDynamicRestyleObservations();
        if (!context.resolvedStyle)
            data->resetStyleState();
    }

    NodeRenderingContext(this, context.resolvedStyle).createRendererForElementIfNeeded();

    createPseudoElementIfNeeded(BEFORE);

    // When a shadow root exists, it does the work of attaching the children.
    if (ElementShadow* shadow = this->shadow()) {
        parentPusher.push();
        shadow->attach(context);
    } else if (firstChild())
        parentPusher.push();

    ContainerNode::attach(context);

    createPseudoElementIfNeeded(AFTER);
    createPseudoElementIfNeeded(BACKDROP);

    if (hasRareData()) {
        ElementRareData* data = elementRareData();
        if (data->needsFocusAppearanceUpdateSoonAfterAttach()) {
            if (isFocusable() && document().focusedElement() == this)
                document().updateFocusAppearanceSoon(false /* don't restore selection */);
            data->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
        }
    }

    InspectorInstrumentation::didRecalculateStyleForElement(this);
}

void Element::unregisterNamedFlowContentNode()
{
    if (RuntimeEnabledFeatures::cssRegionsEnabled() && inNamedFlow() && document().renderView())
        document().renderView()->flowThreadController()->unregisterNamedFlowContentNode(this);
}

void Element::detach(const AttachContext& context)
{
    WidgetHierarchyUpdatesSuspensionScope suspendWidgetHierarchyUpdates;
    unregisterNamedFlowContentNode();
    cancelFocusAppearanceUpdate();
    if (hasRareData()) {
        ElementRareData* data = elementRareData();
        data->setPseudoElement(BEFORE, 0);
        data->setPseudoElement(AFTER, 0);
        data->setPseudoElement(BACKDROP, 0);
        data->clearComputedStyle();
        data->resetDynamicRestyleObservations();
        data->setIsInsideRegion(false);

        // Only clear the style state if we're not going to reuse the style from recalcStyle.
        if (!context.resolvedStyle)
            data->resetStyleState();

        if (RuntimeEnabledFeatures::webAnimationsCSSEnabled() && !context.performingReattach) {
            if (ActiveAnimations* activeAnimations = data->activeAnimations())
                activeAnimations->cssAnimations()->cancel();
        }
    }
    if (ElementShadow* shadow = this->shadow())
        shadow->detach(context);
    ContainerNode::detach(context);
}

bool Element::pseudoStyleCacheIsInvalid(const RenderStyle* currentStyle, RenderStyle* newStyle)
{
    ASSERT(currentStyle == renderStyle());
    ASSERT(renderer());

    if (!currentStyle)
        return false;

    const PseudoStyleCache* pseudoStyleCache = currentStyle->cachedPseudoStyles();
    if (!pseudoStyleCache)
        return false;

    size_t cacheSize = pseudoStyleCache->size();
    for (size_t i = 0; i < cacheSize; ++i) {
        RefPtr<RenderStyle> newPseudoStyle;
        PseudoId pseudoId = pseudoStyleCache->at(i)->styleType();
        if (pseudoId == FIRST_LINE || pseudoId == FIRST_LINE_INHERITED)
            newPseudoStyle = renderer()->uncachedFirstLineStyle(newStyle);
        else
            newPseudoStyle = renderer()->getUncachedPseudoStyle(PseudoStyleRequest(pseudoId), newStyle, newStyle);
        if (!newPseudoStyle)
            return true;
        if (*newPseudoStyle != *pseudoStyleCache->at(i)) {
            if (pseudoId < FIRST_INTERNAL_PSEUDOID)
                newStyle->setHasPseudoStyle(pseudoId);
            newStyle->addCachedPseudoStyle(newPseudoStyle);
            if (pseudoId == FIRST_LINE || pseudoId == FIRST_LINE_INHERITED) {
                // FIXME: We should do an actual diff to determine whether a repaint vs. layout
                // is needed, but for now just assume a layout will be required.  The diff code
                // in RenderObject::setStyle would need to be factored out so that it could be reused.
                renderer()->setNeedsLayoutAndPrefWidthsRecalc();
            }
            return true;
        }
    }
    return false;
}

PassRefPtr<RenderStyle> Element::styleForRenderer()
{
    if (hasCustomStyleCallbacks()) {
        if (RefPtr<RenderStyle> style = customStyleForRenderer())
            return style.release();
    }

    return originalStyleForRenderer();
}

PassRefPtr<RenderStyle> Element::originalStyleForRenderer()
{
    return document().styleResolver()->styleForElement(this);
}

bool Element::recalcStyle(StyleRecalcChange change)
{
    ASSERT(document().inStyleRecalc());

    if (hasCustomStyleCallbacks())
        willRecalcStyle(change);

    if (hasRareData() && (change > NoChange || needsStyleRecalc())) {
        ElementRareData* data = elementRareData();
        data->resetStyleState();
        data->clearComputedStyle();
    }

    // Active InsertionPoints have no renderers so they never need to go through a recalc.
    if ((change >= Inherit || needsStyleRecalc()) && parentRenderStyle() && !isActiveInsertionPoint(this))
        change = recalcOwnStyle(change);

    // If we reattached we don't need to recalc the style of our descendants anymore.
    if (change < Reattach)
        recalcChildStyle(change);

    clearNeedsStyleRecalc();
    clearChildNeedsStyleRecalc();

    if (hasCustomStyleCallbacks())
        didRecalcStyle(change);

    return change == Reattach;
}

StyleRecalcChange Element::recalcOwnStyle(StyleRecalcChange change)
{
    ASSERT(document().inStyleRecalc());

    CSSAnimationUpdateScope cssAnimationUpdateScope(this);
    RefPtr<RenderStyle> oldStyle = renderStyle();
    RefPtr<RenderStyle> newStyle = styleForRenderer();
    StyleRecalcChange localChange = RenderStyle::compare(oldStyle.get(), newStyle.get());

    if (localChange == Reattach) {
        AttachContext reattachContext;
        reattachContext.resolvedStyle = newStyle.get();
        reattach(reattachContext);
        return Reattach;
    }

    InspectorInstrumentation::didRecalculateStyleForElement(this);

    if (RenderObject* renderer = this->renderer()) {
        if (localChange != NoChange || pseudoStyleCacheIsInvalid(oldStyle.get(), newStyle.get()) || (change == Force && renderer->requiresForcedStyleRecalcPropagation()) || shouldNotifyRendererWithIdenticalStyles()) {
            renderer->setAnimatableStyle(newStyle.get());
        } else if (needsStyleRecalc()) {
            // Although no change occurred, we use the new style so that the cousin style sharing code won't get
            // fooled into believing this style is the same.
            renderer->setStyleInternal(newStyle.get());
        }
    }

    // If "rem" units are used anywhere in the document, and if the document element's font size changes, then go ahead and force font updating
    // all the way down the tree. This is simpler than having to maintain a cache of objects (and such font size changes should be rare anyway).
    if (document().styleSheetCollections()->usesRemUnits() && document().documentElement() == this && oldStyle && newStyle && oldStyle->fontSize() != newStyle->fontSize()) {
        // Cached RenderStyles may depend on the re units.
        document().styleResolver()->invalidateMatchedPropertiesCache();
        return Force;
    }

    if (styleChangeType() >= SubtreeStyleChange)
        return Force;

    return max(localChange, change);
}

void Element::recalcChildStyle(StyleRecalcChange change)
{
    ASSERT(document().inStyleRecalc());

    StyleResolverParentPusher parentPusher(this);

    for (ShadowRoot* root = youngestShadowRoot(); root; root = root->olderShadowRoot()) {
        if (shouldRecalcStyle(change, root)) {
            parentPusher.push();
            root->recalcStyle(change);
        }
    }

    if (shouldRecalcStyle(change, this))
        updatePseudoElement(BEFORE, change);

    // FIXME: This check is good enough for :hover + foo, but it is not good enough for :hover + foo + bar.
    // For now we will just worry about the common case, since it's a lot trickier to get the second case right
    // without doing way too much re-resolution.
    bool hasDirectAdjacentRules = childrenAffectedByDirectAdjacentRules();
    bool hasIndirectAdjacentRules = childrenAffectedByForwardPositionalRules();
    bool forceCheckOfNextElementSibling = false;
    bool forceCheckOfAnyElementSibling = false;
    bool forceReattachOfAnyWhitespaceSibling = false;
    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        bool didReattach = false;

        if (child->renderer())
            forceReattachOfAnyWhitespaceSibling = false;

        if (child->isTextNode()) {
            if (forceReattachOfAnyWhitespaceSibling && toText(child)->containsOnlyWhitespace())
                child->reattach();
            else
                didReattach = toText(child)->recalcTextStyle(change);
        } else if (child->isElementNode()) {
            Element* element = toElement(child);

            bool childRulesChanged = element->needsStyleRecalc() && element->styleChangeType() >= SubtreeStyleChange;

            if (forceCheckOfNextElementSibling || forceCheckOfAnyElementSibling)
                element->setNeedsStyleRecalc();

            forceCheckOfNextElementSibling = childRulesChanged && hasDirectAdjacentRules;
            forceCheckOfAnyElementSibling = forceCheckOfAnyElementSibling || (childRulesChanged && hasIndirectAdjacentRules);

            if (shouldRecalcStyle(change, element)) {
                parentPusher.push();
                didReattach = element->recalcStyle(change);
            }
        }

        forceReattachOfAnyWhitespaceSibling = didReattach || forceReattachOfAnyWhitespaceSibling;
    }

    if (shouldRecalcStyle(change, this)) {
        updatePseudoElement(AFTER, change);
        updatePseudoElement(BACKDROP, change);
    }
}

ElementShadow* Element::shadow() const
{
    return hasRareData() ? elementRareData()->shadow() : 0;
}

ElementShadow* Element::ensureShadow()
{
    return ensureElementRareData()->ensureShadow();
}

void Element::didAffectSelector(AffectedSelectorMask mask)
{
    setNeedsStyleRecalc();
    if (ElementShadow* elementShadow = shadowOfParentForDistribution(this))
        elementShadow->didAffectSelector(mask);
}

PassRefPtr<ShadowRoot> Element::createShadowRoot(ExceptionState& es)
{
    if (alwaysCreateUserAgentShadowRoot())
        ensureUserAgentShadowRoot();

    if (RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled())
        return ensureShadow()->addShadowRoot(this, ShadowRoot::AuthorShadowRoot);

    // Since some elements recreates shadow root dynamically, multiple shadow
    // subtrees won't work well in that element. Until they are fixed, we disable
    // adding author shadow root for them.
    if (!areAuthorShadowsAllowed()) {
        es.throwDOMException(HierarchyRequestError);
        return 0;
    }
    return ensureShadow()->addShadowRoot(this, ShadowRoot::AuthorShadowRoot);
}

ShadowRoot* Element::shadowRoot() const
{
    ElementShadow* elementShadow = shadow();
    if (!elementShadow)
        return 0;
    ShadowRoot* shadowRoot = elementShadow->youngestShadowRoot();
    if (shadowRoot->type() == ShadowRoot::AuthorShadowRoot)
        return shadowRoot;
    return 0;
}

ShadowRoot* Element::userAgentShadowRoot() const
{
    if (ElementShadow* elementShadow = shadow()) {
        if (ShadowRoot* shadowRoot = elementShadow->oldestShadowRoot()) {
            ASSERT(shadowRoot->type() == ShadowRoot::UserAgentShadowRoot);
            return shadowRoot;
        }
    }

    return 0;
}

ShadowRoot* Element::ensureUserAgentShadowRoot()
{
    if (ShadowRoot* shadowRoot = userAgentShadowRoot())
        return shadowRoot;
    ShadowRoot* shadowRoot = ensureShadow()->addShadowRoot(this, ShadowRoot::UserAgentShadowRoot);
    didAddUserAgentShadowRoot(shadowRoot);
    return shadowRoot;
}

bool Element::childTypeAllowed(NodeType type) const
{
    switch (type) {
    case ELEMENT_NODE:
    case TEXT_NODE:
    case COMMENT_NODE:
    case PROCESSING_INSTRUCTION_NODE:
    case CDATA_SECTION_NODE:
        return true;
    default:
        break;
    }
    return false;
}

static void inline checkForEmptyStyleChange(Element* element, RenderStyle* style)
{
    if (!style && !element->styleAffectedByEmpty())
        return;

    if (!style || (element->styleAffectedByEmpty() && (!style->emptyState() || element->hasChildNodes())))
        element->setNeedsStyleRecalc();
}

static void checkForSiblingStyleChanges(Element* e, RenderStyle* style, bool finishedParsingCallback,
                                        Node* beforeChange, Node* afterChange, int childCountDelta)
{
    if (!e->attached() || e->document().hasPendingForcedStyleRecalc() || e->styleChangeType() >= SubtreeStyleChange)
        return;

    // :empty selector.
    checkForEmptyStyleChange(e, style);

    if (!style || (e->needsStyleRecalc() && e->childrenAffectedByPositionalRules()))
        return;

    // Forward positional selectors include the ~ selector, nth-child, nth-of-type, first-of-type and only-of-type.
    // Backward positional selectors include nth-last-child, nth-last-of-type, last-of-type and only-of-type.
    // We have to invalidate everything following the insertion point in the forward case, and everything before the insertion point in the
    // backward case.
    // |afterChange| is 0 in the parser callback case, so we won't do any work for the forward case if we don't have to.
    // For performance reasons we just mark the parent node as changed, since we don't want to make childrenChanged O(n^2) by crawling all our kids
    // here. recalcStyle will then force a walk of the children when it sees that this has happened.
    if ((e->childrenAffectedByForwardPositionalRules() && afterChange) || (e->childrenAffectedByBackwardPositionalRules() && beforeChange)) {
        e->setNeedsStyleRecalc();
        return;
    }

    // :first-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    // |afterChange| is 0 in the parser case, so it works out that we'll skip this block.
    if (e->childrenAffectedByFirstChildRules() && afterChange) {
        // Find our new first child.
        Node* newFirstChild = e->firstElementChild();
        RenderStyle* newFirstChildStyle = newFirstChild ? newFirstChild->renderStyle() : 0;

        // Find the first element node following |afterChange|
        Node* firstElementAfterInsertion = afterChange->isElementNode() ? afterChange : afterChange->nextElementSibling();
        RenderStyle* firstElementAfterInsertionStyle = firstElementAfterInsertion ? firstElementAfterInsertion->renderStyle() : 0;

        // This is the insert/append case.
        if (newFirstChild != firstElementAfterInsertion && firstElementAfterInsertionStyle && firstElementAfterInsertionStyle->firstChildState())
            firstElementAfterInsertion->setNeedsStyleRecalc();

        // We also have to handle node removal.
        if (childCountDelta < 0 && newFirstChild == firstElementAfterInsertion && newFirstChild && (!newFirstChildStyle || !newFirstChildStyle->firstChildState()))
            newFirstChild->setNeedsStyleRecalc();
    }

    // :last-child.  In the parser callback case, we don't have to check anything, since we were right the first time.
    // In the DOM case, we only need to do something if |afterChange| is not 0.
    if (e->childrenAffectedByLastChildRules() && beforeChange) {
        // Find our new last child.
        Node* newLastChild = e->lastElementChild();
        RenderStyle* newLastChildStyle = newLastChild ? newLastChild->renderStyle() : 0;

        // Find the last element node going backwards from |beforeChange|
        Node* lastElementBeforeInsertion = beforeChange->isElementNode() ? beforeChange : beforeChange->previousElementSibling();
        RenderStyle* lastElementBeforeInsertionStyle = lastElementBeforeInsertion ? lastElementBeforeInsertion->renderStyle() : 0;

        if (newLastChild != lastElementBeforeInsertion && lastElementBeforeInsertionStyle && lastElementBeforeInsertionStyle->lastChildState())
            lastElementBeforeInsertion->setNeedsStyleRecalc();

        // We also have to handle node removal.  The parser callback case is similar to node removal as well in that we need to change the last child
        // to match now.
        if ((childCountDelta < 0 || finishedParsingCallback) && newLastChild == lastElementBeforeInsertion && newLastChild && (!newLastChildStyle || !newLastChildStyle->lastChildState()))
            newLastChild->setNeedsStyleRecalc();
    }

    // The + selector.  We need to invalidate the first element following the insertion point.  It is the only possible element
    // that could be affected by this DOM change.
    if (e->childrenAffectedByDirectAdjacentRules() && afterChange) {
        if (Node* firstElementAfterInsertion = afterChange->nextElementSibling())
            firstElementAfterInsertion->setNeedsStyleRecalc();
    }
}

void Element::childrenChanged(bool changedByParser, Node* beforeChange, Node* afterChange, int childCountDelta)
{
    ContainerNode::childrenChanged(changedByParser, beforeChange, afterChange, childCountDelta);
    if (changedByParser)
        checkForEmptyStyleChange(this, renderStyle());
    else
        checkForSiblingStyleChanges(this, renderStyle(), false, beforeChange, afterChange, childCountDelta);

    if (ElementShadow* shadow = this->shadow())
        shadow->setNeedsDistributionRecalc();
}

void Element::removeAllEventListeners()
{
    ContainerNode::removeAllEventListeners();
    if (ElementShadow* shadow = this->shadow())
        shadow->removeAllEventListeners();
}

void Element::beginParsingChildren()
{
    clearIsParsingChildrenFinished();
}

void Element::finishParsingChildren()
{
    setIsParsingChildrenFinished();
    checkForSiblingStyleChanges(this, renderStyle(), true, lastChild(), 0, 0);
    if (isCustomElement())
        CustomElement::didFinishParsingChildren(this);
}

#ifndef NDEBUG
void Element::formatForDebugger(char* buffer, unsigned length) const
{
    StringBuilder result;
    String s;

    result.append(nodeName());

    s = getIdAttribute();
    if (s.length() > 0) {
        if (result.length() > 0)
            result.appendLiteral("; ");
        result.appendLiteral("id=");
        result.append(s);
    }

    s = getAttribute(classAttr);
    if (s.length() > 0) {
        if (result.length() > 0)
            result.appendLiteral("; ");
        result.appendLiteral("class=");
        result.append(s);
    }

    strncpy(buffer, result.toString().utf8().data(), length - 1);
}
#endif

const Vector<RefPtr<Attr> >& Element::attrNodeList()
{
    ASSERT(hasSyntheticAttrChildNodes());
    return *attrNodeListForElement(this);
}

PassRefPtr<Attr> Element::setAttributeNode(Attr* attrNode, ExceptionState& es)
{
    if (!attrNode) {
        es.throwDOMException(TypeMismatchError);
        return 0;
    }

    RefPtr<Attr> oldAttrNode = attrIfExists(attrNode->qualifiedName());
    if (oldAttrNode.get() == attrNode)
        return attrNode; // This Attr is already attached to the element.

    // InUseAttributeError: Raised if node is an Attr that is already an attribute of another Element object.
    // The DOM user must explicitly clone Attr nodes to re-use them in other elements.
    if (attrNode->ownerElement()) {
        es.throwDOMException(InUseAttributeError);
        return 0;
    }

    synchronizeAllAttributes();
    UniqueElementData* elementData = ensureUniqueElementData();

    size_t index = elementData->getAttributeItemIndex(attrNode->qualifiedName(), shouldIgnoreAttributeCase(this));
    if (index != notFound) {
        if (oldAttrNode)
            detachAttrNodeFromElementWithValue(oldAttrNode.get(), elementData->attributeItem(index)->value());
        else
            oldAttrNode = Attr::create(document(), attrNode->qualifiedName(), elementData->attributeItem(index)->value());
    }

    setAttributeInternal(index, attrNode->qualifiedName(), attrNode->value(), NotInSynchronizationOfLazyAttribute);

    attrNode->attachToElement(this);
    treeScope().adoptIfNeeded(attrNode);
    ensureAttrNodeListForElement(this)->append(attrNode);

    return oldAttrNode.release();
}

PassRefPtr<Attr> Element::setAttributeNodeNS(Attr* attr, ExceptionState& es)
{
    return setAttributeNode(attr, es);
}

PassRefPtr<Attr> Element::removeAttributeNode(Attr* attr, ExceptionState& es)
{
    if (!attr) {
        es.throwDOMException(TypeMismatchError);
        return 0;
    }
    if (attr->ownerElement() != this) {
        es.throwDOMException(NotFoundError);
        return 0;
    }

    ASSERT(&document() == &attr->document());

    synchronizeAttribute(attr->qualifiedName());

    size_t index = elementData()->getAttrIndex(attr);
    if (index == notFound) {
        es.throwDOMException(NotFoundError);
        return 0;
    }

    RefPtr<Attr> guard(attr);
    detachAttrNodeAtIndex(attr, index);
    return guard.release();
}

bool Element::parseAttributeName(QualifiedName& out, const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionState& es)
{
    String prefix, localName;
    if (!Document::parseQualifiedName(qualifiedName, prefix, localName, es))
        return false;
    ASSERT(!es.hadException());

    QualifiedName qName(prefix, localName, namespaceURI);

    if (!Document::hasValidNamespaceForAttributes(qName)) {
        es.throwDOMException(NamespaceError);
        return false;
    }

    out = qName;
    return true;
}

void Element::setAttributeNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& value, ExceptionState& es)
{
    QualifiedName parsedName = anyName;
    if (!parseAttributeName(parsedName, namespaceURI, qualifiedName, es))
        return;
    setAttribute(parsedName, value);
}

void Element::removeAttributeInternal(size_t index, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < attributeCount());

    UniqueElementData* elementData = ensureUniqueElementData();

    QualifiedName name = elementData->attributeItem(index)->name();
    AtomicString valueBeingRemoved = elementData->attributeItem(index)->value();

    if (!inSynchronizationOfLazyAttribute) {
        if (!valueBeingRemoved.isNull())
            willModifyAttribute(name, valueBeingRemoved, nullAtom);
    }

    if (RefPtr<Attr> attrNode = attrIfExists(name))
        detachAttrNodeFromElementWithValue(attrNode.get(), elementData->attributeItem(index)->value());

    elementData->removeAttribute(index);

    if (!inSynchronizationOfLazyAttribute)
        didRemoveAttribute(name);
}

void Element::addAttributeInternal(const QualifiedName& name, const AtomicString& value, SynchronizationOfLazyAttribute inSynchronizationOfLazyAttribute)
{
    if (!inSynchronizationOfLazyAttribute)
        willModifyAttribute(name, nullAtom, value);
    ensureUniqueElementData()->addAttribute(name, value);
    if (!inSynchronizationOfLazyAttribute)
        didAddAttribute(name, value);
}

void Element::removeAttribute(const AtomicString& name)
{
    if (!elementData())
        return;

    AtomicString localName = shouldIgnoreAttributeCase(this) ? name.lower() : name;
    size_t index = elementData()->getAttributeItemIndex(localName, false);
    if (index == notFound) {
        if (UNLIKELY(localName == styleAttr) && elementData()->m_styleAttributeIsDirty && isStyledElement())
            removeAllInlineStyleProperties();
        return;
    }

    removeAttributeInternal(index, NotInSynchronizationOfLazyAttribute);
}

void Element::removeAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    removeAttribute(QualifiedName(nullAtom, localName, namespaceURI));
}

PassRefPtr<Attr> Element::getAttributeNode(const AtomicString& localName)
{
    if (!elementData())
        return 0;
    synchronizeAttribute(localName);
    const Attribute* attribute = elementData()->getAttributeItem(localName, shouldIgnoreAttributeCase(this));
    if (!attribute)
        return 0;
    return ensureAttr(attribute->name());
}

PassRefPtr<Attr> Element::getAttributeNodeNS(const AtomicString& namespaceURI, const AtomicString& localName)
{
    if (!elementData())
        return 0;
    QualifiedName qName(nullAtom, localName, namespaceURI);
    synchronizeAttribute(qName);
    const Attribute* attribute = elementData()->getAttributeItem(qName);
    if (!attribute)
        return 0;
    return ensureAttr(attribute->name());
}

bool Element::hasAttribute(const AtomicString& localName) const
{
    if (!elementData())
        return false;
    synchronizeAttribute(localName);
    return elementData()->getAttributeItem(shouldIgnoreAttributeCase(this) ? localName.lower() : localName, false);
}

bool Element::hasAttributeNS(const AtomicString& namespaceURI, const AtomicString& localName) const
{
    if (!elementData())
        return false;
    QualifiedName qName(nullAtom, localName, namespaceURI);
    synchronizeAttribute(qName);
    return elementData()->getAttributeItem(qName);
}

void Element::focus(bool restorePreviousSelection, FocusDirection direction)
{
    if (!inDocument())
        return;

    Document& doc = document();
    if (doc.focusedElement() == this)
        return;

    // If the stylesheets have already been loaded we can reliably check isFocusable.
    // If not, we continue and set the focused node on the focus controller below so
    // that it can be updated soon after attach.
    if (doc.haveStylesheetsLoaded()) {
        doc.updateLayoutIgnorePendingStylesheets();
        if (!isFocusable())
            return;
    }

    if (!supportsFocus())
        return;

    RefPtr<Node> protect;
    if (Page* page = doc.page()) {
        // Focus and change event handlers can cause us to lose our last ref.
        // If a focus event handler changes the focus to a different node it
        // does not make sense to continue and update appearence.
        protect = this;
        if (!page->focusController().setFocusedElement(this, doc.frame(), direction))
            return;
    }

    // Setting the focused node above might have invalidated the layout due to scripts.
    doc.updateLayoutIgnorePendingStylesheets();

    if (!isFocusable()) {
        ensureElementRareData()->setNeedsFocusAppearanceUpdateSoonAfterAttach(true);
        return;
    }

    cancelFocusAppearanceUpdate();
    updateFocusAppearance(restorePreviousSelection);
}

void Element::updateFocusAppearance(bool /*restorePreviousSelection*/)
{
    if (isRootEditableElement()) {
        Frame* frame = document().frame();
        if (!frame)
            return;

        // When focusing an editable element in an iframe, don't reset the selection if it already contains a selection.
        if (this == frame->selection().rootEditableElement())
            return;

        // FIXME: We should restore the previous selection if there is one.
        VisibleSelection newSelection = VisibleSelection(firstPositionInOrBeforeNode(this), DOWNSTREAM);

        if (frame->selection().shouldChangeSelection(newSelection)) {
            frame->selection().setSelection(newSelection);
            frame->selection().revealSelection();
        }
    } else if (renderer() && !renderer()->isWidget())
        renderer()->scrollRectToVisible(boundingBox());
}

void Element::blur()
{
    cancelFocusAppearanceUpdate();
    if (treeScope().adjustedFocusedElement() == this) {
        Document& doc = document();
        if (doc.page())
            doc.page()->focusController().setFocusedElement(0, doc.frame());
        else
            doc.setFocusedElement(0);
    }
}

bool Element::isFocusable() const
{
    return inDocument() && supportsFocus() && !isInert() && rendererIsFocusable();
}

bool Element::isKeyboardFocusable() const
{
    return isFocusable() && tabIndex() >= 0;
}

bool Element::isMouseFocusable() const
{
    return isFocusable();
}

void Element::dispatchFocusEvent(Element* oldFocusedElement, FocusDirection)
{
    RefPtr<FocusEvent> event = FocusEvent::create(eventNames().focusEvent, false, false, document().defaultView(), 0, oldFocusedElement);
    EventDispatcher::dispatchEvent(this, FocusEventDispatchMediator::create(event.release()));
}

void Element::dispatchBlurEvent(Element* newFocusedElement)
{
    RefPtr<FocusEvent> event = FocusEvent::create(eventNames().blurEvent, false, false, document().defaultView(), 0, newFocusedElement);
    EventDispatcher::dispatchEvent(this, BlurEventDispatchMediator::create(event.release()));
}

void Element::dispatchFocusInEvent(const AtomicString& eventType, Element* oldFocusedElement)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(eventType == eventNames().focusinEvent || eventType == eventNames().DOMFocusInEvent);
    dispatchScopedEventDispatchMediator(FocusInEventDispatchMediator::create(FocusEvent::create(eventType, true, false, document().defaultView(), 0, oldFocusedElement)));
}

void Element::dispatchFocusOutEvent(const AtomicString& eventType, Element* newFocusedElement)
{
    ASSERT(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(eventType == eventNames().focusoutEvent || eventType == eventNames().DOMFocusOutEvent);
    dispatchScopedEventDispatchMediator(FocusOutEventDispatchMediator::create(FocusEvent::create(eventType, true, false, document().defaultView(), 0, newFocusedElement)));
}

String Element::innerText()
{
    // We need to update layout, since plainText uses line boxes in the render tree.
    document().updateLayoutIgnorePendingStylesheets();

    if (!renderer())
        return textContent(true);

    return plainText(rangeOfContents(const_cast<Element*>(this)).get());
}

String Element::outerText()
{
    // Getting outerText is the same as getting innerText, only
    // setting is different. You would think this should get the plain
    // text for the outer range, but this is wrong, <br> for instance
    // would return different values for inner and outer text by such
    // a rule, but it doesn't in WinIE, and we want to match that.
    return innerText();
}

String Element::textFromChildren()
{
    Text* firstTextNode = 0;
    bool foundMultipleTextNodes = false;
    unsigned totalLength = 0;

    for (Node* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isTextNode())
            continue;
        Text* text = toText(child);
        if (!firstTextNode)
            firstTextNode = text;
        else
            foundMultipleTextNodes = true;
        unsigned length = text->data().length();
        if (length > std::numeric_limits<unsigned>::max() - totalLength)
            return emptyString();
        totalLength += length;
    }

    if (!firstTextNode)
        return emptyString();

    if (firstTextNode && !foundMultipleTextNodes) {
        firstTextNode->atomize();
        return firstTextNode->data();
    }

    StringBuilder content;
    content.reserveCapacity(totalLength);
    for (Node* child = firstTextNode; child; child = child->nextSibling()) {
        if (!child->isTextNode())
            continue;
        content.append(toText(child)->data());
    }

    ASSERT(content.length() == totalLength);
    return content.toString();
}

// pseudo is used via shadowPseudoId.
const AtomicString& Element::pseudo() const
{
    return getAttribute(pseudoAttr);
}

const AtomicString& Element::part() const
{
    return getAttribute(partAttr);
}

void Element::setPart(const AtomicString& value)
{
    setAttribute(partAttr, value);
}

LayoutSize Element::minimumSizeForResizing() const
{
    return hasRareData() ? elementRareData()->minimumSizeForResizing() : defaultMinimumSizeForResizing();
}

void Element::setMinimumSizeForResizing(const LayoutSize& size)
{
    if (!hasRareData() && size == defaultMinimumSizeForResizing())
        return;
    ensureElementRareData()->setMinimumSizeForResizing(size);
}

RenderStyle* Element::computedStyle(PseudoId pseudoElementSpecifier)
{
    if (PseudoElement* element = pseudoElement(pseudoElementSpecifier))
        return element->computedStyle();

    // FIXME: Find and use the renderer from the pseudo element instead of the actual element so that the 'length'
    // properties, which are only known by the renderer because it did the layout, will be correct and so that the
    // values returned for the ":selection" pseudo-element will be correct.
    if (RenderStyle* usedStyle = renderStyle()) {
        if (pseudoElementSpecifier) {
            RenderStyle* cachedPseudoStyle = usedStyle->getCachedPseudoStyle(pseudoElementSpecifier);
            return cachedPseudoStyle ? cachedPseudoStyle : usedStyle;
         } else
            return usedStyle;
    }

    if (!attached())
        // FIXME: Try to do better than this. Ensure that styleForElement() works for elements that are not in the
        // document tree and figure out when to destroy the computed style for such elements.
        return 0;

    ElementRareData* data = ensureElementRareData();
    if (!data->computedStyle())
        data->setComputedStyle(document().styleForElementIgnoringPendingStylesheets(this));
    return pseudoElementSpecifier ? data->computedStyle()->getCachedPseudoStyle(pseudoElementSpecifier) : data->computedStyle();
}

void Element::setStyleAffectedByEmpty()
{
    ensureElementRareData()->setStyleAffectedByEmpty(true);
}

void Element::setChildrenAffectedByHover(bool value)
{
    if (value || hasRareData())
        ensureElementRareData()->setChildrenAffectedByHover(value);
}

void Element::setChildrenAffectedByActive(bool value)
{
    if (value || hasRareData())
        ensureElementRareData()->setChildrenAffectedByActive(value);
}

void Element::setChildrenAffectedByDrag(bool value)
{
    if (value || hasRareData())
        ensureElementRareData()->setChildrenAffectedByDrag(value);
}

void Element::setChildrenAffectedByFirstChildRules()
{
    ensureElementRareData()->setChildrenAffectedByFirstChildRules(true);
}

void Element::setChildrenAffectedByLastChildRules()
{
    ensureElementRareData()->setChildrenAffectedByLastChildRules(true);
}

void Element::setChildrenAffectedByDirectAdjacentRules()
{
    ensureElementRareData()->setChildrenAffectedByDirectAdjacentRules(true);
}

void Element::setChildrenAffectedByForwardPositionalRules()
{
    ensureElementRareData()->setChildrenAffectedByForwardPositionalRules(true);
}

void Element::setChildrenAffectedByBackwardPositionalRules()
{
    ensureElementRareData()->setChildrenAffectedByBackwardPositionalRules(true);
}

void Element::setChildIndex(unsigned index)
{
    ElementRareData* rareData = ensureElementRareData();
    if (RenderStyle* style = renderStyle())
        style->setUnique();
    rareData->setChildIndex(index);
}

bool Element::hasFlagsSetDuringStylingOfChildren() const
{
    if (!hasRareData())
        return false;
    return rareDataChildrenAffectedByHover()
        || rareDataChildrenAffectedByActive()
        || rareDataChildrenAffectedByDrag()
        || rareDataChildrenAffectedByFirstChildRules()
        || rareDataChildrenAffectedByLastChildRules()
        || rareDataChildrenAffectedByDirectAdjacentRules()
        || rareDataChildrenAffectedByForwardPositionalRules()
        || rareDataChildrenAffectedByBackwardPositionalRules();
}

bool Element::rareDataStyleAffectedByEmpty() const
{
    ASSERT(hasRareData());
    return elementRareData()->styleAffectedByEmpty();
}

bool Element::rareDataChildrenAffectedByHover() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByHover();
}

bool Element::rareDataChildrenAffectedByActive() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByActive();
}

bool Element::rareDataChildrenAffectedByDrag() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByDrag();
}

bool Element::rareDataChildrenAffectedByFirstChildRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByFirstChildRules();
}

bool Element::rareDataChildrenAffectedByLastChildRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByLastChildRules();
}

bool Element::rareDataChildrenAffectedByDirectAdjacentRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByDirectAdjacentRules();
}

bool Element::rareDataChildrenAffectedByForwardPositionalRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByForwardPositionalRules();
}

bool Element::rareDataChildrenAffectedByBackwardPositionalRules() const
{
    ASSERT(hasRareData());
    return elementRareData()->childrenAffectedByBackwardPositionalRules();
}

unsigned Element::rareDataChildIndex() const
{
    ASSERT(hasRareData());
    return elementRareData()->childIndex();
}

void Element::setIsInCanvasSubtree(bool isInCanvasSubtree)
{
    ensureElementRareData()->setIsInCanvasSubtree(isInCanvasSubtree);
}

bool Element::isInCanvasSubtree() const
{
    return hasRareData() && elementRareData()->isInCanvasSubtree();
}

void Element::setIsInsideRegion(bool value)
{
    if (value == isInsideRegion())
        return;

    ensureElementRareData()->setIsInsideRegion(value);
}

bool Element::isInsideRegion() const
{
    return hasRareData() ? elementRareData()->isInsideRegion() : false;
}

void Element::setRegionOversetState(RegionOversetState state)
{
    ensureElementRareData()->setRegionOversetState(state);
}

RegionOversetState Element::regionOversetState() const
{
    return hasRareData() ? elementRareData()->regionOversetState() : RegionUndefined;
}

AtomicString Element::computeInheritedLanguage() const
{
    const Node* n = this;
    AtomicString value;
    // The language property is inherited, so we iterate over the parents to find the first language.
    do {
        if (n->isElementNode()) {
            if (const ElementData* elementData = toElement(n)->elementData()) {
                // Spec: xml:lang takes precedence -- http://www.w3.org/TR/xhtml1/#C_7
                if (const Attribute* attribute = elementData->getAttributeItem(XMLNames::langAttr))
                    value = attribute->value();
                else if (const Attribute* attribute = elementData->getAttributeItem(HTMLNames::langAttr))
                    value = attribute->value();
            }
        } else if (n->isDocumentNode()) {
            // checking the MIME content-language
            value = toDocument(n)->contentLanguage();
        }

        n = n->parentNode();
    } while (n && value.isNull());

    return value;
}

Locale& Element::locale() const
{
    return document().getCachedLocale(computeInheritedLanguage());
}

void Element::cancelFocusAppearanceUpdate()
{
    if (hasRareData())
        elementRareData()->setNeedsFocusAppearanceUpdateSoonAfterAttach(false);
    if (document().focusedElement() == this)
        document().cancelFocusAppearanceUpdate();
}

void Element::normalizeAttributes()
{
    if (!hasAttributes())
        return;
    for (unsigned i = 0; i < attributeCount(); ++i) {
        if (RefPtr<Attr> attr = attrIfExists(attributeItem(i)->name()))
            attr->normalize();
    }
}

void Element::updatePseudoElement(PseudoId pseudoId, StyleRecalcChange change)
{
    PseudoElement* element = pseudoElement(pseudoId);
    if (element && (needsStyleRecalc() || shouldRecalcStyle(change, element))) {
        // PseudoElement styles hang off their parent element's style so if we needed
        // a style recalc we should Force one on the pseudo.
        element->recalcStyle(needsStyleRecalc() ? Force : change);

        // Wait until our parent is not displayed or pseudoElementRendererIsNeeded
        // is false, otherwise we could continously create and destroy PseudoElements
        // when RenderObject::isChildAllowed on our parent returns false for the
        // PseudoElement's renderer for each style recalc.
        if (!renderer() || !pseudoElementRendererIsNeeded(renderer()->getCachedPseudoStyle(pseudoId)))
            elementRareData()->setPseudoElement(pseudoId, 0);
    } else if (change >= Inherit || needsStyleRecalc())
        createPseudoElementIfNeeded(pseudoId);
}

void Element::createPseudoElementIfNeeded(PseudoId pseudoId)
{
    if (pseudoId == BACKDROP && !isInTopLayer())
        return;

    if (!renderer() || !pseudoElementRendererIsNeeded(renderer()->getCachedPseudoStyle(pseudoId)))
        return;

    if (!renderer()->canHaveGeneratedChildren())
        return;

    ASSERT(!isPseudoElement());
    RefPtr<PseudoElement> element = PseudoElement::create(this, pseudoId);
    if (pseudoId == BACKDROP)
        document().addToTopLayer(element.get(), this);
    element->attach();

    ensureElementRareData()->setPseudoElement(pseudoId, element.release());
}

PseudoElement* Element::pseudoElement(PseudoId pseudoId) const
{
    return hasRareData() ? elementRareData()->pseudoElement(pseudoId) : 0;
}

RenderObject* Element::pseudoElementRenderer(PseudoId pseudoId) const
{
    if (PseudoElement* element = pseudoElement(pseudoId))
        return element->renderer();
    return 0;
}

bool Element::webkitMatchesSelector(const String& selector, ExceptionState& es)
{
    if (selector.isEmpty()) {
        es.throwDOMException(SyntaxError);
        return false;
    }

    SelectorQuery* selectorQuery = document().selectorQueryCache()->add(selector, document(), es);
    if (!selectorQuery)
        return false;
    return selectorQuery->matches(this);
}

DOMTokenList* Element::classList()
{
    ElementRareData* data = ensureElementRareData();
    if (!data->classList())
        data->setClassList(ClassList::create(this));
    return data->classList();
}

DOMStringMap* Element::dataset()
{
    ElementRareData* data = ensureElementRareData();
    if (!data->dataset())
        data->setDataset(DatasetDOMStringMap::create(this));
    return data->dataset();
}

KURL Element::getURLAttribute(const QualifiedName& name) const
{
#if !ASSERT_DISABLED
    if (elementData()) {
        if (const Attribute* attribute = getAttributeItem(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    return document().completeURL(stripLeadingAndTrailingHTMLSpaces(getAttribute(name)));
}

KURL Element::getNonEmptyURLAttribute(const QualifiedName& name) const
{
#if !ASSERT_DISABLED
    if (elementData()) {
        if (const Attribute* attribute = getAttributeItem(name))
            ASSERT(isURLAttribute(*attribute));
    }
#endif
    String value = stripLeadingAndTrailingHTMLSpaces(getAttribute(name));
    if (value.isEmpty())
        return KURL();
    return document().completeURL(value);
}

int Element::getIntegralAttribute(const QualifiedName& attributeName) const
{
    return getAttribute(attributeName).string().toInt();
}

void Element::setIntegralAttribute(const QualifiedName& attributeName, int value)
{
    // FIXME: Need an AtomicString version of String::number.
    setAttribute(attributeName, String::number(value));
}

unsigned Element::getUnsignedIntegralAttribute(const QualifiedName& attributeName) const
{
    return getAttribute(attributeName).string().toUInt();
}

void Element::setUnsignedIntegralAttribute(const QualifiedName& attributeName, unsigned value)
{
    // FIXME: Need an AtomicString version of String::number.
    setAttribute(attributeName, String::number(value));
}

bool Element::childShouldCreateRenderer(const Node& child) const
{
    // Only create renderers for SVG elements whose parents are SVG elements, or for proper <svg xmlns="svgNS"> subdocuments.
    if (child.isSVGElement())
        return child.hasTagName(SVGNames::svgTag) || isSVGElement();

    return ContainerNode::childShouldCreateRenderer(child);
}

void Element::webkitRequestFullscreen()
{
    FullscreenElementStack::from(&document())->requestFullScreenForElement(this, ALLOW_KEYBOARD_INPUT, FullscreenElementStack::EnforceIFrameAllowFullScreenRequirement);
}

void Element::webkitRequestFullScreen(unsigned short flags)
{
    FullscreenElementStack::from(&document())->requestFullScreenForElement(this, (flags | LEGACY_MOZILLA_REQUEST), FullscreenElementStack::EnforceIFrameAllowFullScreenRequirement);
}

bool Element::containsFullScreenElement() const
{
    return hasRareData() && elementRareData()->containsFullScreenElement();
}

void Element::setContainsFullScreenElement(bool flag)
{
    ensureElementRareData()->setContainsFullScreenElement(flag);
    setNeedsStyleRecalc(SubtreeStyleChange);
}

static Element* parentCrossingFrameBoundaries(Element* element)
{
    ASSERT(element);
    return element->parentElement() ? element->parentElement() : element->document().ownerElement();
}

void Element::setContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool flag)
{
    Element* element = this;
    while ((element = parentCrossingFrameBoundaries(element)))
        element->setContainsFullScreenElement(flag);
}

bool Element::isInTopLayer() const
{
    return hasRareData() && elementRareData()->isInTopLayer();
}

void Element::setIsInTopLayer(bool inTopLayer)
{
    if (isInTopLayer() == inTopLayer)
        return;
    ensureElementRareData()->setIsInTopLayer(inTopLayer);

    // We must ensure a reattach occurs so the renderer is inserted in the correct sibling order under RenderView according to its
    // top layer position, or in its usual place if not in the top layer.
    lazyReattachIfAttached();
}

void Element::webkitRequestPointerLock()
{
    if (document().page())
        document().page()->pointerLockController().requestPointerLock(this);
}

SpellcheckAttributeState Element::spellcheckAttributeState() const
{
    const AtomicString& value = getAttribute(HTMLNames::spellcheckAttr);
    if (value == nullAtom)
        return SpellcheckAttributeDefault;
    if (equalIgnoringCase(value, "true") || equalIgnoringCase(value, ""))
        return SpellcheckAttributeTrue;
    if (equalIgnoringCase(value, "false"))
        return SpellcheckAttributeFalse;

    return SpellcheckAttributeDefault;
}

bool Element::isSpellCheckingEnabled() const
{
    for (const Element* element = this; element; element = element->parentOrShadowHostElement()) {
        switch (element->spellcheckAttributeState()) {
        case SpellcheckAttributeTrue:
            return true;
        case SpellcheckAttributeFalse:
            return false;
        case SpellcheckAttributeDefault:
            break;
        }
    }

    return true;
}

RenderRegion* Element::renderRegion() const
{
    if (renderer() && renderer()->isRenderRegion())
        return toRenderRegion(renderer());

    return 0;
}

bool Element::shouldMoveToFlowThread(RenderStyle* styleToUse) const
{
    ASSERT(styleToUse);

    if (FullscreenElementStack::isActiveFullScreenElement(this))
        return false;

    if (isInShadowTree())
        return false;

    if (styleToUse->flowThread().isEmpty())
        return false;

    return !isRegisteredWithNamedFlow();
}

const AtomicString& Element::webkitRegionOverset() const
{
    document().updateLayoutIgnorePendingStylesheets();

    DEFINE_STATIC_LOCAL(AtomicString, undefinedState, ("undefined", AtomicString::ConstructFromLiteral));
    if (!RuntimeEnabledFeatures::cssRegionsEnabled() || !renderRegion())
        return undefinedState;

    switch (renderRegion()->regionOversetState()) {
    case RegionFit: {
        DEFINE_STATIC_LOCAL(AtomicString, fitState, ("fit", AtomicString::ConstructFromLiteral));
        return fitState;
    }
    case RegionEmpty: {
        DEFINE_STATIC_LOCAL(AtomicString, emptyState, ("empty", AtomicString::ConstructFromLiteral));
        return emptyState;
    }
    case RegionOverset: {
        DEFINE_STATIC_LOCAL(AtomicString, overflowState, ("overset", AtomicString::ConstructFromLiteral));
        return overflowState;
    }
    case RegionUndefined:
        return undefinedState;
    }

    ASSERT_NOT_REACHED();
    return undefinedState;
}

Vector<RefPtr<Range> > Element::webkitGetRegionFlowRanges() const
{
    document().updateLayoutIgnorePendingStylesheets();

    Vector<RefPtr<Range> > rangeObjects;
    if (RuntimeEnabledFeatures::cssRegionsEnabled() && renderer() && renderer()->isRenderRegion()) {
        RenderRegion* region = toRenderRegion(renderer());
        if (region->isValid())
            region->getRanges(rangeObjects);
    }

    return rangeObjects;
}

#ifndef NDEBUG
bool Element::fastAttributeLookupAllowed(const QualifiedName& name) const
{
    if (name == HTMLNames::styleAttr)
        return false;

    if (isSVGElement())
        return !static_cast<const SVGElement*>(this)->isAnimatableAttribute(name);

    return true;
}
#endif

#ifdef DUMP_NODE_STATISTICS
bool Element::hasNamedNodeMap() const
{
    return hasRareData() && elementRareData()->attributeMap();
}
#endif

inline void Element::updateName(const AtomicString& oldName, const AtomicString& newName)
{
    if (!inDocument() || isInShadowTree())
        return;

    if (oldName == newName)
        return;

    if (shouldRegisterAsNamedItem())
        updateNamedItemRegistration(oldName, newName);
}

inline void Element::updateId(const AtomicString& oldId, const AtomicString& newId)
{
    if (!isInTreeScope())
        return;

    if (oldId == newId)
        return;

    updateId(treeScope(), oldId, newId);
}

inline void Element::updateId(TreeScope& scope, const AtomicString& oldId, const AtomicString& newId)
{
    ASSERT(isInTreeScope());
    ASSERT(oldId != newId);

    if (!oldId.isEmpty())
        scope.removeElementById(oldId, this);
    if (!newId.isEmpty())
        scope.addElementById(newId, this);

    if (shouldRegisterAsExtraNamedItem())
        updateExtraNamedItemRegistration(oldId, newId);
}

void Element::updateLabel(TreeScope& scope, const AtomicString& oldForAttributeValue, const AtomicString& newForAttributeValue)
{
    ASSERT(hasTagName(labelTag));

    if (!inDocument())
        return;

    if (oldForAttributeValue == newForAttributeValue)
        return;

    if (!oldForAttributeValue.isEmpty())
        scope.removeLabel(oldForAttributeValue, toHTMLLabelElement(this));
    if (!newForAttributeValue.isEmpty())
        scope.addLabel(newForAttributeValue, toHTMLLabelElement(this));
}

static bool hasSelectorForAttribute(Document* document, const AtomicString& localName)
{
    return document->styleResolver() && document->styleResolver()->ruleFeatureSet().hasSelectorForAttribute(localName);
}

void Element::willModifyAttribute(const QualifiedName& name, const AtomicString& oldValue, const AtomicString& newValue)
{
    if (isIdAttributeName(name))
        updateId(oldValue, newValue);
    else if (name == HTMLNames::nameAttr)
        updateName(oldValue, newValue);
    else if (name == HTMLNames::forAttr && hasTagName(labelTag)) {
        TreeScope& scope = treeScope();
        if (scope.shouldCacheLabelsByForAttribute())
            updateLabel(scope, oldValue, newValue);
    }

    if (oldValue != newValue) {
        if (attached() && hasSelectorForAttribute(&document(), name.localName()))
           setNeedsStyleRecalc();

        if (isUpgradedCustomElement())
            CustomElement::attributeDidChange(this, name.localName(), oldValue, newValue);
    }

    if (OwnPtr<MutationObserverInterestGroup> recipients = MutationObserverInterestGroup::createForAttributesMutation(this, name))
        recipients->enqueueMutationRecord(MutationRecord::createAttributes(this, name, oldValue));

    InspectorInstrumentation::willModifyDOMAttr(&document(), this, oldValue, newValue);
}

void Element::didAddAttribute(const QualifiedName& name, const AtomicString& value)
{
    attributeChanged(name, value);
    InspectorInstrumentation::didModifyDOMAttr(&document(), this, name.localName(), value);
    dispatchSubtreeModifiedEvent();
}

void Element::didModifyAttribute(const QualifiedName& name, const AtomicString& value)
{
    attributeChanged(name, value);
    InspectorInstrumentation::didModifyDOMAttr(&document(), this, name.localName(), value);
    // Do not dispatch a DOMSubtreeModified event here; see bug 81141.
}

void Element::didRemoveAttribute(const QualifiedName& name)
{
    attributeChanged(name, nullAtom);
    InspectorInstrumentation::didRemoveDOMAttr(&document(), this, name.localName());
    dispatchSubtreeModifiedEvent();
}

void Element::didMoveToNewDocument(Document* oldDocument)
{
    Node::didMoveToNewDocument(oldDocument);

    // If the documents differ by quirks mode then they differ by case sensitivity
    // for class and id names so we need to go through the attribute change logic
    // to pick up the new casing in the ElementData.
    if (oldDocument->inQuirksMode() != document().inQuirksMode()) {
        if (hasID())
            setIdAttribute(getIdAttribute());
        if (hasClass())
            setAttribute(HTMLNames::classAttr, getClassAttribute());
    }
}

void Element::updateNamedItemRegistration(const AtomicString& oldName, const AtomicString& newName)
{
    if (!document().isHTMLDocument())
        return;

    if (!oldName.isEmpty())
        toHTMLDocument(document()).removeNamedItem(oldName);

    if (!newName.isEmpty())
        toHTMLDocument(document()).addNamedItem(newName);
}

void Element::updateExtraNamedItemRegistration(const AtomicString& oldId, const AtomicString& newId)
{
    if (!document().isHTMLDocument())
        return;

    if (!oldId.isEmpty())
        toHTMLDocument(document()).removeExtraNamedItem(oldId);

    if (!newId.isEmpty())
        toHTMLDocument(document()).addExtraNamedItem(newId);
}

PassRefPtr<HTMLCollection> Element::ensureCachedHTMLCollection(CollectionType type)
{
    if (HTMLCollection* collection = cachedHTMLCollection(type))
        return collection;

    RefPtr<HTMLCollection> collection;
    if (type == TableRows) {
        ASSERT(hasTagName(tableTag));
        return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLTableRowsCollection>(this, type);
    } else if (type == SelectOptions) {
        ASSERT(hasTagName(selectTag));
        return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLOptionsCollection>(this, type);
    } else if (type == FormControls) {
        ASSERT(hasTagName(formTag) || hasTagName(fieldsetTag));
        return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLFormControlsCollection>(this, type);
    }
    return ensureRareData()->ensureNodeLists()->addCacheWithAtomicName<HTMLCollection>(this, type);
}

static void scheduleLayerUpdateCallback(Node* node)
{
    // Notify the renderer even is the styles are identical since it may need to
    // create or destroy a RenderLayer.
    node->setNeedsStyleRecalc(LocalStyleChange, StyleChangeFromRenderer);
}

void Element::scheduleLayerUpdate()
{
    if (postAttachCallbacksAreSuspended())
        queuePostAttachCallback(scheduleLayerUpdateCallback, this);
    else
        scheduleLayerUpdateCallback(this);
}

HTMLCollection* Element::cachedHTMLCollection(CollectionType type)
{
    return hasRareData() && rareData()->nodeLists() ? rareData()->nodeLists()->cacheWithAtomicName<HTMLCollection>(type) : 0;
}

IntSize Element::savedLayerScrollOffset() const
{
    return hasRareData() ? elementRareData()->savedLayerScrollOffset() : IntSize();
}

void Element::setSavedLayerScrollOffset(const IntSize& size)
{
    if (size.isZero() && !hasRareData())
        return;
    ensureElementRareData()->setSavedLayerScrollOffset(size);
}

PassRefPtr<Attr> Element::attrIfExists(const QualifiedName& name)
{
    if (AttrNodeList* attrNodeList = attrNodeListForElement(this))
        return findAttrNodeInList(attrNodeList, name);
    return 0;
}

PassRefPtr<Attr> Element::ensureAttr(const QualifiedName& name)
{
    AttrNodeList* attrNodeList = ensureAttrNodeListForElement(this);
    RefPtr<Attr> attrNode = findAttrNodeInList(attrNodeList, name);
    if (!attrNode) {
        attrNode = Attr::create(*this, name);
        treeScope().adoptIfNeeded(attrNode.get());
        attrNodeList->append(attrNode);
    }
    return attrNode.release();
}

void Element::detachAttrNodeFromElementWithValue(Attr* attrNode, const AtomicString& value)
{
    ASSERT(hasSyntheticAttrChildNodes());
    attrNode->detachFromElementWithValue(value);

    AttrNodeList* attrNodeList = attrNodeListForElement(this);
    for (unsigned i = 0; i < attrNodeList->size(); ++i) {
        if (attrNodeList->at(i)->qualifiedName() == attrNode->qualifiedName()) {
            attrNodeList->remove(i);
            if (attrNodeList->isEmpty())
                removeAttrNodeListForElement(this);
            return;
        }
    }
    ASSERT_NOT_REACHED();
}

void Element::detachAllAttrNodesFromElement()
{
    AttrNodeList* attrNodeList = attrNodeListForElement(this);
    ASSERT(attrNodeList);

    for (unsigned i = 0; i < attributeCount(); ++i) {
        const Attribute* attribute = attributeItem(i);
        if (RefPtr<Attr> attrNode = findAttrNodeInList(attrNodeList, attribute->name()))
            attrNode->detachFromElementWithValue(attribute->value());
    }

    removeAttrNodeListForElement(this);
}

void Element::willRecalcStyle(StyleRecalcChange)
{
    ASSERT(hasCustomStyleCallbacks());
}

void Element::didRecalcStyle(StyleRecalcChange)
{
    ASSERT(hasCustomStyleCallbacks());
}


PassRefPtr<RenderStyle> Element::customStyleForRenderer()
{
    ASSERT(hasCustomStyleCallbacks());
    return 0;
}

void Element::cloneAttributesFromElement(const Element& other)
{
    if (hasSyntheticAttrChildNodes())
        detachAllAttrNodesFromElement();

    other.synchronizeAllAttributes();
    if (!other.m_elementData) {
        m_elementData.clear();
        return;
    }

    const AtomicString& oldID = getIdAttribute();
    const AtomicString& newID = other.getIdAttribute();

    if (!oldID.isNull() || !newID.isNull())
        updateId(oldID, newID);

    const AtomicString& oldName = getNameAttribute();
    const AtomicString& newName = other.getNameAttribute();

    if (!oldName.isNull() || !newName.isNull())
        updateName(oldName, newName);

    // Quirks mode makes class and id not case sensitive. We can't share the ElementData
    // if the idForStyleResolution and the className need different casing.
    bool ownerDocumentsHaveDifferentCaseSensitivity = false;
    if (other.hasClass() || other.hasID())
        ownerDocumentsHaveDifferentCaseSensitivity = other.document().inQuirksMode() != document().inQuirksMode();

    // If 'other' has a mutable ElementData, convert it to an immutable one so we can share it between both elements.
    // We can only do this if there is no CSSOM wrapper for other's inline style, and there are no presentation attributes,
    // and sharing the data won't result in different case sensitivity of class or id.
    if (other.m_elementData->isUnique()
        && !ownerDocumentsHaveDifferentCaseSensitivity
        && !other.m_elementData->presentationAttributeStyle()
        && (!other.m_elementData->inlineStyle() || !other.m_elementData->inlineStyle()->hasCSSOMWrapper()))
        const_cast<Element&>(other).m_elementData = static_cast<const UniqueElementData*>(other.m_elementData.get())->makeShareableCopy();

    if (!other.m_elementData->isUnique() && !ownerDocumentsHaveDifferentCaseSensitivity)
        m_elementData = other.m_elementData;
    else
        m_elementData = other.m_elementData->makeUniqueCopy();

    for (unsigned i = 0; i < m_elementData->length(); ++i) {
        const Attribute* attribute = const_cast<const ElementData*>(m_elementData.get())->attributeItem(i);
        attributeChangedFromParserOrByCloning(attribute->name(), attribute->value(), ModifiedByCloning);
    }
}

void Element::cloneDataFromElement(const Element& other)
{
    cloneAttributesFromElement(other);
    copyNonAttributePropertiesFromElement(other);
}

void Element::createUniqueElementData()
{
    if (!m_elementData)
        m_elementData = UniqueElementData::create();
    else {
        ASSERT(!m_elementData->isUnique());
        m_elementData = static_cast<ShareableElementData*>(m_elementData.get())->makeUniqueCopy();
    }
}

InputMethodContext* Element::inputMethodContext()
{
    return ensureElementRareData()->ensureInputMethodContext(toHTMLElement(this));
}

bool Element::hasPendingResources() const
{
    return hasRareData() && elementRareData()->hasPendingResources();
}

void Element::setHasPendingResources()
{
    ensureElementRareData()->setHasPendingResources(true);
}

void Element::clearHasPendingResources()
{
    ensureElementRareData()->setHasPendingResources(false);
}

struct PresentationAttributeCacheKey {
    PresentationAttributeCacheKey() : tagName(0) { }
    StringImpl* tagName;
    // Only the values need refcounting.
    Vector<pair<StringImpl*, AtomicString>, 3> attributesAndValues;
};

struct PresentationAttributeCacheEntry {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PresentationAttributeCacheKey key;
    RefPtr<StylePropertySet> value;
};

typedef HashMap<unsigned, OwnPtr<PresentationAttributeCacheEntry>, AlreadyHashed> PresentationAttributeCache;

static bool operator!=(const PresentationAttributeCacheKey& a, const PresentationAttributeCacheKey& b)
{
    if (a.tagName != b.tagName)
        return true;
    return a.attributesAndValues != b.attributesAndValues;
}

static PresentationAttributeCache& presentationAttributeCache()
{
    DEFINE_STATIC_LOCAL(PresentationAttributeCache, cache, ());
    return cache;
}

class PresentationAttributeCacheCleaner {
    WTF_MAKE_NONCOPYABLE(PresentationAttributeCacheCleaner); WTF_MAKE_FAST_ALLOCATED;
public:
    PresentationAttributeCacheCleaner()
        : m_hitCount(0)
        , m_cleanTimer(this, &PresentationAttributeCacheCleaner::cleanCache)
    {
    }

    void didHitPresentationAttributeCache()
    {
        if (presentationAttributeCache().size() < minimumPresentationAttributeCacheSizeForCleaning)
            return;

        m_hitCount++;

        if (!m_cleanTimer.isActive())
            m_cleanTimer.startOneShot(presentationAttributeCacheCleanTimeInSeconds);
    }

private:
    static const unsigned presentationAttributeCacheCleanTimeInSeconds = 60;
    static const int minimumPresentationAttributeCacheSizeForCleaning = 100;
    static const unsigned minimumPresentationAttributeCacheHitCountPerMinute = (100 * presentationAttributeCacheCleanTimeInSeconds) / 60;

    void cleanCache(Timer<PresentationAttributeCacheCleaner>* timer)
    {
        ASSERT_UNUSED(timer, timer == &m_cleanTimer);
        unsigned hitCount = m_hitCount;
        m_hitCount = 0;
        if (hitCount > minimumPresentationAttributeCacheHitCountPerMinute)
            return;
        presentationAttributeCache().clear();
    }

    unsigned m_hitCount;
    Timer<PresentationAttributeCacheCleaner> m_cleanTimer;
};

static PresentationAttributeCacheCleaner& presentationAttributeCacheCleaner()
{
    DEFINE_STATIC_LOCAL(PresentationAttributeCacheCleaner, cleaner, ());
    return cleaner;
}

void Element::synchronizeStyleAttributeInternal() const
{
    ASSERT(isStyledElement());
    ASSERT(elementData());
    ASSERT(elementData()->m_styleAttributeIsDirty);
    elementData()->m_styleAttributeIsDirty = false;
    if (const StylePropertySet* inlineStyle = this->inlineStyle())
        const_cast<Element*>(this)->setSynchronizedLazyAttribute(styleAttr, inlineStyle->asText());
}

CSSStyleDeclaration* Element::style()
{
    if (!isStyledElement())
        return 0;
    return ensureMutableInlineStyle()->ensureInlineCSSStyleDeclaration(this);
}

MutableStylePropertySet* Element::ensureMutableInlineStyle()
{
    ASSERT(isStyledElement());
    RefPtr<StylePropertySet>& inlineStyle = ensureUniqueElementData()->m_inlineStyle;
    if (!inlineStyle)
        inlineStyle = MutableStylePropertySet::create(strictToCSSParserMode(isHTMLElement() && !document().inQuirksMode()));
    else if (!inlineStyle->isMutable())
        inlineStyle = inlineStyle->mutableCopy();
    ASSERT(inlineStyle->isMutable());
    return static_cast<MutableStylePropertySet*>(inlineStyle.get());
}

PropertySetCSSStyleDeclaration* Element::inlineStyleCSSOMWrapper()
{
    if (!inlineStyle() || !inlineStyle()->hasCSSOMWrapper())
        return 0;
    PropertySetCSSStyleDeclaration* cssomWrapper = ensureMutableInlineStyle()->cssStyleDeclaration();
    ASSERT(cssomWrapper && cssomWrapper->parentElement() == this);
    return cssomWrapper;
}

inline void Element::setInlineStyleFromString(const AtomicString& newStyleString)
{
    ASSERT(isStyledElement());
    RefPtr<StylePropertySet>& inlineStyle = elementData()->m_inlineStyle;

    // Avoid redundant work if we're using shared attribute data with already parsed inline style.
    if (inlineStyle && !elementData()->isUnique())
        return;

    // We reconstruct the property set instead of mutating if there is no CSSOM wrapper.
    // This makes wrapperless property sets immutable and so cacheable.
    if (inlineStyle && !inlineStyle->isMutable())
        inlineStyle.clear();

    if (!inlineStyle) {
        inlineStyle = CSSParser::parseInlineStyleDeclaration(newStyleString, this);
    } else {
        ASSERT(inlineStyle->isMutable());
        static_pointer_cast<MutableStylePropertySet>(inlineStyle)->parseDeclaration(newStyleString, document().elementSheet()->contents());
    }
}

void Element::styleAttributeChanged(const AtomicString& newStyleString, AttributeModificationReason modificationReason)
{
    ASSERT(isStyledElement());
    WTF::OrdinalNumber startLineNumber = WTF::OrdinalNumber::beforeFirst();
    if (document().scriptableDocumentParser() && !document().isInDocumentWrite())
        startLineNumber = document().scriptableDocumentParser()->lineNumber();

    if (newStyleString.isNull()) {
        if (PropertySetCSSStyleDeclaration* cssomWrapper = inlineStyleCSSOMWrapper())
            cssomWrapper->clearParentElement();
        ensureUniqueElementData()->m_inlineStyle.clear();
    } else if (modificationReason == ModifiedByCloning || document().contentSecurityPolicy()->allowInlineStyle(document().url(), startLineNumber)) {
        setInlineStyleFromString(newStyleString);
    }

    elementData()->m_styleAttributeIsDirty = false;

    setNeedsStyleRecalc(LocalStyleChange);
    InspectorInstrumentation::didInvalidateStyleAttr(&document(), this);
}

void Element::inlineStyleChanged()
{
    ASSERT(isStyledElement());
    setNeedsStyleRecalc(LocalStyleChange);
    ASSERT(elementData());
    elementData()->m_styleAttributeIsDirty = true;
    InspectorInstrumentation::didInvalidateStyleAttr(&document(), this);
}

bool Element::setInlineStyleProperty(CSSPropertyID propertyID, CSSValueID identifier, bool important)
{
    ASSERT(isStyledElement());
    ensureMutableInlineStyle()->setProperty(propertyID, cssValuePool().createIdentifierValue(identifier), important);
    inlineStyleChanged();
    return true;
}

bool Element::setInlineStyleProperty(CSSPropertyID propertyID, CSSPropertyID identifier, bool important)
{
    ASSERT(isStyledElement());
    ensureMutableInlineStyle()->setProperty(propertyID, cssValuePool().createIdentifierValue(identifier), important);
    inlineStyleChanged();
    return true;
}

bool Element::setInlineStyleProperty(CSSPropertyID propertyID, double value, CSSPrimitiveValue::UnitTypes unit, bool important)
{
    ASSERT(isStyledElement());
    ensureMutableInlineStyle()->setProperty(propertyID, cssValuePool().createValue(value, unit), important);
    inlineStyleChanged();
    return true;
}

bool Element::setInlineStyleProperty(CSSPropertyID propertyID, const String& value, bool important)
{
    ASSERT(isStyledElement());
    bool changes = ensureMutableInlineStyle()->setProperty(propertyID, value, important, document().elementSheet()->contents());
    if (changes)
        inlineStyleChanged();
    return changes;
}

bool Element::removeInlineStyleProperty(CSSPropertyID propertyID)
{
    ASSERT(isStyledElement());
    if (!inlineStyle())
        return false;
    bool changes = ensureMutableInlineStyle()->removeProperty(propertyID);
    if (changes)
        inlineStyleChanged();
    return changes;
}

void Element::removeAllInlineStyleProperties()
{
    ASSERT(isStyledElement());
    if (!inlineStyle() || inlineStyle()->isEmpty())
        return;
    ensureMutableInlineStyle()->clear();
    inlineStyleChanged();
}

void Element::addSubresourceAttributeURLs(ListHashSet<KURL>& urls) const
{
    ASSERT(isStyledElement());
    if (const StylePropertySet* inlineStyle = elementData() ? elementData()->inlineStyle() : 0)
        inlineStyle->addSubresourceStyleURLs(urls, document().elementSheet()->contents());
}

static inline bool attributeNameSort(const pair<StringImpl*, AtomicString>& p1, const pair<StringImpl*, AtomicString>& p2)
{
    // Sort based on the attribute name pointers. It doesn't matter what the order is as long as it is always the same.
    return p1.first < p2.first;
}

void Element::makePresentationAttributeCacheKey(PresentationAttributeCacheKey& result) const
{
    ASSERT(isStyledElement());
    // FIXME: Enable for SVG.
    if (namespaceURI() != xhtmlNamespaceURI)
        return;
    // Interpretation of the size attributes on <input> depends on the type attribute.
    if (hasTagName(inputTag))
        return;
    unsigned size = attributeCount();
    for (unsigned i = 0; i < size; ++i) {
        const Attribute* attribute = attributeItem(i);
        if (!isPresentationAttribute(attribute->name()))
            continue;
        if (!attribute->namespaceURI().isNull())
            return;
        // FIXME: Background URL may depend on the base URL and can't be shared. Disallow caching.
        if (attribute->name() == backgroundAttr)
            return;
        result.attributesAndValues.append(std::make_pair(attribute->localName().impl(), attribute->value()));
    }
    if (result.attributesAndValues.isEmpty())
        return;
    // Attribute order doesn't matter. Sort for easy equality comparison.
    std::sort(result.attributesAndValues.begin(), result.attributesAndValues.end(), attributeNameSort);
    // The cache key is non-null when the tagName is set.
    result.tagName = localName().impl();
}

static unsigned computePresentationAttributeCacheHash(const PresentationAttributeCacheKey& key)
{
    if (!key.tagName)
        return 0;
    ASSERT(key.attributesAndValues.size());
    unsigned attributeHash = StringHasher::hashMemory(key.attributesAndValues.data(), key.attributesAndValues.size() * sizeof(key.attributesAndValues[0]));
    return WTF::pairIntHash(key.tagName->existingHash(), attributeHash);
}

void Element::rebuildPresentationAttributeStyle()
{
    ASSERT(isStyledElement());
    PresentationAttributeCacheKey cacheKey;
    makePresentationAttributeCacheKey(cacheKey);

    unsigned cacheHash = computePresentationAttributeCacheHash(cacheKey);

    PresentationAttributeCache::iterator cacheIterator;
    if (cacheHash) {
        cacheIterator = presentationAttributeCache().add(cacheHash, nullptr).iterator;
        if (cacheIterator->value && cacheIterator->value->key != cacheKey)
            cacheHash = 0;
    } else {
        cacheIterator = presentationAttributeCache().end();
    }

    RefPtr<StylePropertySet> style;
    if (cacheHash && cacheIterator->value) {
        style = cacheIterator->value->value;
        presentationAttributeCacheCleaner().didHitPresentationAttributeCache();
    } else {
        style = MutableStylePropertySet::create(isSVGElement() ? SVGAttributeMode : CSSQuirksMode);
        unsigned size = attributeCount();
        for (unsigned i = 0; i < size; ++i) {
            const Attribute* attribute = attributeItem(i);
            collectStyleForPresentationAttribute(attribute->name(), attribute->value(), static_cast<MutableStylePropertySet*>(style.get()));
        }
    }

    // ShareableElementData doesn't store presentation attribute style, so make sure we have a UniqueElementData.
    UniqueElementData* elementData = ensureUniqueElementData();

    elementData->m_presentationAttributeStyleIsDirty = false;
    elementData->m_presentationAttributeStyle = style->isEmpty() ? 0 : style;

    if (!cacheHash || cacheIterator->value)
        return;

    OwnPtr<PresentationAttributeCacheEntry> newEntry = adoptPtr(new PresentationAttributeCacheEntry);
    newEntry->key = cacheKey;
    newEntry->value = style.release();

    static const int presentationAttributeCacheMaximumSize = 4096;
    if (presentationAttributeCache().size() > presentationAttributeCacheMaximumSize) {
        // Start building from scratch if the cache ever gets big.
        presentationAttributeCache().clear();
        presentationAttributeCache().set(cacheHash, newEntry.release());
    } else {
        cacheIterator->value = newEntry.release();
    }
}

void Element::addPropertyToPresentationAttributeStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, CSSValueID identifier)
{
    ASSERT(isStyledElement());
    style->setProperty(propertyID, cssValuePool().createIdentifierValue(identifier));
}

void Element::addPropertyToPresentationAttributeStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, double value, CSSPrimitiveValue::UnitTypes unit)
{
    ASSERT(isStyledElement());
    style->setProperty(propertyID, cssValuePool().createValue(value, unit));
}

void Element::addPropertyToPresentationAttributeStyle(MutableStylePropertySet* style, CSSPropertyID propertyID, const String& value)
{
    ASSERT(isStyledElement());
    style->setProperty(propertyID, value, false, document().elementSheet()->contents());
}

void ElementData::deref()
{
    if (!derefBase())
        return;

    if (m_isUnique)
        delete static_cast<UniqueElementData*>(this);
    else
        delete static_cast<ShareableElementData*>(this);
}

ElementData::ElementData()
    : m_isUnique(true)
    , m_arraySize(0)
    , m_presentationAttributeStyleIsDirty(false)
    , m_styleAttributeIsDirty(false)
    , m_animatedSVGAttributesAreDirty(false)
{
}

ElementData::ElementData(unsigned arraySize)
    : m_isUnique(false)
    , m_arraySize(arraySize)
    , m_presentationAttributeStyleIsDirty(false)
    , m_styleAttributeIsDirty(false)
    , m_animatedSVGAttributesAreDirty(false)
{
}

struct SameSizeAsElementData : public RefCounted<SameSizeAsElementData> {
    unsigned bitfield;
    void* refPtrs[3];
};

COMPILE_ASSERT(sizeof(ElementData) == sizeof(SameSizeAsElementData), element_attribute_data_should_stay_small);

static size_t sizeForShareableElementDataWithAttributeCount(unsigned count)
{
    return sizeof(ShareableElementData) + sizeof(Attribute) * count;
}

PassRefPtr<ShareableElementData> ShareableElementData::createWithAttributes(const Vector<Attribute>& attributes)
{
    void* slot = WTF::fastMalloc(sizeForShareableElementDataWithAttributeCount(attributes.size()));
    return adoptRef(new (slot) ShareableElementData(attributes));
}

PassRefPtr<UniqueElementData> UniqueElementData::create()
{
    return adoptRef(new UniqueElementData);
}

ShareableElementData::ShareableElementData(const Vector<Attribute>& attributes)
    : ElementData(attributes.size())
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        new (&m_attributeArray[i]) Attribute(attributes[i]);
}

ShareableElementData::~ShareableElementData()
{
    for (unsigned i = 0; i < m_arraySize; ++i)
        m_attributeArray[i].~Attribute();
}

ShareableElementData::ShareableElementData(const UniqueElementData& other)
    : ElementData(other, false)
{
    ASSERT(!other.m_presentationAttributeStyle);

    if (other.m_inlineStyle) {
        ASSERT(!other.m_inlineStyle->hasCSSOMWrapper());
        m_inlineStyle = other.m_inlineStyle->immutableCopyIfNeeded();
    }

    for (unsigned i = 0; i < m_arraySize; ++i)
        new (&m_attributeArray[i]) Attribute(other.m_attributeVector.at(i));
}

ElementData::ElementData(const ElementData& other, bool isUnique)
    : m_isUnique(isUnique)
    , m_arraySize(isUnique ? 0 : other.length())
    , m_presentationAttributeStyleIsDirty(other.m_presentationAttributeStyleIsDirty)
    , m_styleAttributeIsDirty(other.m_styleAttributeIsDirty)
    , m_animatedSVGAttributesAreDirty(other.m_animatedSVGAttributesAreDirty)
    , m_classNames(other.m_classNames)
    , m_idForStyleResolution(other.m_idForStyleResolution)
{
    // NOTE: The inline style is copied by the subclass copy constructor since we don't know what to do with it here.
}

UniqueElementData::UniqueElementData()
{
}

UniqueElementData::UniqueElementData(const UniqueElementData& other)
    : ElementData(other, true)
    , m_presentationAttributeStyle(other.m_presentationAttributeStyle)
    , m_attributeVector(other.m_attributeVector)
{
    m_inlineStyle = other.m_inlineStyle ? other.m_inlineStyle->mutableCopy() : 0;
}

UniqueElementData::UniqueElementData(const ShareableElementData& other)
    : ElementData(other, true)
{
    // An ShareableElementData should never have a mutable inline StylePropertySet attached.
    ASSERT(!other.m_inlineStyle || !other.m_inlineStyle->isMutable());
    m_inlineStyle = other.m_inlineStyle;

    m_attributeVector.reserveCapacity(other.length());
    for (unsigned i = 0; i < other.length(); ++i)
        m_attributeVector.uncheckedAppend(other.m_attributeArray[i]);
}

PassRefPtr<UniqueElementData> ElementData::makeUniqueCopy() const
{
    if (isUnique())
        return adoptRef(new UniqueElementData(static_cast<const UniqueElementData&>(*this)));
    return adoptRef(new UniqueElementData(static_cast<const ShareableElementData&>(*this)));
}

PassRefPtr<ShareableElementData> UniqueElementData::makeShareableCopy() const
{
    void* slot = WTF::fastMalloc(sizeForShareableElementDataWithAttributeCount(m_attributeVector.size()));
    return adoptRef(new (slot) ShareableElementData(*this));
}

void UniqueElementData::addAttribute(const QualifiedName& attributeName, const AtomicString& value)
{
    m_attributeVector.append(Attribute(attributeName, value));
}

void UniqueElementData::removeAttribute(size_t index)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < length());
    m_attributeVector.remove(index);
}

bool ElementData::isEquivalent(const ElementData* other) const
{
    if (!other)
        return isEmpty();

    unsigned len = length();
    if (len != other->length())
        return false;

    for (unsigned i = 0; i < len; i++) {
        const Attribute* attribute = attributeItem(i);
        const Attribute* otherAttr = other->getAttributeItem(attribute->name());
        if (!otherAttr || attribute->value() != otherAttr->value())
            return false;
    }

    return true;
}

size_t ElementData::getAttrIndex(Attr* attr) const
{
    // This relies on the fact that Attr's QualifiedName == the Attribute's name.
    for (unsigned i = 0; i < length(); ++i) {
        if (attributeItem(i)->name() == attr->qualifiedName())
            return i;
    }
    return notFound;
}

size_t ElementData::getAttributeItemIndexSlowCase(const AtomicString& name, bool shouldIgnoreAttributeCase) const
{
    // Continue to checking case-insensitively and/or full namespaced names if necessary:
    for (unsigned i = 0; i < length(); ++i) {
        const Attribute* attribute = attributeItem(i);
        if (!attribute->name().hasPrefix()) {
            if (shouldIgnoreAttributeCase && equalIgnoringCase(name, attribute->localName()))
                return i;
        } else {
            // FIXME: Would be faster to do this comparison without calling toString, which
            // generates a temporary string by concatenation. But this branch is only reached
            // if the attribute name has a prefix, which is rare in HTML.
            if (equalPossiblyIgnoringCase(name, attribute->name().toString(), shouldIgnoreAttributeCase))
                return i;
        }
    }
    return notFound;
}

Attribute* UniqueElementData::getAttributeItem(const QualifiedName& name)
{
    for (unsigned i = 0; i < length(); ++i) {
        if (m_attributeVector.at(i).name().matches(name))
            return &m_attributeVector.at(i);
    }
    return 0;
}

Attribute* UniqueElementData::attributeItem(unsigned index)
{
    ASSERT_WITH_SECURITY_IMPLICATION(index < length());
    return &m_attributeVector.at(index);
}

} // namespace WebCore
