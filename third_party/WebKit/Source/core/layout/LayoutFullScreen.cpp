/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/layout/LayoutFullScreen.h"

#include "core/dom/Fullscreen.h"
#include "core/frame/FrameHost.h"
#include "core/frame/Settings.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/page/Chrome.h"
#include "core/page/Page.h"

#include "public/platform/WebScreenInfo.h"

using namespace blink;

class LayoutFullScreenPlaceholder final : public LayoutBlockFlow {
public:
    LayoutFullScreenPlaceholder(LayoutFullScreen* owner)
        : LayoutBlockFlow(0)
        , m_owner(owner)
    {
        setDocumentForAnonymous(&owner->document());
    }
private:
    virtual bool isOfType(LayoutObjectType type) const override { return type == LayoutObjectLayoutFullScreenPlaceholder || LayoutBlockFlow::isOfType(type); }
    virtual void willBeDestroyed() override;
    LayoutFullScreen* m_owner;
};

void LayoutFullScreenPlaceholder::willBeDestroyed()
{
    m_owner->setPlaceholder(0);
    LayoutBlockFlow::willBeDestroyed();
}

LayoutFullScreen::LayoutFullScreen()
    : LayoutFlexibleBox(0)
    , m_placeholder(nullptr)
{
    setReplaced(false);
}

LayoutFullScreen* LayoutFullScreen::createAnonymous(Document* document)
{
    LayoutFullScreen* renderer = new LayoutFullScreen();
    renderer->setDocumentForAnonymous(document);
    return renderer;
}

void LayoutFullScreen::willBeDestroyed()
{
    if (m_placeholder) {
        remove();
        if (!m_placeholder->beingDestroyed())
            m_placeholder->destroy();
        ASSERT(!m_placeholder);
    }

    // LayoutObjects are unretained, so notify the document (which holds a pointer to a LayoutFullScreen)
    // if its LayoutFullScreen is destroyed.
    Fullscreen& fullscreen = Fullscreen::from(document());
    if (fullscreen.fullScreenRenderer() == this)
        fullscreen.fullScreenRendererDestroyed();

    LayoutFlexibleBox::willBeDestroyed();
}

void LayoutFullScreen::updateStyle()
{
    RefPtr<LayoutStyle> fullscreenStyle = LayoutStyle::create();

    // Create a stacking context:
    fullscreenStyle->setZIndex(INT_MAX);

    fullscreenStyle->setFontDescription(FontDescription());
    fullscreenStyle->font().update(nullptr);

    fullscreenStyle->setDisplay(FLEX);
    fullscreenStyle->setJustifyContent(ContentPositionCenter);
    fullscreenStyle->setAlignItems(ItemPositionCenter);
    fullscreenStyle->setFlexDirection(FlowColumn);

    fullscreenStyle->setPosition(FixedPosition);
    fullscreenStyle->setLeft(Length(0, blink::Fixed));
    fullscreenStyle->setTop(Length(0, blink::Fixed));
    if (document().page()->settings().pinchVirtualViewportEnabled()) {
        IntSize viewportSize = document().page()->frameHost().pinchViewport().size();
        fullscreenStyle->setWidth(Length(viewportSize.width(), blink::Fixed));
        fullscreenStyle->setHeight(Length(viewportSize.height(), blink::Fixed));
    } else {
        fullscreenStyle->setWidth(Length(100.0, Percent));
        fullscreenStyle->setHeight(Length(100.0, Percent));
    }

    fullscreenStyle->setBackgroundColor(StyleColor(Color::black));

    setStyle(fullscreenStyle);
}

LayoutObject* LayoutFullScreen::wrapRenderer(LayoutObject* object, LayoutObject* parent, Document* document)
{
    // FIXME: We should not modify the structure of the render tree during
    // layout. crbug.com/370459
    DeprecatedDisableModifyRenderTreeStructureAsserts disabler;

    LayoutFullScreen* fullscreenRenderer = LayoutFullScreen::createAnonymous(document);
    fullscreenRenderer->updateStyle();
    if (parent && !parent->isChildAllowed(fullscreenRenderer, fullscreenRenderer->styleRef())) {
        fullscreenRenderer->destroy();
        return 0;
    }
    if (object) {
        // |object->parent()| can be null if the object is not yet attached
        // to |parent|.
        if (LayoutObject* parent = object->parent()) {
            LayoutBlock* containingBlock = object->containingBlock();
            ASSERT(containingBlock);
            // Since we are moving the |object| to a new parent |fullscreenRenderer|,
            // the line box tree underneath our |containingBlock| is not longer valid.
            containingBlock->deleteLineBoxTree();

            parent->addChild(fullscreenRenderer, object);
            object->remove();

            // Always just do a full layout to ensure that line boxes get deleted properly.
            // Because objects moved from |parent| to |fullscreenRenderer|, we want to
            // make new line boxes instead of leaving the old ones around.
            parent->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
            containingBlock->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
        }
        fullscreenRenderer->addChild(object);
        fullscreenRenderer->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
    }

    ASSERT(document);
    Fullscreen::from(*document).setFullScreenRenderer(fullscreenRenderer);
    return fullscreenRenderer;
}

void LayoutFullScreen::unwrapRenderer()
{
    // FIXME: We should not modify the structure of the render tree during
    // layout. crbug.com/370459
    DeprecatedDisableModifyRenderTreeStructureAsserts disabler;

    if (parent()) {
        for (LayoutObject* child = firstChild(); child; child = firstChild()) {
            // We have to clear the override size, because as a flexbox, we
            // may have set one on the child, and we don't want to leave that
            // lying around on the child.
            if (child->isBox())
                toLayoutBox(child)->clearOverrideSize();
            child->remove();
            parent()->addChild(child, this);
            parent()->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
        }
    }
    if (placeholder())
        placeholder()->remove();
    remove();
    destroy();
}

void LayoutFullScreen::setPlaceholder(LayoutBlock* placeholder)
{
    m_placeholder = placeholder;
}

void LayoutFullScreen::createPlaceholder(PassRefPtr<LayoutStyle> style, const LayoutRect& frameRect)
{
    if (style->width().isAuto())
        style->setWidth(Length(frameRect.width(), Fixed));
    if (style->height().isAuto())
        style->setHeight(Length(frameRect.height(), Fixed));

    if (!m_placeholder) {
        m_placeholder = new LayoutFullScreenPlaceholder(this);
        m_placeholder->setStyle(style);
        if (parent()) {
            parent()->addChild(m_placeholder, this);
            parent()->setNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation();
        }
    } else {
        m_placeholder->setStyle(style);
    }
}
