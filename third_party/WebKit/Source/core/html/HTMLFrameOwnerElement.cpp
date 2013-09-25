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

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/rendering/RenderPart.h"
#include "core/svg/SVGDocument.h"
#include "weborigin/SecurityOrigin.h"
#include "weborigin/SecurityPolicy.h"

namespace WebCore {

HTMLFrameOwnerElement::HTMLFrameOwnerElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , m_contentFrame(0)
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

void HTMLFrameOwnerElement::setContentFrame(Frame* frame)
{
    // Make sure we will not end up with two frames referencing the same owner element.
    ASSERT(!m_contentFrame || m_contentFrame->ownerElement() != this);
    ASSERT(frame);
    // Disconnected frames should not be allowed to load.
    ASSERT(inDocument());
    m_contentFrame = frame;

    for (ContainerNode* node = this; node; node = node->parentOrShadowHostNode())
        node->incrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::clearContentFrame()
{
    if (!m_contentFrame)
        return;

    m_contentFrame = 0;

    for (ContainerNode* node = this; node; node = node->parentOrShadowHostNode())
        node->decrementConnectedSubframeCount();
}

void HTMLFrameOwnerElement::disconnectContentFrame()
{
    // FIXME: Currently we don't do this in removedFrom because this causes an
    // unload event in the subframe which could execute script that could then
    // reach up into this document and then attempt to look back down. We should
    // see if this behavior is really needed as Gecko does not allow this.
    if (Frame* frame = contentFrame()) {
        RefPtr<Frame> protect(frame);
        frame->loader()->frameDetached();
        frame->disconnectOwnerElement();
    }
}

HTMLFrameOwnerElement::~HTMLFrameOwnerElement()
{
    if (m_contentFrame)
        m_contentFrame->disconnectOwnerElement();
}

Document* HTMLFrameOwnerElement::contentDocument() const
{
    return m_contentFrame ? m_contentFrame->document() : 0;
}

DOMWindow* HTMLFrameOwnerElement::contentWindow() const
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

SVGDocument* HTMLFrameOwnerElement::getSVGDocument(ExceptionState& es) const
{
    Document* doc = contentDocument();
    if (doc && doc->isSVGDocument())
        return toSVGDocument(doc);
    // Spec: http://www.w3.org/TR/SVG/struct.html#InterfaceGetSVGDocument
    es.throwUninformativeAndGenericDOMException(NotSupportedError);
    return 0;
}

bool HTMLFrameOwnerElement::loadOrRedirectSubframe(const KURL& url, const AtomicString& frameName, bool lockBackForwardList)
{
    RefPtr<Frame> parentFrame = document().frame();
    if (contentFrame()) {
        contentFrame()->navigationScheduler()->scheduleLocationChange(document().securityOrigin(), url.string(), parentFrame->loader()->outgoingReferrer(), lockBackForwardList);
        return true;
    }

    if (!document().securityOrigin()->canDisplay(url)) {
        FrameLoader::reportLocalLoadFailed(parentFrame.get(), url.string());
        return false;
    }

    if (!SubframeLoadingDisabler::canLoadFrame(this))
        return false;

    String referrer = SecurityPolicy::generateReferrerHeader(document().referrerPolicy(), url, parentFrame->loader()->outgoingReferrer());
    RefPtr<Frame> childFrame = parentFrame->loader()->client()->createFrame(url, frameName, referrer, this);

    if (!childFrame)  {
        parentFrame->loader()->checkCompleted();
        return false;
    }

    // All new frames will have m_isComplete set to true at this point due to synchronously loading
    // an empty document in FrameLoader::init(). But many frames will now be starting an
    // asynchronous load of url, so we set m_isComplete to false and then check if the load is
    // actually completed below. (Note that we set m_isComplete to false even for synchronous
    // loads, so that checkCompleted() below won't bail early.)
    // FIXME: Can we remove this entirely? m_isComplete normally gets set to false when a load is committed.
    childFrame->loader()->started();

    RenderObject* renderObject = renderer();
    FrameView* view = childFrame->view();
    if (renderObject && renderObject->isWidget() && view)
        toRenderWidget(renderObject)->setWidget(view);

    // Some loads are performed synchronously (e.g., about:blank and loads
    // cancelled by returning a null ResourceRequest from requestFromDelegate).
    // In these cases, the synchronous load would have finished
    // before we could connect the signals, so make sure to send the
    // completed() signal for the child by hand and mark the load as being
    // complete.
    // FIXME: In this case the Frame will have finished loading before
    // it's being added to the child list. It would be a good idea to
    // create the child first, then invoke the loader separately.
    if (childFrame->loader()->state() == FrameStateComplete && !childFrame->loader()->policyDocumentLoader())
        childFrame->loader()->checkCompleted();
    return true;
}


} // namespace WebCore
