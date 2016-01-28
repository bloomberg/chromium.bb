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

#include "core/frame/LocalFrame.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/dom/DocumentType.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/InputMethodController.h"
#include "core/editing/serializers/Serialization.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/Event.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLFrameElementBase.h"
#include "core/html/HTMLPlugInElement.h"
#include "core/input/EventHandler.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/layout/HitTestResult.h"
#include "core/layout/LayoutView.h"
#include "core/layout/compositing/PaintLayerCompositor.h"
#include "core/loader/FrameLoadRequest.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/loader/NavigationScheduler.h"
#include "core/page/ChromeClient.h"
#include "core/page/FocusController.h"
#include "core/page/Page.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/PaintLayer.h"
#include "core/paint/TransformRecorder.h"
#include "core/svg/SVGDocumentExtensions.h"
#include "platform/DragImage.h"
#include "platform/PluginScriptForbiddenScope.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/graphics/paint/ClipRecorder.h"
#include "platform/graphics/paint/SkPictureBuilder.h"
#include "platform/graphics/paint/TransformDisplayItem.h"
#include "platform/text/TextStream.h"
#include "public/platform/WebFrameScheduler.h"
#include "public/platform/WebScreenInfo.h"
#include "public/platform/WebSecurityOrigin.h"
#include "public/platform/WebViewScheduler.h"
#include "third_party/skia/include/core/SkImage.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StdLibExtras.h"

namespace blink {

using namespace HTMLNames;

namespace {

// Convenience class for initializing a GraphicsContext to build a DragImage from a specific
// region specified by |bounds|. After painting the using context(), the DragImage returned from
// createImage() will only contain the content in |bounds| with the appropriate device scale
// factor included.
class DragImageBuilder {
    STACK_ALLOCATED();
public:
    DragImageBuilder(const LocalFrame* localFrame, const IntRect& bounds, Node* draggedNode, float opacity = 1)
        : m_localFrame(localFrame)
        , m_draggedNode(draggedNode)
        , m_bounds(bounds)
        , m_opacity(opacity)
    {
        if (m_draggedNode && m_draggedNode->layoutObject())
            m_draggedNode->layoutObject()->updateDragState(true);

        float deviceScaleFactor = m_localFrame->host()->deviceScaleFactor();
        m_bounds.setWidth(m_bounds.width() * deviceScaleFactor);
        m_bounds.setHeight(m_bounds.height() * deviceScaleFactor);
        m_pictureBuilder = adoptPtr(new SkPictureBuilder(SkRect::MakeIWH(m_bounds.width(), m_bounds.height())));

        AffineTransform transform;
        transform.scale(deviceScaleFactor, deviceScaleFactor);
        transform.translate(-m_bounds.x(), -m_bounds.y());
        context().paintController().createAndAppend<BeginTransformDisplayItem>(*m_localFrame, transform);
    }

    GraphicsContext& context() { return m_pictureBuilder->context(); }

    PassOwnPtr<DragImage> createImage()
    {
        if (m_draggedNode && m_draggedNode->layoutObject())
            m_draggedNode->layoutObject()->updateDragState(false);
        context().paintController().endItem<EndTransformDisplayItem>(*m_localFrame);
        RefPtr<const SkPicture> recording = m_pictureBuilder->endRecording();
        RefPtr<SkImage> skImage = adoptRef(SkImage::NewFromPicture(recording.get(),
            SkISize::Make(m_bounds.width(), m_bounds.height()), nullptr, nullptr));
        RefPtr<Image> image = StaticBitmapImage::create(skImage.release());
        RespectImageOrientationEnum imageOrientation = DoNotRespectImageOrientation;
        if (m_draggedNode && m_draggedNode->layoutObject())
            imageOrientation = LayoutObject::shouldRespectImageOrientation(m_draggedNode->layoutObject());

        float screenDeviceScaleFactor = m_localFrame->page()->chromeClient().screenInfo().deviceScaleFactor;

        return DragImage::create(image.get(), imageOrientation, screenDeviceScaleFactor, InterpolationHigh, m_opacity);
    }

private:
    RawPtrWillBeMember<const LocalFrame> m_localFrame;
    RawPtrWillBeMember<Node> m_draggedNode;
    IntRect m_bounds;
    float m_opacity;
    OwnPtr<SkPictureBuilder> m_pictureBuilder;
};

inline float parentPageZoomFactor(LocalFrame* frame)
{
    Frame* parent = frame->tree().parent();
    if (!parent || !parent->isLocalFrame())
        return 1;
    return toLocalFrame(parent)->pageZoomFactor();
}

inline float parentTextZoomFactor(LocalFrame* frame)
{
    Frame* parent = frame->tree().parent();
    if (!parent || !parent->isLocalFrame())
        return 1;
    return toLocalFrame(parent)->textZoomFactor();
}

} // namespace

PassRefPtrWillBeRawPtr<LocalFrame> LocalFrame::create(FrameLoaderClient* client, FrameHost* host, FrameOwner* owner)
{
    RefPtrWillBeRawPtr<LocalFrame> frame = adoptRefWillBeNoop(new LocalFrame(client, host, owner));
    InspectorInstrumentation::frameAttachedToParent(frame.get());
    return frame.release();
}

void LocalFrame::setView(PassRefPtrWillBeRawPtr<FrameView> view)
{
    ASSERT(!m_view || m_view != view);
    ASSERT(!document() || !document()->isActive());

    eventHandler().clear();

    m_view = view;
}

void LocalFrame::createView(const IntSize& viewportSize, const Color& backgroundColor, bool transparent,
    ScrollbarMode horizontalScrollbarMode, bool horizontalLock,
    ScrollbarMode verticalScrollbarMode, bool verticalLock)
{
    ASSERT(this);
    ASSERT(page());

    bool isLocalRoot = this->isLocalRoot();

    if (isLocalRoot && view())
        view()->setParentVisible(false);

    setView(nullptr);

    RefPtrWillBeRawPtr<FrameView> frameView = nullptr;
    if (isLocalRoot) {
        frameView = FrameView::create(this, viewportSize);

        // The layout size is set by WebViewImpl to support @viewport
        frameView->setLayoutSizeFixedToFrameSize(false);
    } else {
        frameView = FrameView::create(this);
    }

    frameView->setScrollbarModes(horizontalScrollbarMode, verticalScrollbarMode, horizontalLock, verticalLock);

    setView(frameView);

    frameView->updateBackgroundRecursively(backgroundColor, transparent);

    if (isLocalRoot)
        frameView->setParentVisible(true);

    // FIXME: Not clear what the right thing for OOPI is here.
    if (ownerLayoutObject()) {
        HTMLFrameOwnerElement* owner = deprecatedLocalOwner();
        ASSERT(owner);
        // FIXME: OOPI might lead to us temporarily lying to a frame and telling it
        // that it's owned by a FrameOwner that knows nothing about it. If we're
        // lying to this frame, don't let it clobber the existing widget.
        if (owner->contentFrame() == this)
            owner->setWidget(frameView);
    }

    if (owner())
        view()->setCanHaveScrollbars(owner()->scrollingMode() != ScrollbarAlwaysOff);
}

LocalFrame::~LocalFrame()
{
    // Verify that the FrameView has been cleared as part of detaching
    // the frame owner.
    ASSERT(!m_view);
#if !ENABLE(OILPAN)
    // Oilpan: see setDOMWindow() comment why it is acceptable not to
    // explicitly call setDOMWindow() here.
    setDOMWindow(nullptr);
#endif
}

DEFINE_TRACE(LocalFrame)
{
    visitor->trace(m_instrumentingAgents);
#if ENABLE(OILPAN)
    visitor->trace(m_loader);
    visitor->trace(m_navigationScheduler);
    visitor->trace(m_view);
    visitor->trace(m_domWindow);
    visitor->trace(m_pagePopupOwner);
    visitor->trace(m_script);
    visitor->trace(m_editor);
    visitor->trace(m_spellChecker);
    visitor->trace(m_selection);
    visitor->trace(m_eventHandler);
    visitor->trace(m_console);
    visitor->trace(m_inputMethodController);
    HeapSupplementable<LocalFrame>::trace(visitor);
#endif
    LocalFrameLifecycleNotifier::trace(visitor);
    Frame::trace(visitor);
}

DOMWindow* LocalFrame::domWindow() const
{
    return m_domWindow.get();
}

WindowProxy* LocalFrame::windowProxy(DOMWrapperWorld& world)
{
    return m_script->windowProxy(world);
}

void LocalFrame::navigate(Document& originDocument, const KURL& url, bool replaceCurrentItem, UserGestureStatus userGestureStatus)
{
    // TODO(dcheng): Special case for window.open("about:blank") to ensure it loads synchronously into
    // a new window. This is our historical behavior, and it's consistent with the creation of
    // a new iframe with src="about:blank". Perhaps we could get rid of this if we started reporting
    // the initial empty document's url as about:blank? See crbug.com/471239.
    // TODO(japhet): This special case is also necessary for behavior asserted by some extensions tests.
    // Using NavigationScheduler::scheduleNavigationChange causes the navigation to be flagged as a
    // client redirect, which is observable via the webNavigation extension api.
    if (isMainFrame() && !m_loader.stateMachine()->committedFirstRealDocumentLoad()) {
        FrameLoadRequest request(&originDocument, url);
        request.resourceRequest().setHasUserGesture(userGestureStatus == UserGestureStatus::Active);
        m_loader.load(request);
    } else {
        m_navigationScheduler->scheduleLocationChange(&originDocument, url.string(), replaceCurrentItem);
    }
}

void LocalFrame::navigate(const FrameLoadRequest& request)
{
    if (!isNavigationAllowed())
        return;
    m_loader.load(request);
}

void LocalFrame::reload(FrameLoadType loadType, ClientRedirectPolicy clientRedirectPolicy)
{
    ASSERT(loadType == FrameLoadTypeReload || loadType == FrameLoadTypeReloadFromOrigin);
    ASSERT(clientRedirectPolicy == NotClientRedirect || loadType == FrameLoadTypeReload);
    if (clientRedirectPolicy == NotClientRedirect) {
        if (!m_loader.currentItem())
            return;
        FrameLoadRequest request = FrameLoadRequest(
            nullptr, m_loader.resourceRequestForReload(loadType, KURL(), clientRedirectPolicy));
        request.setClientRedirect(clientRedirectPolicy);
        m_loader.load(request, loadType);
    } else {
        m_navigationScheduler->scheduleReload();
    }
}

void LocalFrame::detach(FrameDetachType type)
{
    PluginScriptForbiddenScope forbidPluginDestructorScripting;
    // A lot of the following steps can result in the current frame being
    // detached, so protect a reference to it.
    RefPtrWillBeRawPtr<LocalFrame> protect(this);
    m_loader.stopAllLoaders();
    m_loader.dispatchUnloadEvent();
    detachChildren();
    m_frameScheduler.clear();

    // All done if detaching the subframes brought about a detach of this frame also.
    if (!client())
        return;

    // stopAllLoaders() needs to be called after detachChildren(), because detachChildren()
    // will trigger the unload event handlers of any child frames, and those event
    // handlers might start a new subresource load in this frame.
    m_loader.stopAllLoaders();
    m_loader.detach();
    document()->detach();
    m_loader.clear();
    if (!client())
        return;

    client()->willBeDetached();
    // Notify ScriptController that the frame is closing, since its cleanup ends up calling
    // back to FrameLoaderClient via WindowProxy.
    script().clearForClose();
    ScriptForbiddenScope forbidScript;
    setView(nullptr);
    willDetachFrameHost();
    InspectorInstrumentation::frameDetachedFromParent(this);
    Frame::detach(type);

    // Signal frame destruction here rather than in the destructor.
    // Main motivation is to avoid being dependent on its exact timing (Oilpan.)
    LocalFrameLifecycleNotifier::notifyContextDestroyed();
    // TODO(dcheng): Temporary, to debug https://crbug.com/531291.
    // If this is true, we somehow re-entered LocalFrame::detach. But this is
    // probably OK?
    if (m_supplementStatus == SupplementStatus::Cleared)
        RELEASE_ASSERT(m_supplements.isEmpty());
    // If this is true, we somehow re-entered LocalFrame::detach in the middle
    // of cleaning up supplements.
    RELEASE_ASSERT(m_supplementStatus != SupplementStatus::Clearing);
    RELEASE_ASSERT(m_supplementStatus == SupplementStatus::Uncleared);
    m_supplementStatus = SupplementStatus::Clearing;

    // TODO(haraken): Temporary code to debug https://crbug.com/531291.
    // Check that m_supplements doesn't duplicate OwnPtrs.
    HashSet<void*> supplementPointers;
    for (auto& it : m_supplements) {
        void* pointer = reinterpret_cast<void*>(it.value.get());
        RELEASE_ASSERT(!supplementPointers.contains(pointer));
        supplementPointers.add(pointer);
    }

    m_supplements.clear();
    m_supplementStatus = SupplementStatus::Cleared;
    WeakIdentifierMap<LocalFrame>::notifyObjectDestroyed(this);
}

bool LocalFrame::prepareForCommit()
{
    return loader().prepareForCommit();
}

SecurityContext* LocalFrame::securityContext() const
{
    return document();
}

void LocalFrame::printNavigationErrorMessage(const Frame& targetFrame, const char* reason)
{
    // URLs aren't available for RemoteFrames, so the error message uses their
    // origin instead.
    String targetFrameDescription = targetFrame.isLocalFrame() ? "with URL '" + toLocalFrame(targetFrame).document()->url().string() + "'" : "with origin '" + targetFrame.securityContext()->securityOrigin()->toString() + "'";
    String message = "Unsafe JavaScript attempt to initiate navigation for frame " + targetFrameDescription + " from frame with URL '" + document()->url().string() + "'. " + reason + "\n";

    localDOMWindow()->printErrorMessage(message);
}

WindowProxyManager* LocalFrame::windowProxyManager() const
{
    return m_script->windowProxyManager();
}

void LocalFrame::disconnectOwnerElement()
{
    if (owner()) {
        if (Document* document = this->document())
            document->topDocument().clearAXObjectCache();
    }
    Frame::disconnectOwnerElement();
}

bool LocalFrame::shouldClose()
{
    // TODO(dcheng): This should be fixed to dispatch beforeunload events to
    // both local and remote frames.
    return m_loader.shouldClose();
}

void LocalFrame::willDetachFrameHost()
{
    LocalFrameLifecycleNotifier::notifyWillDetachFrameHost();

    // FIXME: Page should take care of updating focus/scrolling instead of Frame.
    // FIXME: It's unclear as to why this is called more than once, but it is,
    // so page() could be null.
    if (page() && page()->focusController().focusedFrame() == this)
        page()->focusController().setFocusedFrame(nullptr);
    script().clearScriptObjects();

    if (page() && page()->scrollingCoordinator() && m_view)
        page()->scrollingCoordinator()->willDestroyScrollableArea(m_view.get());
}

void LocalFrame::setDOMWindow(PassRefPtrWillBeRawPtr<LocalDOMWindow> domWindow)
{
    // Oilpan: setDOMWindow() cannot be used when finalizing. Which
    // is acceptable as its actions are either not needed or handled
    // by other means --
    //
    //  - LocalFrameLifecycleObserver::willDetachFrameHost() will have
    //    signalled the Inspector frameWindowDiscarded() notifications.
    //    We assume that all LocalFrames are detached, where that notification
    //    will have been done.
    //
    //  - Calling LocalDOMWindow::reset() is not needed (called from
    //    Frame::setDOMWindow().) The Member references it clears will now
    //    die with the window. And the registered DOMWindowProperty instances that don't,
    //    only keep a weak reference to this frame, so there's no need to be
    //    explicitly notified that this frame is going away.
    if (m_domWindow && host())
        host()->consoleMessageStorage().frameWindowDiscarded(m_domWindow.get());
    if (domWindow)
        script().clearWindowProxy();

    if (m_domWindow)
        m_domWindow->reset();
    m_domWindow = domWindow;
}

Document* LocalFrame::document() const
{
    return m_domWindow ? m_domWindow->document() : nullptr;
}

void LocalFrame::setPagePopupOwner(Element& owner)
{
    m_pagePopupOwner = &owner;
}

LayoutView* LocalFrame::contentLayoutObject() const
{
    return document() ? document()->layoutView() : nullptr;
}

void LocalFrame::didChangeVisibilityState()
{
    if (document())
        document()->didChangeVisibilityState();

    WillBeHeapVector<RefPtrWillBeMember<LocalFrame>> childFrames;
    for (Frame* child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            childFrames.append(toLocalFrame(child));
    }

    for (size_t i = 0; i < childFrames.size(); ++i)
        childFrames[i]->didChangeVisibilityState();
}

LocalFrame* LocalFrame::localFrameRoot()
{
    LocalFrame* curFrame = this;
    while (curFrame && curFrame->tree().parent() && curFrame->tree().parent()->isLocalFrame())
        curFrame = toLocalFrame(curFrame->tree().parent());

    return curFrame;
}

String LocalFrame::layerTreeAsText(LayerTreeFlags flags) const
{
    TextStream textStream;
    textStream << localLayerTreeAsText(flags);

    for (Frame* child = tree().firstChild(); child; child = child->tree().traverseNext(this)) {
        if (!child->isLocalFrame())
            continue;
        String childLayerTree = toLocalFrame(child)->localLayerTreeAsText(flags);
        if (!childLayerTree.length())
            continue;

        textStream << "\n\n--------\nFrame: '";
        textStream << child->tree().uniqueName();
        textStream << "'\n--------\n";
        textStream << childLayerTree;
    }

    return textStream.release();
}

void LocalFrame::setPrinting(bool printing, const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkRatio)
{
    // In setting printing, we should not validate resources already cached for the document.
    // See https://bugs.webkit.org/show_bug.cgi?id=43704
    ResourceCacheValidationSuppressor validationSuppressor(document()->fetcher());

    document()->setPrinting(printing);
    view()->adjustMediaTypeForPrinting(printing);

    if (shouldUsePrintingLayout()) {
        view()->forceLayoutForPagination(pageSize, originalPageSize, maximumShrinkRatio);
    } else {
        if (LayoutView* layoutView = view()->layoutView()) {
            layoutView->setPreferredLogicalWidthsDirty();
            layoutView->setNeedsLayout(LayoutInvalidationReason::PrintingChanged);
            layoutView->setShouldDoFullPaintInvalidationForViewAndAllDescendants();
        }
        view()->layout();
        view()->adjustViewSize();
    }

    // Subframes of the one we're printing don't lay out to the page size.
    for (RefPtrWillBeRawPtr<Frame> child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            toLocalFrame(child.get())->setPrinting(printing, FloatSize(), FloatSize(), 0);
    }
}

bool LocalFrame::shouldUsePrintingLayout() const
{
    // Only top frame being printed should be fit to page size.
    // Subframes should be constrained by parents only.
    return document()->printing() && (!tree().parent() || !tree().parent()->isLocalFrame() || !toLocalFrame(tree().parent())->document()->printing());
}

FloatSize LocalFrame::resizePageRectsKeepingRatio(const FloatSize& originalSize, const FloatSize& expectedSize)
{
    FloatSize resultSize;
    if (!contentLayoutObject())
        return FloatSize();

    if (contentLayoutObject()->style()->isHorizontalWritingMode()) {
        ASSERT(fabs(originalSize.width()) > std::numeric_limits<float>::epsilon());
        float ratio = originalSize.height() / originalSize.width();
        resultSize.setWidth(floorf(expectedSize.width()));
        resultSize.setHeight(floorf(resultSize.width() * ratio));
    } else {
        ASSERT(fabs(originalSize.height()) > std::numeric_limits<float>::epsilon());
        float ratio = originalSize.width() / originalSize.height();
        resultSize.setHeight(floorf(expectedSize.height()));
        resultSize.setWidth(floorf(resultSize.height() * ratio));
    }
    return resultSize;
}

void LocalFrame::setPageZoomFactor(float factor)
{
    setPageAndTextZoomFactors(factor, m_textZoomFactor);
}

void LocalFrame::setTextZoomFactor(float factor)
{
    setPageAndTextZoomFactors(m_pageZoomFactor, factor);
}

void LocalFrame::setPageAndTextZoomFactors(float pageZoomFactor, float textZoomFactor)
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
        if (!document->accessSVGExtensions().zoomAndPanEnabled())
            return;
    }

    if (m_pageZoomFactor != pageZoomFactor) {
        if (FrameView* view = this->view()) {
            // Update the scroll position when doing a full page zoom, so the content stays in relatively the same position.
            LayoutPoint scrollPosition = view->scrollPosition();
            float percentDifference = (pageZoomFactor / m_pageZoomFactor);
            view->setScrollPosition(
                DoublePoint(scrollPosition.x() * percentDifference, scrollPosition.y() * percentDifference),
                ProgrammaticScroll);
        }
    }

    m_pageZoomFactor = pageZoomFactor;
    m_textZoomFactor = textZoomFactor;

    for (RefPtrWillBeRawPtr<Frame> child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            toLocalFrame(child.get())->setPageAndTextZoomFactors(m_pageZoomFactor, m_textZoomFactor);
    }

    document->setNeedsStyleRecalc(SubtreeStyleChange, StyleChangeReasonForTracing::create(StyleChangeReason::Zoom));
    document->updateLayoutIgnorePendingStylesheets();
}

void LocalFrame::deviceScaleFactorChanged()
{
    document()->mediaQueryAffectingValueChanged();
    for (RefPtrWillBeRawPtr<Frame> child = tree().firstChild(); child; child = child->tree().nextSibling()) {
        if (child->isLocalFrame())
            toLocalFrame(child.get())->deviceScaleFactorChanged();
    }
}

double LocalFrame::devicePixelRatio() const
{
    if (!m_host)
        return 0;

    double ratio = m_host->deviceScaleFactor();
    ratio *= pageZoomFactor();
    return ratio;
}

PassOwnPtr<DragImage> LocalFrame::paintIntoDragImage(const GlobalPaintFlags globalPaintFlags,
    IntRect paintingRect, Node* draggedNode, float opacity)
{
    ASSERT(document()->isActive());
    // Not flattening compositing layers will result in a broken image being painted.
    ASSERT(globalPaintFlags & GlobalPaintFlattenCompositingLayers);

    DragImageBuilder dragImageBuilder(this, paintingRect, draggedNode, opacity);
    m_view->paintContents(dragImageBuilder.context(), globalPaintFlags, paintingRect);
    return dragImageBuilder.createImage();
}

PassOwnPtr<DragImage> LocalFrame::nodeImage(Node& node)
{
    m_view->updateAllLifecyclePhases();
    LayoutObject* layoutObject = node.layoutObject();
    if (!layoutObject)
        return nullptr;

    // Directly paint boxes as if they are a stacking context.
    if (layoutObject->isBox() && layoutObject->container()) {
        IntRect boundingBox = layoutObject->absoluteBoundingBoxRectIncludingDescendants();
        LayoutPoint paintOffset = boundingBox.location() - layoutObject->offsetFromContainer(layoutObject->container(), LayoutPoint());

        DragImageBuilder dragImageBuilder(this, boundingBox, &node);
        {
            PaintInfo paintInfo(dragImageBuilder.context(), boundingBox, PaintPhase::PaintPhaseForeground, GlobalPaintFlattenCompositingLayers, 0);
            ObjectPainter(*layoutObject).paintAsPseudoStackingContext(paintInfo, LayoutPoint(paintOffset));
        }
        return dragImageBuilder.createImage();
    }

    // TODO(pdr): This will also paint the background if the object contains transparency. We can
    // directly call layoutObject->paint(...) (see: ObjectPainter::paintAsPseudoStackingContext) but
    // painters are inconsistent about which transform space they expect (see: svg, inlines, etc.)
    // TODO(pdr): SVG and inlines are painted offset (crbug.com/579153, crbug.com/579158).
    return paintIntoDragImage(GlobalPaintFlattenCompositingLayers, layoutObject->absoluteBoundingBoxRectIncludingDescendants(), &node);
}

PassOwnPtr<DragImage> LocalFrame::dragImageForSelection(float opacity)
{
    if (!selection().isRange())
        return nullptr;

    m_view->updateAllLifecyclePhases();

    return paintIntoDragImage(GlobalPaintSelectionOnly | GlobalPaintFlattenCompositingLayers,
        enclosingIntRect(selection().bounds()), nullptr, opacity);
}

String LocalFrame::selectedText() const
{
    return selection().selectedText();
}

String LocalFrame::selectedTextForClipboard() const
{
    return selection().selectedTextForClipboard();
}

PositionWithAffinity LocalFrame::positionForPoint(const IntPoint& framePoint)
{
    HitTestResult result = eventHandler().hitTestResultAtPoint(framePoint);
    Node* node = result.innerNodeOrImageMapImage();
    if (!node)
        return PositionWithAffinity();
    LayoutObject* layoutObject = node->layoutObject();
    if (!layoutObject)
        return PositionWithAffinity();
    const PositionWithAffinity position = layoutObject->positionForPoint(result.localPoint());
    if (position.isNull())
        return PositionWithAffinity(firstPositionInOrBeforeNode(node));
    return position;
}

Document* LocalFrame::documentAtPoint(const IntPoint& pointInRootFrame)
{
    if (!view())
        return nullptr;

    IntPoint pt = view()->rootFrameToContents(pointInRootFrame);

    if (!contentLayoutObject())
        return nullptr;
    HitTestResult result = eventHandler().hitTestResultAtPoint(pt, HitTestRequest::ReadOnly | HitTestRequest::Active);
    return result.innerNode() ? &result.innerNode()->document() : nullptr;
}

EphemeralRange LocalFrame::rangeForPoint(const IntPoint& framePoint)
{
    const PositionWithAffinity positionWithAffinity = positionForPoint(framePoint);
    if (positionWithAffinity.isNull())
        return EphemeralRange();

    VisiblePosition position = createVisiblePosition(positionWithAffinity);
    VisiblePosition previous = previousPositionOf(position);
    if (previous.isNotNull()) {
        const EphemeralRange previousCharacterRange = makeRange(previous, position);
        IntRect rect = editor().firstRectForRange(previousCharacterRange);
        if (rect.contains(framePoint))
            return EphemeralRange(previousCharacterRange);
    }

    VisiblePosition next = nextPositionOf(position);
    const EphemeralRange nextCharacterRange = makeRange(position, next);
    if (nextCharacterRange.isNotNull()) {
        IntRect rect = editor().firstRectForRange(nextCharacterRange);
        if (rect.contains(framePoint))
            return EphemeralRange(nextCharacterRange);
    }

    return EphemeralRange();
}

bool LocalFrame::isURLAllowed(const KURL& url) const
{
    // Exempt about: URLs from self-reference check.
    if (url.protocolIsAbout())
        return true;

    // We allow one level of self-reference because some sites depend on that,
    // but we don't allow more than one.
    bool foundSelfReference = false;
    for (const Frame* frame = this; frame; frame = frame->tree().parent()) {
        if (!frame->isLocalFrame())
            continue;
        if (equalIgnoringFragmentIdentifier(toLocalFrame(frame)->document()->url(), url)) {
            if (foundSelfReference)
                return false;
            foundSelfReference = true;
        }
    }
    return true;
}

bool LocalFrame::shouldReuseDefaultView(const KURL& url) const
{
    return loader().stateMachine()->isDisplayingInitialEmptyDocument() && document()->isSecureTransitionTo(url);
}

void LocalFrame::removeSpellingMarkersUnderWords(const Vector<String>& words)
{
    spellChecker().removeSpellingMarkersUnderWords(words);
}

static ScrollResult scrollAreaOnBothAxes(ScrollGranularity granularity, const FloatSize& delta, ScrollableArea& view)
{
    ScrollResultOneDimensional scrolledHorizontal = view.userScroll(ScrollLeft, granularity, delta.width());
    ScrollResultOneDimensional scrolledVertical = view.userScroll(ScrollUp, granularity, delta.height());
    return ScrollResult(scrolledHorizontal.didScroll, scrolledVertical.didScroll, scrolledHorizontal.unusedScrollDelta, scrolledVertical.unusedScrollDelta);
}

// Returns true if a scroll occurred.
ScrollResult LocalFrame::applyScrollDelta(ScrollGranularity granularity, const FloatSize& delta, bool isScrollBegin)
{
    if (isScrollBegin)
        host()->topControls().scrollBegin();

    if (!view() || delta.isZero())
        return ScrollResult(false, false, delta.width(), delta.height());

    FloatSize remainingDelta = delta;

    // If this is main frame, allow top controls to scroll first.
    if (shouldScrollTopControls(delta))
        remainingDelta = host()->topControls().scrollBy(remainingDelta);

    if (remainingDelta.isZero())
        return ScrollResult(delta.width(), delta.height(), 0.0f, 0.0f);

    ScrollResult result = scrollAreaOnBothAxes(granularity, remainingDelta, *view()->scrollableArea());
    result.didScrollX = result.didScrollX || (remainingDelta.width() != delta.width());
    result.didScrollY = result.didScrollY || (remainingDelta.height() != delta.height());

    return result;
}

bool LocalFrame::shouldScrollTopControls(const FloatSize& delta) const
{
    if (!isMainFrame())
        return false;

    // Always give the delta to the top controls if the scroll is in
    // the direction to show the top controls. If it's in the
    // direction to hide the top controls, only give the delta to the
    // top controls when the frame can scroll.
    DoublePoint maximumScrollPosition =
        host()->visualViewport().maximumScrollPositionDouble() +
        toDoubleSize(view()->maximumScrollPositionDouble());
    DoublePoint scrollPosition = host()->visualViewport()
        .visibleRectInDocument().location();
    return delta.height() > 0 || scrollPosition.y() < maximumScrollPosition.y();
}

String LocalFrame::localLayerTreeAsText(unsigned flags) const
{
    if (!contentLayoutObject())
        return String();

    return contentLayoutObject()->compositor()->layerTreeAsText(static_cast<LayerTreeFlags>(flags));
}

bool LocalFrame::shouldThrottleRendering() const
{
    return view() && view()->shouldThrottleRendering();
}

inline LocalFrame::LocalFrame(FrameLoaderClient* client, FrameHost* host, FrameOwner* owner)
    : Frame(client, host, owner)
    , m_loader(this)
    , m_navigationScheduler(NavigationScheduler::create(this))
    , m_script(ScriptController::create(this))
    , m_editor(Editor::create(*this))
    , m_spellChecker(SpellChecker::create(*this))
    , m_selection(FrameSelection::create(this))
    , m_eventHandler(adoptPtrWillBeNoop(new EventHandler(this)))
    , m_console(FrameConsole::create(*this))
    , m_inputMethodController(InputMethodController::create(*this))
    , m_navigationDisableCount(0)
    , m_pageZoomFactor(parentPageZoomFactor(this))
    , m_textZoomFactor(parentTextZoomFactor(this))
    , m_inViewSourceMode(false)
{
    if (isLocalRoot())
        m_instrumentingAgents = InstrumentingAgents::create();
    else
        m_instrumentingAgents = localFrameRoot()->m_instrumentingAgents;
}

WebFrameScheduler* LocalFrame::frameScheduler()
{
    if (!m_frameScheduler.get())
        m_frameScheduler = page()->chromeClient().createFrameScheduler();

    ASSERT(m_frameScheduler.get());
    return m_frameScheduler.get();
}

void LocalFrame::scheduleVisualUpdateUnlessThrottled()
{
    if (shouldThrottleRendering())
        return;
    page()->animator().scheduleVisualUpdate(this);
}

void LocalFrame::updateSecurityOrigin(SecurityOrigin* origin)
{
    script().updateSecurityOrigin(origin);
    frameScheduler()->setFrameOrigin(WebSecurityOrigin(origin));
}

DEFINE_WEAK_IDENTIFIER_MAP(LocalFrame);

FrameNavigationDisabler::FrameNavigationDisabler(LocalFrame& frame)
    : m_frame(&frame)
{
    m_frame->disableNavigation();
}

FrameNavigationDisabler::~FrameNavigationDisabler()
{
    m_frame->enableNavigation();
}

} // namespace blink
