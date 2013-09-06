/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999 Lars Knoll <knoll@kde.org>
 *                     1999 Antti Koivisto <koivisto@kde.org>
 *                     2000 Simon Hausmann <hausmann@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 *                     2001 George Staikos <staikos@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Google Inc.
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
#include "core/page/Frame.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ScriptController.h"
#include "core/dom/DocumentType.h"
#include "core/dom/Event.h"
#include "core/dom/WheelController.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/htmlediting.h"
#include "core/editing/markup.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/DOMWindow.h"
#include "core/page/EventHandler.h"
#include "core/page/FocusController.h"
#include "core/page/FrameDestructionObserver.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/page/animation/AnimationController.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/platform/DragImage.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/ImageBuffer.h"
#include "core/rendering/HitTestResult.h"
#include "core/rendering/RenderLayerCompositor.h"
#include "core/rendering/RenderPart.h"
#include "core/rendering/RenderView.h"
#include "core/svg/SVGDocument.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/RefCountedLeakCounter.h"
#include "wtf/StdLibExtras.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, frameCounter, ("Frame"));

static inline Frame* parentFromOwnerElement(HTMLFrameOwnerElement* ownerElement)
{
    if (!ownerElement)
        return 0;
    return ownerElement->document().frame();
}

static inline float parentPageZoomFactor(Frame* frame)
{
    Frame* parent = frame->tree()->parent();
    if (!parent)
        return 1;
    return parent->pageZoomFactor();
}

static inline float parentTextZoomFactor(Frame* frame)
{
    Frame* parent = frame->tree()->parent();
    if (!parent)
        return 1;
    return parent->textZoomFactor();
}

inline Frame::Frame(Page* page, HTMLFrameOwnerElement* ownerElement, FrameLoaderClient* frameLoaderClient)
    : m_page(page)
    , m_treeNode(this, parentFromOwnerElement(ownerElement))
    , m_loader(this, frameLoaderClient)
    , m_navigationScheduler(this)
    , m_ownerElement(ownerElement)
    , m_script(adoptPtr(new ScriptController(this)))
    , m_editor(adoptPtr(new Editor(*this)))
    , m_selection(adoptPtr(new FrameSelection(this)))
    , m_eventHandler(adoptPtr(new EventHandler(this)))
    , m_animationController(adoptPtr(new AnimationController(this)))
    , m_inputMethodController(InputMethodController::create(this))
    , m_pageZoomFactor(parentPageZoomFactor(this))
    , m_textZoomFactor(parentTextZoomFactor(this))
#if ENABLE(ORIENTATION_EVENTS)
    , m_orientation(0)
#endif
    , m_inViewSourceMode(false)
{
    ASSERT(page);

    if (ownerElement) {
        page->incrementSubframeCount();
        ownerElement->setContentFrame(this);
    }

#ifndef NDEBUG
    frameCounter.increment();
#endif
}

PassRefPtr<Frame> Frame::create(Page* page, HTMLFrameOwnerElement* ownerElement, FrameLoaderClient* client)
{
    RefPtr<Frame> frame = adoptRef(new Frame(page, ownerElement, client));
    if (!ownerElement)
        page->setMainFrame(frame);
    InspectorInstrumentation::frameAttachedToParent(frame.get());
    return frame.release();
}

Frame::~Frame()
{
    setView(0);
    loader()->clear(ClearScriptObjects | ClearWindowObject);

    // FIXME: We should not be doing all this work inside the destructor

#ifndef NDEBUG
    frameCounter.decrement();
#endif

    disconnectOwnerElement();

    HashSet<FrameDestructionObserver*>::iterator stop = m_destructionObservers.end();
    for (HashSet<FrameDestructionObserver*>::iterator it = m_destructionObservers.begin(); it != stop; ++it)
        (*it)->frameDestroyed();
}

bool Frame::inScope(TreeScope* scope) const
{
    ASSERT(scope);
    Document* doc = document();
    if (!doc)
        return false;
    HTMLFrameOwnerElement* owner = doc->ownerElement();
    if (!owner)
        return false;
    return &owner->treeScope() == scope;
}

void Frame::addDestructionObserver(FrameDestructionObserver* observer)
{
    m_destructionObservers.add(observer);
}

void Frame::removeDestructionObserver(FrameDestructionObserver* observer)
{
    m_destructionObservers.remove(observer);
}

void Frame::setView(PassRefPtr<FrameView> view)
{
    // We the custom scroll bars as early as possible to prevent m_doc->detach()
    // from messing with the view such that its scroll bars won't be torn down.
    // FIXME: We should revisit this.
    if (m_view)
        m_view->prepareForDetach();

    // Prepare for destruction now, so any unload event handlers get run and the DOMWindow is
    // notified. If we wait until the view is destroyed, then things won't be hooked up enough for
    // these calls to work.
    if (!view && document() && document()->attached()) {
        // FIXME: We don't call willRemove here. Why is that OK?
        document()->prepareForDestruction();
    }

    if (m_view)
        m_view->unscheduleRelayout();

    eventHandler()->clear();

    m_view = view;

    if (m_view && m_page && m_page->mainFrame() == this)
        m_view->setVisibleContentScaleFactor(m_page->pageScaleFactor());
}

#if ENABLE(ORIENTATION_EVENTS)
void Frame::sendOrientationChangeEvent(int orientation)
{
    m_orientation = orientation;
    if (Document* doc = document())
        doc->dispatchWindowEvent(Event::create(eventNames().orientationchangeEvent));
}
#endif // ENABLE(ORIENTATION_EVENTS)

Settings* Frame::settings() const
{
    return m_page ? &m_page->settings() : 0;
}

void Frame::setPrinting(bool printing, const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkRatio, AdjustViewSizeOrNot shouldAdjustViewSize)
{
    // In setting printing, we should not validate resources already cached for the document.
    // See https://bugs.webkit.org/show_bug.cgi?id=43704
    ResourceCacheValidationSuppressor validationSuppressor(document()->fetcher());

    document()->setPrinting(printing);
    view()->adjustMediaTypeForPrinting(printing);

    document()->styleResolverChanged(RecalcStyleImmediately);
    if (shouldUsePrintingLayout()) {
        view()->forceLayoutForPagination(pageSize, originalPageSize, maximumShrinkRatio, shouldAdjustViewSize);
    } else {
        view()->forceLayout();
        if (shouldAdjustViewSize == AdjustViewSize)
            view()->adjustViewSize();
    }

    // Subframes of the one we're printing don't lay out to the page size.
    for (RefPtr<Frame> child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->setPrinting(printing, FloatSize(), FloatSize(), 0, shouldAdjustViewSize);
}

bool Frame::shouldUsePrintingLayout() const
{
    // Only top frame being printed should be fit to page size.
    // Subframes should be constrained by parents only.
    return document()->printing() && (!tree()->parent() || !tree()->parent()->document()->printing());
}

FloatSize Frame::resizePageRectsKeepingRatio(const FloatSize& originalSize, const FloatSize& expectedSize)
{
    FloatSize resultSize;
    if (!contentRenderer())
        return FloatSize();

    if (contentRenderer()->style()->isHorizontalWritingMode()) {
        ASSERT(fabs(originalSize.width()) > numeric_limits<float>::epsilon());
        float ratio = originalSize.height() / originalSize.width();
        resultSize.setWidth(floorf(expectedSize.width()));
        resultSize.setHeight(floorf(resultSize.width() * ratio));
    } else {
        ASSERT(fabs(originalSize.height()) > numeric_limits<float>::epsilon());
        float ratio = originalSize.width() / originalSize.height();
        resultSize.setHeight(floorf(expectedSize.height()));
        resultSize.setWidth(floorf(resultSize.height() * ratio));
    }
    return resultSize;
}

void Frame::setDOMWindow(PassRefPtr<DOMWindow> domWindow)
{
    m_domWindow = domWindow;
}

Document* Frame::document() const
{
    return m_domWindow ? m_domWindow->document() : 0;
}

RenderView* Frame::contentRenderer() const
{
    return document() ? document()->renderView() : 0;
}

RenderPart* Frame::ownerRenderer() const
{
    HTMLFrameOwnerElement* ownerElement = m_ownerElement;
    if (!ownerElement)
        return 0;
    RenderObject* object = ownerElement->renderer();
    if (!object)
        return 0;
    // FIXME: If <object> is ever fixed to disassociate itself from frames
    // that it has started but canceled, then this can turn into an ASSERT
    // since m_ownerElement would be 0 when the load is canceled.
    // https://bugs.webkit.org/show_bug.cgi?id=18585
    if (!object->isRenderPart())
        return 0;
    return toRenderPart(object);
}

Frame* Frame::frameForWidget(const Widget* widget)
{
    ASSERT_ARG(widget, widget);

    if (RenderWidget* renderer = RenderWidget::find(widget)) {
        if (Node* node = renderer->node())
            return node->document().frame();
    }

    // Assume all widgets are either a FrameView or owned by a RenderWidget.
    // FIXME: That assumption is not right for scroll bars!
    ASSERT_WITH_SECURITY_IMPLICATION(widget->isFrameView());
    return &toFrameView(widget)->frame();
}

void Frame::clearTimers(FrameView *view, Document *document)
{
    if (view) {
        view->unscheduleRelayout();
        if (!RuntimeEnabledFeatures::webAnimationsCSSEnabled())
            view->frame().animation()->suspendAnimationsForDocument(document);
        view->frame().eventHandler()->stopAutoscrollTimer();
    }
}

void Frame::clearTimers()
{
    clearTimers(m_view.get(), document());
}

void Frame::dispatchVisibilityStateChangeEvent()
{
    if (document())
        document()->dispatchVisibilityStateChangeEvent();

    Vector<RefPtr<Frame> > childFrames;
    for (Frame* child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        childFrames.append(child);

    for (size_t i = 0; i < childFrames.size(); ++i)
        childFrames[i]->dispatchVisibilityStateChangeEvent();
}

void Frame::willDetachPage()
{
    // We should never be detatching the page during a Layout.
    RELEASE_ASSERT(!m_view || !m_view->isInLayout());

    if (Frame* parent = tree()->parent())
        parent->loader()->checkLoadComplete();

    HashSet<FrameDestructionObserver*>::iterator stop = m_destructionObservers.end();
    for (HashSet<FrameDestructionObserver*>::iterator it = m_destructionObservers.begin(); it != stop; ++it)
        (*it)->willDetachPage();

    // FIXME: It's unclear as to why this is called more than once, but it is,
    // so page() could be NULL.
    if (page() && page()->focusController().focusedFrame() == this)
        page()->focusController().setFocusedFrame(0);

    if (page() && page()->scrollingCoordinator() && m_view)
        page()->scrollingCoordinator()->willDestroyScrollableArea(m_view.get());

    script()->clearScriptObjects();
}

void Frame::detachFromPage()
{
    // We should never be detatching the page during a Layout.
    RELEASE_ASSERT(!m_view || !m_view->isInLayout());
    m_page = 0;
}

void Frame::disconnectOwnerElement()
{
    if (m_ownerElement) {
        if (Document* doc = document())
            doc->topDocument()->clearAXObjectCache();
        m_ownerElement->clearContentFrame();
        if (m_page)
            m_page->decrementSubframeCount();
    }
    m_ownerElement = 0;
}

String Frame::documentTypeString() const
{
    if (DocumentType* doctype = document()->doctype())
        return createMarkup(doctype);

    return String();
}

String Frame::displayStringModifiedByEncoding(const String& str) const
{
    return document() ? document()->displayStringModifiedByEncoding(str) : str;
}

String Frame::selectedText() const
{
    return selection().selectedText();
}

String Frame::selectedTextForClipboard() const
{
    return selection().selectedTextForClipboard();
}

VisiblePosition Frame::visiblePositionForPoint(const IntPoint& framePoint)
{
    HitTestResult result = eventHandler()->hitTestResultAtPoint(framePoint);
    Node* node = result.innerNonSharedNode();
    if (!node)
        return VisiblePosition();
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return VisiblePosition();
    VisiblePosition visiblePos = VisiblePosition(renderer->positionForPoint(result.localPoint()));
    if (visiblePos.isNull())
        visiblePos = firstPositionInOrBeforeNode(node);
    return visiblePos;
}

Document* Frame::documentAtPoint(const IntPoint& point)
{
    if (!view())
        return 0;

    IntPoint pt = view()->windowToContents(point);
    HitTestResult result = HitTestResult(pt);

    if (contentRenderer())
        result = eventHandler()->hitTestResultAtPoint(pt, HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent);
    return result.innerNode() ? &result.innerNode()->document() : 0;
}

PassRefPtr<Range> Frame::rangeForPoint(const IntPoint& framePoint)
{
    VisiblePosition position = visiblePositionForPoint(framePoint);
    if (position.isNull())
        return 0;

    VisiblePosition previous = position.previous();
    if (previous.isNotNull()) {
        RefPtr<Range> previousCharacterRange = makeRange(previous, position);
        LayoutRect rect = editor().firstRectForRange(previousCharacterRange.get());
        if (rect.contains(framePoint))
            return previousCharacterRange.release();
    }

    VisiblePosition next = position.next();
    if (RefPtr<Range> nextCharacterRange = makeRange(position, next)) {
        LayoutRect rect = editor().firstRectForRange(nextCharacterRange.get());
        if (rect.contains(framePoint))
            return nextCharacterRange.release();
    }

    return 0;
}

void Frame::createView(const IntSize& viewportSize, const Color& backgroundColor, bool transparent,
    const IntSize& fixedLayoutSize, bool useFixedLayout, ScrollbarMode horizontalScrollbarMode, bool horizontalLock,
    ScrollbarMode verticalScrollbarMode, bool verticalLock)
{
    ASSERT(this);
    ASSERT(m_page);

    bool isMainFrame = this == m_page->mainFrame();

    if (isMainFrame && view())
        view()->setParentVisible(false);

    setView(0);

    RefPtr<FrameView> frameView;
    if (isMainFrame) {
        frameView = FrameView::create(this, viewportSize);
        frameView->setFixedLayoutSize(fixedLayoutSize);
        frameView->setUseFixedLayout(useFixedLayout);
    } else
        frameView = FrameView::create(this);

    frameView->setScrollbarModes(horizontalScrollbarMode, verticalScrollbarMode, horizontalLock, verticalLock);

    setView(frameView);

    if (backgroundColor.isValid())
        frameView->updateBackgroundRecursively(backgroundColor, transparent);

    if (isMainFrame)
        frameView->setParentVisible(true);

    if (ownerRenderer())
        ownerRenderer()->setWidget(frameView);

    if (HTMLFrameOwnerElement* owner = ownerElement())
        view()->setCanHaveScrollbars(owner->scrollingMode() != ScrollbarAlwaysOff);
}

String Frame::layerTreeAsText(unsigned flags) const
{
    document()->updateLayout();

    if (!contentRenderer())
        return String();

    return contentRenderer()->compositor()->layerTreeAsText(static_cast<LayerTreeFlags>(flags));
}

String Frame::trackedRepaintRectsAsText() const
{
    if (!m_view)
        return String();
    return m_view->trackedRepaintRectsAsText();
}

void Frame::setPageZoomFactor(float factor)
{
    setPageAndTextZoomFactors(factor, m_textZoomFactor);
}

void Frame::setTextZoomFactor(float factor)
{
    setPageAndTextZoomFactors(m_pageZoomFactor, factor);
}

void Frame::setPageAndTextZoomFactors(float pageZoomFactor, float textZoomFactor)
{
    if (m_pageZoomFactor == pageZoomFactor && m_textZoomFactor == textZoomFactor)
        return;

    Page* page = this->page();
    if (!page)
        return;

    Document* document = this->document();
    if (!document)
        return;

    // Respect SVGs zoomAndPan="disabled" property in standalone SVG documents.
    // FIXME: How to handle compound documents + zoomAndPan="disabled"? Needs SVG WG clarification.
    if (document->isSVGDocument()) {
        if (!toSVGDocument(document)->zoomAndPanEnabled())
            return;
    }

    if (m_pageZoomFactor != pageZoomFactor) {
        if (FrameView* view = this->view()) {
            // Update the scroll position when doing a full page zoom, so the content stays in relatively the same position.
            LayoutPoint scrollPosition = view->scrollPosition();
            float percentDifference = (pageZoomFactor / m_pageZoomFactor);
            view->setScrollPosition(IntPoint(scrollPosition.x() * percentDifference, scrollPosition.y() * percentDifference));
        }
    }

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;

    document->recalcStyle(Force);

    for (RefPtr<Frame> child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->setPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor);

    if (FrameView* view = this->view()) {
        if (document->renderer() && document->renderer()->needsLayout() && view->didFirstLayout())
            view->layout();
    }
}

void Frame::deviceOrPageScaleFactorChanged()
{
    for (RefPtr<Frame> child = tree()->firstChild(); child; child = child->tree()->nextSibling())
        child->deviceOrPageScaleFactorChanged();

    m_page->chrome().client().deviceOrPageScaleFactorChanged();
}

void Frame::notifyChromeClientWheelEventHandlerCountChanged() const
{
    // Ensure that this method is being called on the main frame of the page.
    ASSERT(m_page && m_page->mainFrame() == this);

    unsigned count = 0;
    for (const Frame* frame = this; frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            count += WheelController::from(frame->document())->wheelEventHandlerCount();
    }

    m_page->chrome().client().numWheelEventHandlersChanged(count);
}

bool Frame::isURLAllowed(const KURL& url) const
{
    // We allow one level of self-reference because some sites depend on that,
    // but we don't allow more than one.
    if (m_page->subframeCount() >= Page::maxNumberOfFrames)
        return false;
    bool foundSelfReference = false;
    for (const Frame* frame = this; frame; frame = frame->tree()->parent()) {
        if (equalIgnoringFragmentIdentifier(frame->document()->url(), url)) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    return true;
}

struct ScopedFramePaintingState {
    ScopedFramePaintingState(Frame* frame, Node* node)
        : frame(frame)
        , node(node)
        , paintBehavior(frame->view()->paintBehavior())
        , backgroundColor(frame->view()->baseBackgroundColor())
    {
        ASSERT(!node || node->renderer());
        if (node)
            node->renderer()->updateDragState(true);
    }

    ~ScopedFramePaintingState()
    {
        if (node && node->renderer())
            node->renderer()->updateDragState(false);
        frame->view()->setPaintBehavior(paintBehavior);
        frame->view()->setBaseBackgroundColor(backgroundColor);
        frame->view()->setNodeToDraw(0);
    }

    Frame* frame;
    Node* node;
    PaintBehavior paintBehavior;
    Color backgroundColor;
};

PassOwnPtr<DragImage> Frame::nodeImage(Node* node)
{
    if (!node->renderer())
        return nullptr;

    const ScopedFramePaintingState state(this, node);

    m_view->setPaintBehavior(state.paintBehavior | PaintBehaviorFlattenCompositingLayers);

    // When generating the drag image for an element, ignore the document background.
    m_view->setBaseBackgroundColor(Color::transparent);
    document()->updateLayout();
    m_view->setNodeToDraw(node); // Enable special sub-tree drawing mode.

    // Document::updateLayout may have blown away the original RenderObject.
    RenderObject* renderer = node->renderer();
    if (!renderer)
        return nullptr;

    LayoutRect topLevelRect;
    IntRect paintingRect = pixelSnappedIntRect(renderer->paintingRootRect(topLevelRect));

    float deviceScaleFactor = 1;
    if (m_page)
        deviceScaleFactor = m_page->deviceScaleFactor();
    paintingRect.setWidth(paintingRect.width() * deviceScaleFactor);
    paintingRect.setHeight(paintingRect.height() * deviceScaleFactor);

    OwnPtr<ImageBuffer> buffer(ImageBuffer::create(paintingRect.size(), deviceScaleFactor));
    if (!buffer)
        return nullptr;
    buffer->context()->translate(-paintingRect.x(), -paintingRect.y());
    buffer->context()->clip(FloatRect(0, 0, paintingRect.maxX(), paintingRect.maxY()));

    m_view->paintContents(buffer->context(), paintingRect);

    RefPtr<Image> image = buffer->copyImage();
    return DragImage::create(image.get(), renderer->shouldRespectImageOrientation());
}

PassOwnPtr<DragImage> Frame::dragImageForSelection()
{
    if (!selection().isRange())
        return nullptr;

    const ScopedFramePaintingState state(this, 0);
    m_view->setPaintBehavior(PaintBehaviorSelectionOnly | PaintBehaviorFlattenCompositingLayers);
    document()->updateLayout();

    IntRect paintingRect = enclosingIntRect(selection().bounds());

    float deviceScaleFactor = 1;
    if (m_page)
        deviceScaleFactor = m_page->deviceScaleFactor();
    paintingRect.setWidth(paintingRect.width() * deviceScaleFactor);
    paintingRect.setHeight(paintingRect.height() * deviceScaleFactor);

    OwnPtr<ImageBuffer> buffer(ImageBuffer::create(paintingRect.size(), deviceScaleFactor));
    if (!buffer)
        return nullptr;
    buffer->context()->translate(-paintingRect.x(), -paintingRect.y());
    buffer->context()->clip(FloatRect(0, 0, paintingRect.maxX(), paintingRect.maxY()));

    m_view->paintContents(buffer->context(), paintingRect);

    RefPtr<Image> image = buffer->copyImage();
    return DragImage::create(image.get());
}

double Frame::devicePixelRatio() const
{
    if (!m_page)
        return 0;

    double ratio = m_page->deviceScaleFactor();
    if (RuntimeEnabledFeatures::devicePixelRatioIncludesZoomEnabled())
        ratio *= pageZoomFactor();
    return ratio;
}

} // namespace WebCore
