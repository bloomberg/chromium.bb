/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
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
#include "core/html/HTMLFrameOwnerElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderPart.h"
#include "core/rendering/compositing/RenderLayerCompositor.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/weborigin/SecurityPolicy.h"

namespace blink {

typedef HashMap<RefPtr<Widget>, FrameView*> WidgetToParentMap;
static WidgetToParentMap& widgetNewParentMap()
{
    DEFINE_STATIC_LOCAL(WidgetToParentMap, map, ());
    return map;
}

WillBeHeapHashCountedSet<RawPtrWillBeMember<Node> >& SubframeLoadingDisabler::disabledSubtreeRoots()
{
    DEFINE_STATIC_LOCAL(OwnPtrWillBePersistent<WillBeHeapHashCountedSet<RawPtrWillBeMember<Node> > >, nodes, (adoptPtrWillBeNoop(new WillBeHeapHashCountedSet<RawPtrWillBeMember<Node> >())));
    return *nodes;
}

static unsigned s_updateSuspendCount = 0;

HTMLFrameOwnerElement::UpdateSuspendScope::UpdateSuspendScope()
{
    ++s_updateSuspendCount;
}

void HTMLFrameOwnerElement::UpdateSuspendScope::performDeferredWidgetTreeOperations()
{
    WidgetToParentMap map;
    widgetNewParentMap().swap(map);
    WidgetToParentMap::iterator end = map.end();
    for (WidgetToParentMap::iterator it = map.begin(); it != end; ++it) {
        Widget* child = it->key.get();
        ScrollView* currentParent = toScrollView(child->parent());
        FrameView* newParent = it->value;
        if (newParent != currentParent) {
            if (currentParent)
                currentParent->removeChild(child);
            if (newParent)
                newParent->addChild(child);
        }
    }
}

HTMLFrameOwnerElement::UpdateSuspendScope::~UpdateSuspendScope()
{
    ASSERT(s_updateSuspendCount > 0);
    if (s_updateSuspendCount == 1)
        performDeferredWidgetTreeOperations();
    --s_updateSuspendCount;
}

static void moveWidgetToParentSoon(Widget* child, FrameView* parent)
{
    if (!s_updateSuspendCount) {
        if (parent)
            parent->addChild(child);
        else if (toScrollView(child->parent()))
            toScrollView(child->parent())->removeChild(child);
        return;
    }
    widgetNewParentMap().set(child, parent);
}

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_contentFrame(nullptr)
    , m_widget(nullptr)
    , m_sandboxFlags(SandboxNone)
{
}

RenderPart* HTMLFrameOwnerElement::renderPart() const
{
    // HTMLObjectElement and HTMLEmbedElement may return arbitrary renderers
    // when using fallback content.
    if (!renderer() || !renderer()->isRenderPart())
        return 0;
    return toRenderPart(renderer());
}

void HTMLFrameOwnerElement::setContentFrame(Frame& frame)
{
    // Make sure we will not end up with two frames referencing the same owner element.
    ASSERT(!m_contentFrame || m_contentFrame->owner() != this);
    // Disconnected frames should not be allowed to load.
    ASSERT(inDocument());
    m_contentFrame = &frame;

    for (ContainerNode* node = this; node; node = node->parentOrShadowHostNode())
        node->incrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::clearContentFrame()
{
    if (!m_contentFrame)
        return;

    m_contentFrame = nullptr;

    for (ContainerNode* node = this; node; node = node->parentOrShadowHostNode())
        node->decrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::disconnectContentFrame()
{
    // FIXME: Currently we don't do this in removedFrom because this causes an
    // unload event in the subframe which could execute script that could then
    // reach up into this document and then attempt to look back down. We should
    // see if this behavior is really needed as Gecko does not allow this.
    if (RefPtrWillBeRawPtr<Frame> frame = contentFrame()) {
        frame->detach();
#if ENABLE(OILPAN)
        // FIXME: Oilpan: the plugin container is released and finalized here
        // in order to work around the current inability to make the plugin
        // container a FrameDestructionObserver (it needs to effectively be on
        // the heap, and Widget isn't). Hence, release it here while its
        // frame reference is still valid.
        if (m_widget && m_widget->isPluginContainer())
            m_widget = nullptr;
#endif
        frame->disconnectOwnerElement();
    }
}

HTMLFrameOwnerElement::~HTMLFrameOwnerElement()
{
#if ENABLE(OILPAN)
    // An owner must by now have been informed of detachment
    // when the frame was closed.
    ASSERT(!m_contentFrame);
#else
    if (m_contentFrame)
        m_contentFrame->disconnectOwnerElement();
#endif
}

Document* HTMLFrameOwnerElement::contentDocument() const
{
    return (m_contentFrame && m_contentFrame->isLocalFrame()) ? toLocalFrame(m_contentFrame)->document() : 0;
}

LocalDOMWindow* HTMLFrameOwnerElement::contentWindow() const
{
    return m_contentFrame ? m_contentFrame->domWindow() : 0;
}

void HTMLFrameOwnerElement::setSandboxFlags(SandboxFlags flags)
{
    m_sandboxFlags = flags;
}

bool HTMLFrameOwnerElement::isKeyboardFocusable() const
{
    return m_contentFrame && HTMLElement::isKeyboardFocusable();
}

void HTMLFrameOwnerElement::dispatchLoad()
{
    dispatchEvent(Event::create(EventTypeNames::load));
}

Document* HTMLFrameOwnerElement::getSVGDocument(ExceptionState& exceptionState) const
{
    Document* doc = contentDocument();
    if (doc && doc->isSVGDocument())
        return doc;
    return 0;
}

void HTMLFrameOwnerElement::setWidget(PassRefPtr<Widget> widget)
{
    if (widget == m_widget)
        return;

    if (m_widget) {
        if (m_widget->parent())
            moveWidgetToParentSoon(m_widget.get(), 0);
        m_widget = nullptr;
    }

    m_widget = widget;

    RenderWidget* renderWidget = toRenderWidget(renderer());
    if (!renderWidget)
        return;

    if (m_widget) {
        renderWidget->updateOnWidgetChange();

        ASSERT(document().view() == renderWidget->frameView());
        ASSERT(renderWidget->frameView());
        moveWidgetToParentSoon(m_widget.get(), renderWidget->frameView());
    }

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(renderWidget);
}

Widget* HTMLFrameOwnerElement::ownedWidget() const
{
    return m_widget.get();
}

bool HTMLFrameOwnerElement::loadOrRedirectSubframe(const KURL& url, const AtomicString& frameName, bool lockBackForwardList)
{
    RefPtrWillBeRawPtr<LocalFrame> parentFrame = document().frame();
    if (contentFrame()) {
        contentFrame()->navigate(document(), url, Referrer(document().outgoingReferrer(), document().referrerPolicy()), lockBackForwardList);
        return true;
    }

    if (!document().securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(parentFrame.get(), url.string());
        return false;
    }

    if (!SubframeLoadingDisabler::canLoadFrame(*this))
        return false;

    String referrer = SecurityPolicy::generateReferrerHeader(document().referrerPolicy(), url, document().outgoingReferrer());
    return parentFrame->loader().client()->createFrame(url, frameName, Referrer(referrer, document().referrerPolicy()), this);
}

void HTMLFrameOwnerElement::trace(Visitor* visitor)
{
    visitor->trace(m_contentFrame);
    HTMLElement::trace(visitor);
    FrameOwner::trace(visitor);
}


} // namespace blink
