/*
 * Copyright (C) 2006, 2008, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "core/rendering/HitTestResult.h"

#include "HTMLNames.h"
#include "SVGNames.h"
#include "XLinkNames.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/dom/NodeRenderingTraversal.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/fetch/ImageResource.h"
#include "core/html/HTMLAnchorElement.h"
#include "core/html/HTMLAreaElement.h"
#include "core/html/HTMLImageElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/HTMLTextAreaElement.h"
#include "core/html/HTMLVideoElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/platform/Scrollbar.h"
#include "core/rendering/HitTestLocation.h"
#include "core/rendering/RenderImage.h"

namespace WebCore {

using namespace HTMLNames;

HitTestResult::HitTestResult()
    : m_isOverWidget(false)
{
}

HitTestResult::HitTestResult(const LayoutPoint& point)
    : m_hitTestLocation(point)
    , m_pointInInnerNodeFrame(point)
    , m_isOverWidget(false)
{
}

HitTestResult::HitTestResult(const LayoutPoint& centerPoint, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding)
    : m_hitTestLocation(centerPoint, topPadding, rightPadding, bottomPadding, leftPadding)
    , m_pointInInnerNodeFrame(centerPoint)
    , m_isOverWidget(false)
{
}

HitTestResult::HitTestResult(const HitTestLocation& other)
    : m_hitTestLocation(other)
    , m_pointInInnerNodeFrame(m_hitTestLocation.point())
    , m_isOverWidget(false)
{
}

HitTestResult::HitTestResult(const HitTestResult& other)
    : m_hitTestLocation(other.m_hitTestLocation)
    , m_innerNode(other.innerNode())
    , m_innerNonSharedNode(other.innerNonSharedNode())
    , m_pointInInnerNodeFrame(other.m_pointInInnerNodeFrame)
    , m_localPoint(other.localPoint())
    , m_innerURLElement(other.URLElement())
    , m_scrollbar(other.scrollbar())
    , m_isOverWidget(other.isOverWidget())
{
    // Only copy the NodeSet in case of rect hit test.
    m_rectBasedTestResult = adoptPtr(other.m_rectBasedTestResult ? new NodeSet(*other.m_rectBasedTestResult) : 0);
}

HitTestResult::~HitTestResult()
{
}

HitTestResult& HitTestResult::operator=(const HitTestResult& other)
{
    m_hitTestLocation = other.m_hitTestLocation;
    m_innerNode = other.innerNode();
    m_innerNonSharedNode = other.innerNonSharedNode();
    m_pointInInnerNodeFrame = other.m_pointInInnerNodeFrame;
    m_localPoint = other.localPoint();
    m_innerURLElement = other.URLElement();
    m_scrollbar = other.scrollbar();
    m_isOverWidget = other.isOverWidget();

    // Only copy the NodeSet in case of rect hit test.
    m_rectBasedTestResult = adoptPtr(other.m_rectBasedTestResult ? new NodeSet(*other.m_rectBasedTestResult) : 0);

    return *this;
}

void HitTestResult::setToNodesInDocumentTreeScope()
{
    if (Node* node = innerNode()) {
        node = node->document().ancestorInThisScope(node);
        setInnerNode(node);
    }

    if (Node* node = innerNonSharedNode()) {
        node = node->document().ancestorInThisScope(node);
        setInnerNonSharedNode(node);
    }
}

void HitTestResult::setToShadowHostIfInUserAgentShadowRoot()
{
    if (Node* node = innerNode()) {
        if (ShadowRoot* containingShadowRoot = node->containingShadowRoot()) {
            if (containingShadowRoot->type() == ShadowRoot::UserAgentShadowRoot)
                setInnerNode(node->shadowHost());
        }
    }

    if (Node* node = innerNonSharedNode()) {
        if (ShadowRoot* containingShadowRoot = node->containingShadowRoot()) {
            if (containingShadowRoot->type() == ShadowRoot::UserAgentShadowRoot)
                setInnerNonSharedNode(node->shadowHost());
        }
    }
}

void HitTestResult::setInnerNode(Node* n)
{
    if (n && n->isPseudoElement())
        n = n->parentOrShadowHostNode();
    m_innerNode = n;
}

void HitTestResult::setInnerNonSharedNode(Node* n)
{
    if (n && n->isPseudoElement())
        n = n->parentOrShadowHostNode();
    m_innerNonSharedNode = n;
}

void HitTestResult::setURLElement(Element* n)
{
    m_innerURLElement = n;
}

void HitTestResult::setScrollbar(Scrollbar* s)
{
    m_scrollbar = s;
}

Frame* HitTestResult::innerNodeFrame() const
{
    if (m_innerNonSharedNode)
        return m_innerNonSharedNode->document().frame();
    if (m_innerNode)
        return m_innerNode->document().frame();
    return 0;
}

Frame* HitTestResult::targetFrame() const
{
    if (!m_innerURLElement)
        return 0;

    Frame* frame = m_innerURLElement->document().frame();
    if (!frame)
        return 0;

    return frame->tree()->find(m_innerURLElement->target());
}

bool HitTestResult::isSelected() const
{
    if (!m_innerNonSharedNode)
        return false;

    if (Frame* frame = m_innerNonSharedNode->document().frame())
        return frame->selection().contains(m_hitTestLocation.point());
    return false;
}

String HitTestResult::spellingToolTip(TextDirection& dir) const
{
    dir = LTR;
    // Return the tool tip string associated with this point, if any. Only markers associated with bad grammar
    // currently supply strings, but maybe someday markers associated with misspelled words will also.
    if (!m_innerNonSharedNode)
        return String();

    DocumentMarker* marker = m_innerNonSharedNode->document().markers()->markerContainingPoint(m_hitTestLocation.point(), DocumentMarker::Grammar);
    if (!marker)
        return String();

    if (RenderObject* renderer = m_innerNonSharedNode->renderer())
        dir = renderer->style()->direction();
    return marker->description();
}

String HitTestResult::title(TextDirection& dir) const
{
    dir = LTR;
    // Find the title in the nearest enclosing DOM node.
    // For <area> tags in image maps, walk the tree for the <area>, not the <img> using it.
    for (Node* titleNode = m_innerNode.get(); titleNode; titleNode = titleNode->parentNode()) {
        if (titleNode->isElementNode()) {
            String title = toElement(titleNode)->title();
            if (!title.isEmpty()) {
                if (RenderObject* renderer = titleNode->renderer())
                    dir = renderer->style()->direction();
                return title;
            }
        }
    }
    return String();
}

String displayString(const String& string, const Node* node)
{
    if (!node)
        return string;
    return node->document().displayStringModifiedByEncoding(string);
}

String HitTestResult::altDisplayString() const
{
    if (!m_innerNonSharedNode)
        return String();

    if (m_innerNonSharedNode->hasTagName(imgTag)) {
        HTMLImageElement* image = toHTMLImageElement(m_innerNonSharedNode.get());
        return displayString(image->getAttribute(altAttr), m_innerNonSharedNode.get());
    }

    if (m_innerNonSharedNode->hasTagName(inputTag)) {
        HTMLInputElement* input = toHTMLInputElement(m_innerNonSharedNode.get());
        return displayString(input->alt(), m_innerNonSharedNode.get());
    }

    return String();
}

Image* HitTestResult::image() const
{
    if (!m_innerNonSharedNode)
        return 0;

    RenderObject* renderer = m_innerNonSharedNode->renderer();
    if (renderer && renderer->isImage()) {
        RenderImage* image = toRenderImage(renderer);
        if (image->cachedImage() && !image->cachedImage()->errorOccurred())
            return image->cachedImage()->imageForRenderer(image);
    }

    return 0;
}

IntRect HitTestResult::imageRect() const
{
    if (!image())
        return IntRect();
    return m_innerNonSharedNode->renderBox()->absoluteContentQuad().enclosingBoundingBox();
}

KURL HitTestResult::absoluteImageURL() const
{
    if (!m_innerNonSharedNode)
        return KURL();

    if (!(m_innerNonSharedNode->renderer() && m_innerNonSharedNode->renderer()->isImage()))
        return KURL();

    AtomicString urlString;
    if (m_innerNonSharedNode->hasTagName(embedTag)
        || m_innerNonSharedNode->hasTagName(imgTag)
        || m_innerNonSharedNode->hasTagName(inputTag)
        || m_innerNonSharedNode->hasTagName(objectTag)
        || m_innerNonSharedNode->hasTagName(SVGNames::imageTag)
       ) {
        Element* element = toElement(m_innerNonSharedNode.get());
        urlString = element->imageSourceURL();
    } else
        return KURL();

    return m_innerNonSharedNode->document().completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
}

KURL HitTestResult::absoluteMediaURL() const
{
    if (HTMLMediaElement* mediaElt = mediaElement())
        return mediaElt->currentSrc();
    return KURL();
}

HTMLMediaElement* HitTestResult::mediaElement() const
{
    if (!m_innerNonSharedNode)
        return 0;

    if (!(m_innerNonSharedNode->renderer() && m_innerNonSharedNode->renderer()->isMedia()))
        return 0;

    if (isHTMLVideoElement(m_innerNonSharedNode.get()) || m_innerNonSharedNode->hasTagName(HTMLNames::audioTag))
        return toHTMLMediaElement(m_innerNonSharedNode.get());
    return 0;
}

KURL HitTestResult::absoluteLinkURL() const
{
    if (!m_innerURLElement)
        return KURL();

    AtomicString urlString;
    if (isHTMLAnchorElement(m_innerURLElement.get()) || isHTMLAreaElement(m_innerURLElement.get()) || m_innerURLElement->hasTagName(linkTag))
        urlString = m_innerURLElement->getAttribute(hrefAttr);
    else if (m_innerURLElement->hasTagName(SVGNames::aTag))
        urlString = m_innerURLElement->getAttribute(XLinkNames::hrefAttr);
    else
        return KURL();

    return m_innerURLElement->document().completeURL(stripLeadingAndTrailingHTMLSpaces(urlString));
}

bool HitTestResult::isLiveLink() const
{
    if (!m_innerURLElement)
        return false;

    if (isHTMLAnchorElement(m_innerURLElement.get()))
        return toHTMLAnchorElement(m_innerURLElement.get())->isLiveLink();

    if (m_innerURLElement->hasTagName(SVGNames::aTag))
        return m_innerURLElement->isLink();

    return false;
}

bool HitTestResult::isMisspelled() const
{
    if (!targetNode())
        return false;
    VisiblePosition pos(targetNode()->renderer()->positionForPoint(localPoint()));
    if (pos.isNull())
        return false;
    return m_innerNonSharedNode->document().markers()->markersInRange(
        makeRange(pos, pos).get(), DocumentMarker::MisspellingMarkers()).size() > 0;
}

String HitTestResult::titleDisplayString() const
{
    if (!m_innerURLElement)
        return String();

    return displayString(m_innerURLElement->title(), m_innerURLElement.get());
}

String HitTestResult::textContent() const
{
    if (!m_innerURLElement)
        return String();
    return m_innerURLElement->textContent();
}

// FIXME: This function needs a better name and may belong in a different class. It's not
// really isContentEditable(); it's more like needsEditingContextMenu(). In many ways, this
// function would make more sense in the ContextMenu class, except that WebElementDictionary
// hooks into it. Anyway, we should architect this better.
bool HitTestResult::isContentEditable() const
{
    if (!m_innerNonSharedNode)
        return false;

    if (isHTMLTextAreaElement(m_innerNonSharedNode.get()))
        return true;

    if (m_innerNonSharedNode->hasTagName(inputTag))
        return toHTMLInputElement(m_innerNonSharedNode.get())->isTextField();

    return m_innerNonSharedNode->rendererIsEditable();
}

bool HitTestResult::addNodeToRectBasedTestResult(Node* node, const HitTestRequest& request, const HitTestLocation& locationInContainer, const LayoutRect& rect)
{
    // If it is not a rect-based hit test, this method has to be no-op.
    // Return false, so the hit test stops.
    if (!isRectBasedTest())
        return false;

    // If node is null, return true so the hit test can continue.
    if (!node)
        return true;

    if (request.disallowsShadowContent())
        node = node->document().ancestorInThisScope(node);

    mutableRectBasedTestResult().add(node);

    bool regionFilled = rect.contains(locationInContainer.boundingBox());
    return !regionFilled;
}

bool HitTestResult::addNodeToRectBasedTestResult(Node* node, const HitTestRequest& request, const HitTestLocation& locationInContainer, const FloatRect& rect)
{
    // If it is not a rect-based hit test, this method has to be no-op.
    // Return false, so the hit test stops.
    if (!isRectBasedTest())
        return false;

    // If node is null, return true so the hit test can continue.
    if (!node)
        return true;

    if (request.disallowsShadowContent())
        node = node->document().ancestorInThisScope(node);

    mutableRectBasedTestResult().add(node);

    bool regionFilled = rect.contains(locationInContainer.boundingBox());
    return !regionFilled;
}

void HitTestResult::append(const HitTestResult& other)
{
    ASSERT(isRectBasedTest() && other.isRectBasedTest());

    if (!m_innerNode && other.innerNode()) {
        m_innerNode = other.innerNode();
        m_innerNonSharedNode = other.innerNonSharedNode();
        m_localPoint = other.localPoint();
        m_pointInInnerNodeFrame = other.m_pointInInnerNodeFrame;
        m_innerURLElement = other.URLElement();
        m_scrollbar = other.scrollbar();
        m_isOverWidget = other.isOverWidget();
    }

    if (other.m_rectBasedTestResult) {
        NodeSet& set = mutableRectBasedTestResult();
        for (NodeSet::const_iterator it = other.m_rectBasedTestResult->begin(), last = other.m_rectBasedTestResult->end(); it != last; ++it)
            set.add(it->get());
    }
}

const HitTestResult::NodeSet& HitTestResult::rectBasedTestResult() const
{
    if (!m_rectBasedTestResult)
        m_rectBasedTestResult = adoptPtr(new NodeSet);
    return *m_rectBasedTestResult;
}

HitTestResult::NodeSet& HitTestResult::mutableRectBasedTestResult()
{
    if (!m_rectBasedTestResult)
        m_rectBasedTestResult = adoptPtr(new NodeSet);
    return *m_rectBasedTestResult;
}

Node* HitTestResult::targetNode() const
{
    Node* node = innerNode();
    if (!node)
        return 0;
    if (node->inDocument())
        return node;

    Element* element = node->parentElement();
    if (element && element->inDocument())
        return element;

    return node;
}

Element* HitTestResult::innerElement() const
{
    for (Node* node = m_innerNode.get(); node; node = NodeRenderingTraversal::parent(node))
        if (node->isElementNode())
            return toElement(node);

    return 0;
}

} // namespace WebCore
