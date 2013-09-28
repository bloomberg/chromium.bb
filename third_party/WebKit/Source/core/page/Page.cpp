/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All Rights Reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
#include "core/page/Page.h"

#include "core/dom/ClientRectList.h"
#include "core/dom/DocumentMarkerController.h"
#include "core/events/Event.h"
#include "core/events/EventNames.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/VisitedLinkState.h"
#include "core/editing/Caret.h"
#include "core/history/BackForwardController.h"
#include "core/history/HistoryItem.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/ProgressTracker.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DOMTimer.h"
#include "core/page/DragController.h"
#include "core/page/FocusController.h"
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/FrameView.h"
#include "core/page/PageConsole.h"
#include "core/page/PageGroup.h"
#include "core/page/PageLifecycleNotifier.h"
#include "core/page/PointerLockController.h"
#include "core/page/Settings.h"
#include "core/page/ValidationMessageClient.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/platform/network/NetworkStateNotifier.h"
#include "core/plugins/PluginData.h"
#include "core/rendering/RenderView.h"
#include "core/storage/StorageNamespace.h"
#include "wtf/HashMap.h"
#include "wtf/RefCountedLeakCounter.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/Base64.h"

namespace WebCore {

static HashSet<Page*>* allPages;

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, pageCounter, ("Page"));

static void networkStateChanged()
{
    Vector<RefPtr<Frame> > frames;

    // Get all the frames of all the pages in all the page groups
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frames.append(frame);
        InspectorInstrumentation::networkStateChanged(*it);
    }

    AtomicString eventName = networkStateNotifier().onLine() ? eventNames().onlineEvent : eventNames().offlineEvent;
    for (unsigned i = 0; i < frames.size(); i++)
        frames[i]->document()->dispatchWindowEvent(Event::create(eventName));
}

float deviceScaleFactor(Frame* frame)
{
    if (!frame)
        return 1;
    Page* page = frame->page();
    if (!page)
        return 1;
    return page->deviceScaleFactor();
}

Page::Page(PageClients& pageClients)
    : m_autoscrollController(AutoscrollController::create())
    , m_chrome(Chrome::create(this, pageClients.chromeClient))
    , m_dragCaretController(DragCaretController::create())
    , m_dragController(DragController::create(this, pageClients.dragClient))
    , m_focusController(FocusController::create(this))
    , m_contextMenuController(ContextMenuController::create(this, pageClients.contextMenuClient))
    , m_inspectorController(InspectorController::create(this, pageClients.inspectorClient))
    , m_pointerLockController(PointerLockController::create(this))
    , m_settings(Settings::create(this))
    , m_progress(ProgressTracker::create())
    , m_backForwardController(BackForwardController::create(this, pageClients.backForwardClient))
    , m_editorClient(pageClients.editorClient)
    , m_validationMessageClient(0)
    , m_subframeCount(0)
    , m_openedByDOM(false)
    , m_tabKeyCyclesThroughElements(true)
    , m_defersLoading(false)
    , m_pageScaleFactor(1)
    , m_deviceScaleFactor(1)
    , m_didLoadUserStyleSheet(false)
    , m_group(0)
    , m_timerAlignmentInterval(DOMTimer::visiblePageAlignmentInterval())
    , m_visibilityState(PageVisibilityStateVisible)
    , m_isCursorVisible(true)
#ifndef NDEBUG
    , m_isPainting(false)
#endif
    , m_console(PageConsole::create(this))
{
    ASSERT(m_editorClient);

    if (!allPages) {
        allPages = new HashSet<Page*>;

        networkStateNotifier().setNetworkStateChangedFunction(networkStateChanged);
    }

    ASSERT(!allPages->contains(this));
    allPages->add(this);

#ifndef NDEBUG
    pageCounter.increment();
#endif
}

Page::~Page()
{
    m_mainFrame->setView(0);
    clearPageGroup();
    allPages->remove(this);

    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        frame->willDetachPage();
        frame->detachFromPage();
    }

    m_inspectorController->inspectedPageDestroyed();

    if (m_scrollingCoordinator)
        m_scrollingCoordinator->pageDestroyed();

#ifndef NDEBUG
    pageCounter.decrement();
#endif
}

ViewportArguments Page::viewportArguments() const
{
    return mainFrame() && mainFrame()->document() ? mainFrame()->document()->viewportArguments() : ViewportArguments();
}

bool Page::autoscrollInProgress() const
{
    return m_autoscrollController->autoscrollInProgress();
}

bool Page::autoscrollInProgress(const RenderBox* renderer) const
{
    return m_autoscrollController->autoscrollInProgress(renderer);
}

bool Page::panScrollInProgress() const
{
    return m_autoscrollController->panScrollInProgress();
}

void Page::startAutoscrollForSelection(RenderObject* renderer)
{
    return m_autoscrollController->startAutoscrollForSelection(renderer);
}

void Page::stopAutoscrollIfNeeded(RenderObject* renderer)
{
    m_autoscrollController->stopAutoscrollIfNeeded(renderer);
}


void Page::stopAutoscrollTimer()
{
    m_autoscrollController->stopAutoscrollTimer();
}

void Page::updateAutoscrollRenderer()
{
    m_autoscrollController->updateAutoscrollRenderer();
}

void Page::updateDragAndDrop(Node* dropTargetNode, const IntPoint& eventPosition, double eventTime)
{
    m_autoscrollController->updateDragAndDrop(dropTargetNode, eventPosition, eventTime);
}

#if OS(WIN)
void Page::handleMouseReleaseForPanScrolling(Frame* frame, const PlatformMouseEvent& point)
{
    m_autoscrollController->handleMouseReleaseForPanScrolling(frame, point);
}

void Page::startPanScrolling(RenderBox* renderer, const IntPoint& point)
{
    m_autoscrollController->startPanScrolling(renderer, point);
}
#endif


ScrollingCoordinator* Page::scrollingCoordinator()
{
    if (!m_scrollingCoordinator && m_settings->scrollingCoordinatorEnabled())
        m_scrollingCoordinator = ScrollingCoordinator::create(this);

    return m_scrollingCoordinator.get();
}

String Page::mainThreadScrollingReasonsAsText()
{
    if (Document* document = m_mainFrame->document())
        document->updateLayout();

    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
        return scrollingCoordinator->mainThreadScrollingReasonsAsText();

    return String();
}

PassRefPtr<ClientRectList> Page::nonFastScrollableRects(const Frame* frame)
{
    if (Document* document = m_mainFrame->document())
        document->updateLayout();

    Vector<IntRect> rects;
    if (ScrollingCoordinator* scrollingCoordinator = this->scrollingCoordinator())
        rects = scrollingCoordinator->computeShouldHandleScrollGestureOnMainThreadRegion(frame, IntPoint()).rects();

    Vector<FloatQuad> quads(rects.size());
    for (size_t i = 0; i < rects.size(); ++i)
        quads[i] = FloatRect(rects[i]);
    return ClientRectList::create(quads);
}

void Page::setMainFrame(PassRefPtr<Frame> mainFrame)
{
    ASSERT(!m_mainFrame); // Should only be called during initialization
    m_mainFrame = mainFrame;
}

void Page::documentDetached(Document* document)
{
    m_pointerLockController->documentDetached(document);
    m_contextMenuController->documentDetached(document);
    if (m_validationMessageClient)
        m_validationMessageClient->documentDetached(*document);
}

bool Page::openedByDOM() const
{
    return m_openedByDOM;
}

void Page::setOpenedByDOM()
{
    m_openedByDOM = true;
}

void Page::goToItem(HistoryItem* item)
{
    // stopAllLoaders may end up running onload handlers, which could cause further history traversals that may lead to the passed in HistoryItem
    // being deref()-ed. Make sure we can still use it with HistoryController::goToItem later.
    RefPtr<HistoryItem> protector(item);

    if (m_mainFrame->loader()->history()->shouldStopLoadingForHistoryItem(item))
        m_mainFrame->loader()->stopAllLoaders();

    m_mainFrame->loader()->history()->goToItem(item);
}

void Page::clearPageGroup()
{
    if (!m_group)
        return;
    m_group->removePage(this);
    m_group = 0;
}

void Page::setGroupType(PageGroupType type)
{
    clearPageGroup();

    switch (type) {
    case InspectorPageGroup:
        m_group = PageGroup::inspectorGroup();
        break;
    case PrivatePageGroup:
        m_group = PageGroup::create();
        break;
    case SharedPageGroup:
        m_group = PageGroup::sharedGroup();
        break;
    }

    m_group->addPage(this);
}

void Page::scheduleForcedStyleRecalcForAllPages()
{
    if (!allPages)
        return;
    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it)
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext())
            frame->document()->setNeedsStyleRecalc();
}

void Page::setNeedsRecalcStyleInAllFrames()
{
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->document()->styleResolverChanged(RecalcStyleDeferred);
}

void Page::refreshPlugins(bool reload)
{
    if (!allPages)
        return;

    PluginData::refresh();

    Vector<RefPtr<Frame> > framesNeedingReload;

    HashSet<Page*>::iterator end = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != end; ++it) {
        Page* page = *it;

        // Clear out the page's plug-in data.
        if (page->m_pluginData)
            page->m_pluginData = 0;

        if (!reload)
            continue;

        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
            if (frame->loader()->containsPlugins())
                framesNeedingReload.append(frame);
        }
    }

    for (size_t i = 0; i < framesNeedingReload.size(); ++i)
        framesNeedingReload[i]->loader()->reload();
}

PluginData* Page::pluginData() const
{
    if (!mainFrame()->loader()->allowPlugins(NotAboutToInstantiatePlugin))
        return 0;
    if (!m_pluginData)
        m_pluginData = PluginData::create(this);
    return m_pluginData.get();
}

static Frame* incrementFrame(Frame* curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree()->traverseNextWithWrap(wrapFlag)
        : curr->tree()->traversePreviousWithWrap(wrapFlag);
}

void Page::unmarkAllTextMatches()
{
    if (!mainFrame())
        return;

    Frame* frame = mainFrame();
    do {
        frame->document()->markers()->removeMarkers(DocumentMarker::TextMatch);
        frame = incrementFrame(frame, true, false);
    } while (frame);
}

void Page::setDefersLoading(bool defers)
{
    if (defers == m_defersLoading)
        return;

    m_defersLoading = defers;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->loader()->setDefersLoading(defers);
}

void Page::setPageScaleFactor(float scale, const IntPoint& origin)
{
    FrameView* view = mainFrame()->view();

    if (scale != m_pageScaleFactor) {
        m_pageScaleFactor = scale;

        if (view)
            view->setVisibleContentScaleFactor(scale);

        mainFrame()->deviceOrPageScaleFactorChanged();
        m_chrome->client().deviceOrPageScaleFactorChanged();

        if (view)
            view->setViewportConstrainedObjectsNeedLayout();
    }

    if (view && view->scrollPosition() != origin)
        view->notifyScrollPositionChanged(origin);
}

void Page::setDeviceScaleFactor(float scaleFactor)
{
    if (m_deviceScaleFactor == scaleFactor)
        return;

    m_deviceScaleFactor = scaleFactor;
    setNeedsRecalcStyleInAllFrames();

    if (mainFrame()) {
        mainFrame()->deviceOrPageScaleFactorChanged();
        m_chrome->client().deviceOrPageScaleFactorChanged();
    }
}

void Page::setPagination(const Pagination& pagination)
{
    if (m_pagination == pagination)
        return;

    m_pagination = pagination;

    setNeedsRecalcStyleInAllFrames();
}

void Page::userStyleSheetLocationChanged()
{
    // FIXME: Eventually we will move to a model of just being handed the sheet
    // text instead of loading the URL ourselves.
    KURL url = m_settings->userStyleSheetLocation();

    m_didLoadUserStyleSheet = false;
    m_userStyleSheet = String();

    // Data URLs with base64-encoded UTF-8 style sheets are common. We can process them
    // synchronously and avoid using a loader.
    if (url.protocolIsData() && url.string().startsWith("data:text/css;charset=utf-8;base64,")) {
        m_didLoadUserStyleSheet = true;

        Vector<char> styleSheetAsUTF8;
        if (base64Decode(decodeURLEscapeSequences(url.string().substring(35)), styleSheetAsUTF8, Base64IgnoreWhitespace))
            m_userStyleSheet = String::fromUTF8(styleSheetAsUTF8.data(), styleSheetAsUTF8.size());
    }

    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (frame->document())
            frame->document()->styleEngine()->updatePageUserSheet();
    }
}

const String& Page::userStyleSheet() const
{
    return m_userStyleSheet;
}

void Page::allVisitedStateChanged(PageGroup* group)
{
    ASSERT(group);
    if (!allPages)
        return;

    HashSet<Page*>::iterator pagesEnd = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != pagesEnd; ++it) {
        Page* page = *it;
        if (page->m_group != group)
            continue;
        for (Frame* frame = page->m_mainFrame.get(); frame; frame = frame->tree()->traverseNext())
            frame->document()->visitedLinkState()->invalidateStyleForAllLinks();
    }
}

void Page::visitedStateChanged(PageGroup* group, LinkHash linkHash)
{
    ASSERT(group);
    if (!allPages)
        return;

    HashSet<Page*>::iterator pagesEnd = allPages->end();
    for (HashSet<Page*>::iterator it = allPages->begin(); it != pagesEnd; ++it) {
        Page* page = *it;
        if (page->m_group != group)
            continue;
        for (Frame* frame = page->m_mainFrame.get(); frame; frame = frame->tree()->traverseNext())
            frame->document()->visitedLinkState()->invalidateStyleForLink(linkHash);
    }
}

StorageNamespace* Page::sessionStorage(bool optionalCreate)
{
    if (!m_sessionStorage && optionalCreate)
        m_sessionStorage = StorageNamespace::sessionStorageNamespace(this);
    return m_sessionStorage.get();
}

void Page::setTimerAlignmentInterval(double interval)
{
    if (interval == m_timerAlignmentInterval)
        return;

    m_timerAlignmentInterval = interval;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNextWithWrap(false)) {
        if (frame->document())
            frame->document()->didChangeTimerAlignmentInterval();
    }
}

double Page::timerAlignmentInterval() const
{
    return m_timerAlignmentInterval;
}

void Page::dnsPrefetchingStateChanged()
{
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->document()->initDNSPrefetch();
}

#if !ASSERT_DISABLED
void Page::checkSubframeCountConsistency() const
{
    ASSERT(m_subframeCount >= 0);

    int subframeCount = 0;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree()->traverseNext())
        ++subframeCount;

    ASSERT(m_subframeCount + 1 == subframeCount);
}
#endif

void Page::setVisibilityState(PageVisibilityState visibilityState, bool isInitialState)
{
    if (m_visibilityState == visibilityState)
        return;
    m_visibilityState = visibilityState;

    if (visibilityState == WebCore::PageVisibilityStateHidden)
        setTimerAlignmentInterval(DOMTimer::hiddenPageAlignmentInterval());
    else
        setTimerAlignmentInterval(DOMTimer::visiblePageAlignmentInterval());

    if (!isInitialState)
        lifecycleNotifier()->notifyPageVisibilityChanged();

    if (!isInitialState && m_mainFrame)
        m_mainFrame->dispatchVisibilityStateChangeEvent();
}

PageVisibilityState Page::visibilityState() const
{
    return m_visibilityState;
}

void Page::addMultisamplingChangedObserver(MultisamplingChangedObserver* observer)
{
    m_multisamplingChangedObservers.add(observer);
}

void Page::removeMultisamplingChangedObserver(MultisamplingChangedObserver* observer)
{
    m_multisamplingChangedObservers.remove(observer);
}

void Page::multisamplingChanged()
{
    HashSet<MultisamplingChangedObserver*>::iterator stop = m_multisamplingChangedObservers.end();
    for (HashSet<MultisamplingChangedObserver*>::iterator it = m_multisamplingChangedObservers.begin(); it != stop; ++it)
        (*it)->multisamplingChanged(m_settings->openGLMultisamplingEnabled());
}

void Page::didCommitLoad(Frame* frame)
{
    lifecycleNotifier()->notifyDidCommitLoad(frame);
    if (m_mainFrame == frame)
        useCounter().didCommitLoad();
}

PageLifecycleNotifier* Page::lifecycleNotifier()
{
    return static_cast<PageLifecycleNotifier*>(LifecycleContext::lifecycleNotifier());
}

PassOwnPtr<LifecycleNotifier> Page::createLifecycleNotifier()
{
    return PageLifecycleNotifier::create(this);
}

Page::PageClients::PageClients()
    : chromeClient(0)
    , contextMenuClient(0)
    , editorClient(0)
    , dragClient(0)
    , inspectorClient(0)
{
}

Page::PageClients::~PageClients()
{
}

} // namespace WebCore
