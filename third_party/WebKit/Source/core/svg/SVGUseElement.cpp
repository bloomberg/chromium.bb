/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) Research In Motion Limited 2009-2010. All rights reserved.
 * Copyright (C) 2011 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2012 Renata Hodovan <reni@webkit.org>
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

#include "core/svg/SVGUseElement.h"

#include "XLinkNames.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/Document.h"
#include "core/dom/ElementTraversal.h"
#include "core/events/Event.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/fetch/FetchRequest.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/rendering/svg/RenderSVGResource.h"
#include "core/rendering/svg/RenderSVGTransformableContainer.h"
#include "core/svg/SVGElementInstance.h"
#include "core/svg/SVGGElement.h"
#include "core/svg/SVGLengthContext.h"
#include "core/svg/SVGSVGElement.h"
#include "core/xml/parser/XMLDocumentParser.h"

namespace WebCore {

inline SVGUseElement::SVGUseElement(Document& document)
    : SVGGraphicsElement(SVGNames::useTag, document)
    , SVGURIReference(this)
    , m_x(SVGAnimatedLength::create(this, SVGNames::xAttr, SVGLength::create(LengthModeWidth), AllowNegativeLengths))
    , m_y(SVGAnimatedLength::create(this, SVGNames::yAttr, SVGLength::create(LengthModeHeight), AllowNegativeLengths))
    , m_width(SVGAnimatedLength::create(this, SVGNames::widthAttr, SVGLength::create(LengthModeWidth), ForbidNegativeLengths))
    , m_height(SVGAnimatedLength::create(this, SVGNames::heightAttr, SVGLength::create(LengthModeHeight), ForbidNegativeLengths))
    , m_haveFiredLoadEvent(false)
    , m_needsShadowTreeRecreation(false)
    , m_svgLoadEventTimer(this, &SVGElement::svgLoadEventTimerFired)
{
    ASSERT(hasCustomStyleCallbacks());
    ScriptWrappable::init(this);

    addToPropertyMap(m_x);
    addToPropertyMap(m_y);
    addToPropertyMap(m_width);
    addToPropertyMap(m_height);
}

PassRefPtrWillBeRawPtr<SVGUseElement> SVGUseElement::create(Document& document)
{
    // Always build a user agent #shadow-root for SVGUseElement.
    RefPtrWillBeRawPtr<SVGUseElement> use = adoptRefWillBeRefCountedGarbageCollected(new SVGUseElement(document));
    use->ensureUserAgentShadowRoot();
    return use.release();
}

SVGUseElement::~SVGUseElement()
{
    setDocumentResource(0);
#if !ENABLE(OILPAN)
    clearResourceReferences();
#endif
}

SVGElementInstance* SVGUseElement::instanceRoot()
{
    // If there is no element instance tree, force immediate SVGElementInstance tree
    // creation by asking the document to invoke our recalcStyle function - as we can't
    // wait for the lazy creation to happen if e.g. JS wants to access the instanceRoot
    // object right after creating the element on-the-fly
    if (!m_targetElementInstance)
        document().updateRenderTreeIfNeeded();

    return m_targetElementInstance.get();
}

SVGElementInstance* SVGUseElement::animatedInstanceRoot() const
{
    // FIXME: Implement me.
    return 0;
}

bool SVGUseElement::isSupportedAttribute(const QualifiedName& attrName)
{
    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, supportedAttributes, ());
    if (supportedAttributes.isEmpty()) {
        SVGURIReference::addSupportedAttributes(supportedAttributes);
        supportedAttributes.add(SVGNames::xAttr);
        supportedAttributes.add(SVGNames::yAttr);
        supportedAttributes.add(SVGNames::widthAttr);
        supportedAttributes.add(SVGNames::heightAttr);
    }
    return supportedAttributes.contains<SVGAttributeHashTranslator>(attrName);
}

void SVGUseElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    SVGParsingError parseError = NoError;

    if (!isSupportedAttribute(name)) {
        SVGGraphicsElement::parseAttribute(name, value);
    } else if (name == SVGNames::xAttr) {
        m_x->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::yAttr) {
        m_y->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::widthAttr) {
        m_width->setBaseValueAsString(value, parseError);
    } else if (name == SVGNames::heightAttr) {
        m_height->setBaseValueAsString(value, parseError);
    } else if (SVGURIReference::parseAttribute(name, value, parseError)) {
    } else {
        ASSERT_NOT_REACHED();
    }

    reportAttributeParsingError(parseError, name, value);
}

#if !ASSERT_DISABLED
static inline bool isWellFormedDocument(Document* document)
{
    if (document->isXMLDocument())
        return static_cast<XMLDocumentParser*>(document->parser())->wellFormed();
    return true;
}
#endif

Node::InsertionNotificationRequest SVGUseElement::insertedInto(ContainerNode* rootParent)
{
    // This functions exists to assure assumptions made in the code regarding SVGElementInstance creation/destruction are satisfied.
    SVGGraphicsElement::insertedInto(rootParent);
    if (!rootParent->inDocument())
        return InsertionDone;
    ASSERT(!m_targetElementInstance || !isWellFormedDocument(&document()));
    ASSERT(!hasPendingResources() || !isWellFormedDocument(&document()));
    invalidateShadowTree();
    if (!isStructurallyExternal())
        sendSVGLoadEventIfPossibleAsynchronously();
    return InsertionDone;
}

void SVGUseElement::removedFrom(ContainerNode* rootParent)
{
    SVGGraphicsElement::removedFrom(rootParent);
    if (rootParent->inDocument())
        clearResourceReferences();
}

TreeScope* SVGUseElement::referencedScope() const
{
    if (!isExternalURIReference(hrefString(), document()))
        return &treeScope();
    return externalDocument();
}

Document* SVGUseElement::externalDocument() const
{
    if (m_resource && m_resource->isLoaded()) {
        // Gracefully handle error condition.
        if (m_resource->errorOccurred())
            return 0;
        ASSERT(m_resource->document());
        return m_resource->document();
    }
    return 0;
}

void transferUseWidthAndHeightIfNeeded(const SVGUseElement& use, SVGElement* shadowElement, const SVGElement& originalElement)
{
    ASSERT(shadowElement);
    if (isSVGSymbolElement(*shadowElement)) {
        // Spec (<use> on <symbol>): This generated 'svg' will always have explicit values for attributes width and height.
        // If attributes width and/or height are provided on the 'use' element, then these attributes
        // will be transferred to the generated 'svg'. If attributes width and/or height are not specified,
        // the generated 'svg' element will use values of 100% for these attributes.
        shadowElement->setAttribute(SVGNames::widthAttr, use.width()->isSpecified() ? AtomicString(use.width()->currentValue()->valueAsString()) : "100%");
        shadowElement->setAttribute(SVGNames::heightAttr, use.height()->isSpecified() ? AtomicString(use.height()->currentValue()->valueAsString()) : "100%");
    } else if (isSVGSVGElement(*shadowElement)) {
        // Spec (<use> on <svg>): If attributes width and/or height are provided on the 'use' element, then these
        // values will override the corresponding attributes on the 'svg' in the generated tree.
        if (use.width()->isSpecified())
            shadowElement->setAttribute(SVGNames::widthAttr, AtomicString(use.width()->currentValue()->valueAsString()));
        else
            shadowElement->setAttribute(SVGNames::widthAttr, originalElement.getAttribute(SVGNames::widthAttr));
        if (use.height()->isSpecified())
            shadowElement->setAttribute(SVGNames::heightAttr, AtomicString(use.height()->currentValue()->valueAsString()));
        else
            shadowElement->setAttribute(SVGNames::heightAttr, originalElement.getAttribute(SVGNames::heightAttr));
    }
}

void SVGUseElement::svgAttributeChanged(const QualifiedName& attrName)
{
    if (!isSupportedAttribute(attrName)) {
        SVGGraphicsElement::svgAttributeChanged(attrName);
        return;
    }

    SVGElement::InvalidationGuard invalidationGuard(this);

    RenderObject* renderer = this->renderer();
    if (attrName == SVGNames::xAttr
        || attrName == SVGNames::yAttr
        || attrName == SVGNames::widthAttr
        || attrName == SVGNames::heightAttr) {
        updateRelativeLengthsInformation();
        if (m_targetElementInstance) {
            ASSERT(m_targetElementInstance->correspondingElement());
            transferUseWidthAndHeightIfNeeded(*this, m_targetElementInstance->shadowTreeElement(), *m_targetElementInstance->correspondingElement());
        }
        if (renderer)
            RenderSVGResource::markForLayoutAndParentResourceInvalidation(renderer);
        return;
    }

    if (SVGURIReference::isKnownAttribute(attrName)) {
        bool isExternalReference = isExternalURIReference(hrefString(), document());
        if (isExternalReference) {
            KURL url = document().completeURL(hrefString());
            if (url.hasFragmentIdentifier()) {
                FetchRequest request(ResourceRequest(url.string()), localName());
                setDocumentResource(document().fetcher()->fetchSVGDocument(request));
            }
        } else {
            setDocumentResource(0);
        }

        invalidateShadowTree();

        return;
    }

    if (!renderer)
        return;

    ASSERT_NOT_REACHED();
}

static bool isDisallowedElement(Node* node)
{
    // Spec: "Any 'svg', 'symbol', 'g', graphics element or other 'use' is potentially a template object that can be re-used
    // (i.e., "instanced") in the SVG document via a 'use' element."
    // "Graphics Element" is defined as 'circle', 'ellipse', 'image', 'line', 'path', 'polygon', 'polyline', 'rect', 'text'
    // Excluded are anything that is used by reference or that only make sense to appear once in a document.
    // We must also allow the shadow roots of other use elements.
    if (node->isShadowRoot() || node->isTextNode())
        return false;

    if (!node->isSVGElement())
        return true;

    Element* element = toElement(node);

    DEFINE_STATIC_LOCAL(HashSet<QualifiedName>, allowedElementTags, ());
    if (allowedElementTags.isEmpty()) {
        allowedElementTags.add(SVGNames::aTag);
        allowedElementTags.add(SVGNames::circleTag);
        allowedElementTags.add(SVGNames::descTag);
        allowedElementTags.add(SVGNames::ellipseTag);
        allowedElementTags.add(SVGNames::gTag);
        allowedElementTags.add(SVGNames::imageTag);
        allowedElementTags.add(SVGNames::lineTag);
        allowedElementTags.add(SVGNames::metadataTag);
        allowedElementTags.add(SVGNames::pathTag);
        allowedElementTags.add(SVGNames::polygonTag);
        allowedElementTags.add(SVGNames::polylineTag);
        allowedElementTags.add(SVGNames::rectTag);
        allowedElementTags.add(SVGNames::svgTag);
        allowedElementTags.add(SVGNames::switchTag);
        allowedElementTags.add(SVGNames::symbolTag);
        allowedElementTags.add(SVGNames::textTag);
        allowedElementTags.add(SVGNames::textPathTag);
        allowedElementTags.add(SVGNames::titleTag);
        allowedElementTags.add(SVGNames::tspanTag);
        allowedElementTags.add(SVGNames::useTag);
    }
    return !allowedElementTags.contains<SVGAttributeHashTranslator>(element->tagQName());
}

static bool subtreeContainsDisallowedElement(Node* start)
{
    if (isDisallowedElement(start))
        return true;

    for (Node* cur = start->firstChild(); cur; cur = cur->nextSibling()) {
        if (subtreeContainsDisallowedElement(cur))
            return true;
    }

    return false;
}

void SVGUseElement::scheduleShadowTreeRecreation()
{
    if (!referencedScope() || inUseShadowTree())
        return;
    m_needsShadowTreeRecreation = true;
    document().scheduleUseShadowTreeUpdate(*this);
}

void SVGUseElement::clearResourceReferences()
{
    if (m_targetElementInstance) {
        m_targetElementInstance->detach();
        m_targetElementInstance = nullptr;
    }

    // FIXME: We should try to optimize this, to at least allow partial reclones.
    if (ShadowRoot* shadowTreeRootElement = userAgentShadowRoot())
        shadowTreeRootElement->removeChildren();

    m_needsShadowTreeRecreation = false;
    document().unscheduleUseShadowTreeUpdate(*this);

    document().accessSVGExtensions().removeAllTargetReferencesForElement(this);
}

void SVGUseElement::buildPendingResource()
{
    if (!referencedScope() || inUseShadowTree())
        return;
    clearResourceReferences();
    if (!inDocument())
        return;

    AtomicString id;
    Element* target = SVGURIReference::targetElementFromIRIString(hrefString(), treeScope(), &id, externalDocument());
    if (!target || !target->inDocument()) {
        // If we can't find the target of an external element, just give up.
        // We can't observe if the target somewhen enters the external document, nor should we do it.
        if (externalDocument())
            return;
        if (id.isEmpty())
            return;

        referencedScope()->document().accessSVGExtensions().addPendingResource(id, this);
        ASSERT(hasPendingResources());
        return;
    }

    if (target->isSVGElement()) {
        buildShadowAndInstanceTree(toSVGElement(target));
        invalidateDependentShadowTrees();
    }

    ASSERT(!m_needsShadowTreeRecreation);
}

void SVGUseElement::buildShadowAndInstanceTree(SVGElement* target)
{
    ASSERT(!m_targetElementInstance);

    // <use> creates a "user agent" shadow root. Do not build the shadow/instance tree for <use>
    // elements living in a user agent shadow tree because they will get expanded in a second
    // pass -- see expandUseElementsInShadowTree().
    if (inUseShadowTree())
        return;

    // Do not allow self-referencing.
    // 'target' may be null, if it's a non SVG namespaced element.
    if (!target || target == this)
        return;

    // Why a seperated instance/shadow tree? SVG demands it:
    // The instance tree is accesable from JavaScript, and has to
    // expose a 1:1 copy of the referenced tree, whereas internally we need
    // to alter the tree for correct "use-on-symbol", "use-on-svg" support.

    // Build instance tree. Create root SVGElementInstance object for the first sub-tree node.
    //
    // Spec: If the 'use' element references a simple graphics element such as a 'rect', then there is only a
    // single SVGElementInstance object, and the correspondingElement attribute on this SVGElementInstance object
    // is the SVGRectElement that corresponds to the referenced 'rect' element.
    m_targetElementInstance = SVGElementInstance::create(this, this, target);

    // Eventually enter recursion to build SVGElementInstance objects for the sub-tree children
    bool foundProblem = false;
    buildInstanceTree(target, m_targetElementInstance.get(), foundProblem, false);

    if (instanceTreeIsLoading(m_targetElementInstance.get()))
        return;

    // SVG specification does not say a word about <use> & cycles. My view on this is: just ignore it!
    // Non-appearing <use> content is easier to debug, then half-appearing content.
    if (foundProblem) {
        clearResourceReferences();
        return;
    }

    // Assure instance tree building was successfull
    ASSERT(m_targetElementInstance);
    ASSERT(!m_targetElementInstance->shadowTreeElement());
    ASSERT(m_targetElementInstance->correspondingUseElement() == this);
    ASSERT(m_targetElementInstance->directUseElement() == this);
    ASSERT(m_targetElementInstance->correspondingElement() == target);

    ShadowRoot* shadowTreeRootElement = userAgentShadowRoot();
    ASSERT(shadowTreeRootElement);

    // Build shadow tree from instance tree
    // This also handles the special cases: <use> on <symbol>, <use> on <svg>.
    buildShadowTree(target, m_targetElementInstance.get(), shadowTreeRootElement);

    // Expand all <use> elements in the shadow tree.
    // Expand means: replace the actual <use> element by what it references.
    expandUseElementsInShadowTree(shadowTreeRootElement);

    // Expand all <symbol> elements in the shadow tree.
    // Expand means: replace the actual <symbol> element by the <svg> element.
    expandSymbolElementsInShadowTree(shadowTreeRootElement);

    // If no shadow tree element is present, this means that the reference root
    // element was removed, as it is disallowed (ie. <use> on <foreignObject>)
    // Do NOT leave an inconsistent instance tree around, instead destruct it.
    Node* shadowTreeTargetNode = shadowTreeRootElement->firstChild();
    if (!shadowTreeTargetNode) {
        clearResourceReferences();
        return;
    }

    // Now that the shadow tree is completly expanded, we can associate
    // shadow tree elements <-> instances in the instance tree.
    associateInstancesWithShadowTreeElements(shadowTreeTargetNode, m_targetElementInstance.get());

    SVGElement* shadowTreeTargetElement = toSVGElement(shadowTreeTargetNode);
    ASSERT(shadowTreeTargetElement->correspondingElement());
    transferUseWidthAndHeightIfNeeded(*this, shadowTreeTargetElement, *shadowTreeTargetElement->correspondingElement());

    ASSERT(shadowTreeTargetElement->parentNode() == shadowTreeRootElement);

    // Transfer event listeners assigned to the referenced element to our shadow tree elements.
    transferEventListenersToShadowTree(shadowTreeTargetElement);

    // Update relative length information.
    updateRelativeLengthsInformation();
}

RenderObject* SVGUseElement::createRenderer(RenderStyle*)
{
    return new RenderSVGTransformableContainer(this);
}

static bool isDirectReference(const Node& node)
{
    return isSVGPathElement(node)
        || isSVGRectElement(node)
        || isSVGCircleElement(node)
        || isSVGEllipseElement(node)
        || isSVGPolygonElement(node)
        || isSVGPolylineElement(node)
        || isSVGTextElement(node);
}

void SVGUseElement::toClipPath(Path& path)
{
    ASSERT(path.isEmpty());

    Node* n = userAgentShadowRoot()->firstChild();
    if (!n)
        return;

    if (n->isSVGElement() && toSVGElement(n)->isSVGGraphicsElement()) {
        if (!isDirectReference(*n)) {
            // Spec: Indirect references are an error (14.3.5)
            document().accessSVGExtensions().reportError("Not allowed to use indirect reference in <clip-path>");
        } else {
            toSVGGraphicsElement(n)->toClipPath(path);
            // FIXME: Avoid manual resolution of x/y here. Its potentially harmful.
            SVGLengthContext lengthContext(this);
            path.translate(FloatSize(m_x->currentValue()->value(lengthContext), m_y->currentValue()->value(lengthContext)));
            path.transform(animatedLocalTransform());
        }
    }
}

RenderObject* SVGUseElement::rendererClipChild() const
{
    if (Node* n = userAgentShadowRoot()->firstChild()) {
        if (n->isSVGElement() && isDirectReference(*n))
            return toSVGElement(n)->renderer();
    }

    return 0;
}

void SVGUseElement::buildInstanceTree(SVGElement* target, SVGElementInstance* targetInstance, bool& foundProblem, bool foundUse)
{
    ASSERT(target);
    ASSERT(targetInstance);

    // Spec: If the referenced object is itself a 'use', or if there are 'use' subelements within the referenced
    // object, the instance tree will contain recursive expansion of the indirect references to form a complete tree.
    bool targetIsUseElement = isSVGUseElement(*target);
    SVGElement* newTarget = 0;
    if (targetIsUseElement) {
        foundProblem = hasCycleUseReferencing(toSVGUseElement(target), targetInstance, newTarget);
        if (foundProblem)
            return;

        // We only need to track first degree <use> dependencies. Indirect references are handled
        // as the invalidation bubbles up the dependency chain.
        if (!foundUse) {
            document().accessSVGExtensions().addElementReferencingTarget(this, target);
            foundUse = true;
        }
    } else if (isDisallowedElement(target)) {
        foundProblem = true;
        return;
    }

    // A general description from the SVG spec, describing what buildInstanceTree() actually does.
    //
    // Spec: If the 'use' element references a 'g' which contains two 'rect' elements, then the instance tree
    // contains three SVGElementInstance objects, a root SVGElementInstance object whose correspondingElement
    // is the SVGGElement object for the 'g', and then two child SVGElementInstance objects, each of which has
    // its correspondingElement that is an SVGRectElement object.

    for (SVGElement* element = Traversal<SVGElement>::firstChild(*target); element; element = Traversal<SVGElement>::nextSibling(*element)) {
        // Skip any disallowed element.
        if (isDisallowedElement(element))
            continue;

        // Create SVGElementInstance object, for both container/non-container nodes.
        RefPtrWillBeRawPtr<SVGElementInstance> instance = SVGElementInstance::create(this, 0, element);
        SVGElementInstance* instancePtr = instance.get();
        targetInstance->appendChild(instance.release());

        // Enter recursion, appending new instance tree nodes to the "instance" object.
        buildInstanceTree(element, instancePtr, foundProblem, foundUse);
        if (foundProblem)
            return;
    }

    if (!targetIsUseElement || !newTarget)
        return;

    RefPtrWillBeRawPtr<SVGElementInstance> newInstance = SVGElementInstance::create(this, toSVGUseElement(target), newTarget);
    SVGElementInstance* newInstancePtr = newInstance.get();
    targetInstance->appendChild(newInstance.release());
    buildInstanceTree(newTarget, newInstancePtr, foundProblem, foundUse);
}

bool SVGUseElement::hasCycleUseReferencing(SVGUseElement* use, SVGElementInstance* targetInstance, SVGElement*& newTarget)
{
    ASSERT(referencedScope());
    Element* targetElement = SVGURIReference::targetElementFromIRIString(use->hrefString(), *referencedScope());
    newTarget = 0;
    if (targetElement && targetElement->isSVGElement())
        newTarget = toSVGElement(targetElement);

    if (!newTarget)
        return false;

    // Shortcut for self-references
    if (newTarget == this)
        return true;

    AtomicString targetId = newTarget->getIdAttribute();
    SVGElementInstance* instance = targetInstance->parentNode();
    while (instance) {
        SVGElement* element = instance->correspondingElement();

        if (element->hasID() && element->getIdAttribute() == targetId && element->document() == newTarget->document())
            return true;

        instance = instance->parentNode();
    }
    return false;
}

static inline void removeDisallowedElementsFromSubtree(Element& subtree)
{
    ASSERT(!subtree.inDocument());
    Element* element = ElementTraversal::firstWithin(subtree);
    while (element) {
        if (isDisallowedElement(element)) {
            Element* next = ElementTraversal::nextSkippingChildren(*element, &subtree);
            // The subtree is not in document so this won't generate events that could mutate the tree.
            element->parentNode()->removeChild(element);
            element = next;
        } else {
            element = ElementTraversal::next(*element, &subtree);
        }
    }
}

void SVGUseElement::buildShadowTree(SVGElement* target, SVGElementInstance* targetInstance, ShadowRoot* shadowTreeRootElement)
{
    // For instance <use> on <foreignObject> (direct case).
    if (isDisallowedElement(target))
        return;

    RefPtrWillBeRawPtr<Element> newChild = targetInstance->correspondingElement()->cloneElementWithChildren();

    // We don't walk the target tree element-by-element, and clone each element,
    // but instead use cloneElementWithChildren(). This is an optimization for the common
    // case where <use> doesn't contain disallowed elements (ie. <foreignObject>).
    // Though if there are disallowed elements in the subtree, we have to remove them.
    // For instance: <use> on <g> containing <foreignObject> (indirect case).
    if (subtreeContainsDisallowedElement(newChild.get()))
        removeDisallowedElementsFromSubtree(*newChild);

    shadowTreeRootElement->appendChild(newChild.release());
}

void SVGUseElement::expandUseElementsInShadowTree(Node* element)
{
    ASSERT(element);
    // Why expand the <use> elements in the shadow tree here, and not just
    // do this directly in buildShadowTree, if we encounter a <use> element?
    //
    // Short answer: Because we may miss to expand some elements. For example, if a <symbol>
    // contains <use> tags, we'd miss them. So once we're done with setting up the
    // actual shadow tree (after the special case modification for svg/symbol) we have
    // to walk it completely and expand all <use> elements.
    if (isSVGUseElement(*element)) {
        SVGUseElement* use = toSVGUseElement(element);
        ASSERT(!use->resourceIsStillLoading());

        ASSERT(referencedScope());
        Element* targetElement = SVGURIReference::targetElementFromIRIString(use->hrefString(), *referencedScope());
        SVGElement* target = 0;
        if (targetElement && targetElement->isSVGElement())
            target = toSVGElement(targetElement);

        // Don't ASSERT(target) here, it may be "pending", too.
        // Setup sub-shadow tree root node
        RefPtrWillBeRawPtr<SVGGElement> cloneParent = SVGGElement::create(referencedScope()->document());
        use->cloneChildNodes(cloneParent.get());

        // Spec: In the generated content, the 'use' will be replaced by 'g', where all attributes from the
        // 'use' element except for x, y, width, height and xlink:href are transferred to the generated 'g' element.
        transferUseAttributesToReplacedElement(use, cloneParent.get());

        if (target && !isDisallowedElement(target)) {
            RefPtrWillBeRawPtr<Element> newChild = target->cloneElementWithChildren();
            ASSERT(newChild->isSVGElement());
            transferUseWidthAndHeightIfNeeded(*use, toSVGElement(newChild.get()), *target);
            cloneParent->appendChild(newChild.release());
        }

        // We don't walk the target tree element-by-element, and clone each element,
        // but instead use cloneElementWithChildren(). This is an optimization for the common
        // case where <use> doesn't contain disallowed elements (ie. <foreignObject>).
        // Though if there are disallowed elements in the subtree, we have to remove them.
        // For instance: <use> on <g> containing <foreignObject> (indirect case).
        if (subtreeContainsDisallowedElement(cloneParent.get()))
            removeDisallowedElementsFromSubtree(*cloneParent);

        RefPtr<Node> replacingElement(cloneParent.get());

        // Replace <use> with referenced content.
        ASSERT(use->parentNode());
        use->parentNode()->replaceChild(cloneParent.release(), use);

        // Expand the siblings because the *element* is replaced and we will
        // lose the sibling chain when we are back from recursion.
        element = replacingElement.get();
        for (RefPtr<Node> sibling = element->nextSibling(); sibling; sibling = sibling->nextSibling())
            expandUseElementsInShadowTree(sibling.get());
    }

    for (RefPtr<Node> child = element->firstChild(); child; child = child->nextSibling())
        expandUseElementsInShadowTree(child.get());
}

void SVGUseElement::expandSymbolElementsInShadowTree(Node* element)
{
    ASSERT(element);
    if (isSVGSymbolElement(*element)) {
        // Spec: The referenced 'symbol' and its contents are deep-cloned into the generated tree,
        // with the exception that the 'symbol' is replaced by an 'svg'. This generated 'svg' will
        // always have explicit values for attributes width and height. If attributes width and/or
        // height are provided on the 'use' element, then these attributes will be transferred to
        // the generated 'svg'. If attributes width and/or height are not specified, the generated
        // 'svg' element will use values of 100% for these attributes.
        ASSERT(referencedScope());
        RefPtrWillBeRawPtr<SVGSVGElement> svgElement = SVGSVGElement::create(referencedScope()->document());

        // Transfer all data (attributes, etc.) from <symbol> to the new <svg> element.
        svgElement->cloneDataFromElement(*toElement(element));

        // Only clone symbol children, and add them to the new <svg> element
        for (Node* child = element->firstChild(); child; child = child->nextSibling()) {
            RefPtr<Node> newChild = child->cloneNode(true);
            svgElement->appendChild(newChild.release());
        }

        // We don't walk the target tree element-by-element, and clone each element,
        // but instead use cloneNode(deep=true). This is an optimization for the common
        // case where <use> doesn't contain disallowed elements (ie. <foreignObject>).
        // Though if there are disallowed elements in the subtree, we have to remove them.
        // For instance: <use> on <g> containing <foreignObject> (indirect case).
        if (subtreeContainsDisallowedElement(svgElement.get()))
            removeDisallowedElementsFromSubtree(*svgElement);

        RefPtr<Node> replacingElement(svgElement.get());

        // Replace <symbol> with <svg>.
        element->parentNode()->replaceChild(svgElement.release(), element);

        // Expand the siblings because the *element* is replaced and we will
        // lose the sibling chain when we are back from recursion.
        element = replacingElement.get();
        for (RefPtr<Node> sibling = element->nextSibling(); sibling; sibling = sibling->nextSibling())
            expandSymbolElementsInShadowTree(sibling.get());
    }

    for (RefPtr<Node> child = element->firstChild(); child; child = child->nextSibling())
        expandSymbolElementsInShadowTree(child.get());
}

void SVGUseElement::transferEventListenersToShadowTree(SVGElement* shadowTreeTargetElement)
{
    if (!shadowTreeTargetElement)
        return;

    SVGElement* originalElement = shadowTreeTargetElement->correspondingElement();
    ASSERT(originalElement);
    if (EventTargetData* data = originalElement->eventTargetData())
        data->eventListenerMap.copyEventListenersNotCreatedFromMarkupToTarget(shadowTreeTargetElement);

    for (SVGElement* child = Traversal<SVGElement>::firstChild(*shadowTreeTargetElement); child; child = Traversal<SVGElement>::nextSibling(*child))
        transferEventListenersToShadowTree(child);
}

void SVGUseElement::associateInstancesWithShadowTreeElements(Node* target, SVGElementInstance* targetInstance)
{
    if (!target || !targetInstance)
        return;

    SVGElement* originalElement = targetInstance->correspondingElement();
    ASSERT(originalElement);
    if (isSVGUseElement(*originalElement)) {
        // <use> gets replaced by <g>
        ASSERT(AtomicString(target->nodeName()) == SVGNames::gTag);
    } else if (isSVGSymbolElement(*originalElement)) {
        // <symbol> gets replaced by <svg>
        ASSERT(AtomicString(target->nodeName()) == SVGNames::svgTag);
    } else {
        ASSERT(AtomicString(target->nodeName()) == originalElement->nodeName());
    }

    SVGElement* element = 0;
    if (target->isSVGElement())
        element = toSVGElement(target);

    ASSERT(!targetInstance->shadowTreeElement());
    targetInstance->setShadowTreeElement(element);
    element->setCorrespondingElement(originalElement);

    SVGElement* child = Traversal<SVGElement>::firstChild(*target);
    for (SVGElementInstance* instance = targetInstance->firstChild(); child && instance; instance = instance->nextSibling()) {
        associateInstancesWithShadowTreeElements(child, instance);
        child = Traversal<SVGElement>::nextSibling(*child);
    }
}

SVGElementInstance* SVGUseElement::instanceForShadowTreeElement(Node* element) const
{
    if (!m_targetElementInstance) {
        ASSERT(!inDocument());
        return 0;
    }

    return instanceForShadowTreeElement(element, m_targetElementInstance.get());
}

SVGElementInstance* SVGUseElement::instanceForShadowTreeElement(Node* element, SVGElementInstance* instance) const
{
    ASSERT(element);
    ASSERT(instance);

    // We're dispatching a mutation event during shadow tree construction
    // this instance hasn't yet been associated to a shadowTree element.
    if (!instance->shadowTreeElement())
        return 0;

    if (element == instance->shadowTreeElement())
        return instance;

    for (SVGElementInstance* current = instance->firstChild(); current; current = current->nextSibling()) {
        if (SVGElementInstance* search = instanceForShadowTreeElement(element, current))
            return search;
    }

    return 0;
}

void SVGUseElement::invalidateShadowTree()
{
    if (!inActiveDocument() || m_needsShadowTreeRecreation)
        return;
    scheduleShadowTreeRecreation();
    invalidateDependentShadowTrees();
}

void SVGUseElement::invalidateDependentShadowTrees()
{
    // Recursively invalidate dependent <use> shadow trees
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >& instances = instancesForElement();
    const WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator end = instances.end();
    for (WillBeHeapHashSet<RawPtrWillBeWeakMember<SVGElement> >::const_iterator it = instances.begin(); it != end; ++it) {
        if (SVGUseElement* element = (*it)->correspondingUseElement()) {
            ASSERT(element->inDocument());
            element->invalidateShadowTree();
        }
    }
}

void SVGUseElement::transferUseAttributesToReplacedElement(SVGElement* from, SVGElement* to) const
{
    ASSERT(from);
    ASSERT(to);

    to->cloneDataFromElement(*from);

    to->removeAttribute(SVGNames::xAttr);
    to->removeAttribute(SVGNames::yAttr);
    to->removeAttribute(SVGNames::widthAttr);
    to->removeAttribute(SVGNames::heightAttr);
    to->removeAttribute(XLinkNames::hrefAttr);
}

bool SVGUseElement::selfHasRelativeLengths() const
{
    if (m_x->currentValue()->isRelative()
        || m_y->currentValue()->isRelative()
        || m_width->currentValue()->isRelative()
        || m_height->currentValue()->isRelative())
        return true;

    if (!m_targetElementInstance)
        return false;

    SVGElement* element = m_targetElementInstance->correspondingElement();
    if (!element)
        return false;

    return element->hasRelativeLengths();
}

void SVGUseElement::notifyFinished(Resource* resource)
{
    if (!inDocument())
        return;

    invalidateShadowTree();
    if (resource->errorOccurred())
        dispatchEvent(Event::create(EventTypeNames::error));
    else if (!resource->wasCanceled()) {
        if (m_haveFiredLoadEvent)
            return;
        if (!isStructurallyExternal())
            return;
        ASSERT(!m_haveFiredLoadEvent);
        m_haveFiredLoadEvent = true;
        sendSVGLoadEventIfPossibleAsynchronously();
    }
}

bool SVGUseElement::resourceIsStillLoading()
{
    if (m_resource && m_resource->isLoading())
        return true;
    return false;
}

bool SVGUseElement::instanceTreeIsLoading(SVGElementInstance* targetElementInstance)
{
    for (SVGElementInstance* instance = targetElementInstance->firstChild(); instance; instance = instance->nextSibling()) {
        if (SVGUseElement* use = instance->correspondingUseElement()) {
            if (use->resourceIsStillLoading())
                return true;
        }
        if (instance->hasChildren())
            instanceTreeIsLoading(instance);
    }
    return false;
}

void SVGUseElement::setDocumentResource(ResourcePtr<DocumentResource> resource)
{
    if (m_resource == resource)
        return;

    if (m_resource)
        m_resource->removeClient(this);

    m_resource = resource;
    if (m_resource)
        m_resource->addClient(this);
}

void SVGUseElement::trace(Visitor* visitor)
{
    visitor->trace(m_targetElementInstance);
    SVGGraphicsElement::trace(visitor);
}

}
