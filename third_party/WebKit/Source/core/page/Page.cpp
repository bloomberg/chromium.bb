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
#include "core/dom/StyleEngine.h"
#include "core/dom/VisitedLinkState.h"
#include "core/editing/Caret.h"
#include "core/editing/UndoStack.h"
#include "core/events/Event.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/frame/DOMTimer.h"
#include "core/frame/DOMWindow.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/Settings.h"
#include "core/inspector/InspectorController.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/HistoryItem.h"
#include "core/loader/ProgressTracker.h"
#include "core/page/AutoscrollController.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DragController.h"
#include "core/page/FocusController.h"
#include "core/page/FrameTree.h"
#include "core/page/PageGroup.h"
#include "core/page/PageLifecycleNotifier.h"
#include "core/page/PointerLockController.h"
#include "core/page/StorageClient.h"
#include "core/page/ValidationMessageClient.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/rendering/RenderView.h"
#include "core/rendering/TextAutosizer.h"
#include "core/storage/StorageNamespace.h"
#include "platform/plugins/PluginData.h"
#include "wtf/HashMap.h"
#include "wtf/RefCountedLeakCounter.h"
#include "wtf/StdLibExtras.h"
#include "wtf/text/Base64.h"

namespace WebCore {

DEFINE_DEBUG_ONLY_GLOBAL(WTF::RefCountedLeakCounter, pageCounter, ("Page"));

// static
HashSet<Page*>& Page::allPages()
{
    DEFINE_STATIC_LOCAL(HashSet<Page*>, allPages, ());
    return allPages;
}

void Page::networkStateChanged(bool online)
{
    Vector<RefPtr<Frame> > frames;

    // Get all the frames of all the pages in all the page groups
    HashSet<Page*>::iterator end = allPages().end();
    for (HashSet<Page*>::iterator it = allPages().begin(); it != end; ++it) {
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree().traverseNext())
            frames.append(frame);
        InspectorInstrumentation::networkStateChanged(*it, online);
    }

    AtomicString eventName = online ? EventTypeNames::online : EventTypeNames::offline;
    for (unsigned i = 0; i < frames.size(); i++)
        frames[i]->domWindow()->dispatchEvent(Event::create(eventName));
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
    : SettingsDelegate(Settings::create())
    , m_autoscrollController(AutoscrollController::create(*this))
    , m_chrome(Chrome::create(this, pageClients.chromeClient))
    , m_dragCaretController(DragCaretController::create())
    , m_dragController(DragController::create(this, pageClients.dragClient))
    , m_focusController(FocusController::create(this))
    , m_contextMenuController(ContextMenuController::create(this, pageClients.contextMenuClient))
    , m_inspectorController(InspectorController::create(this, pageClients.inspectorClient))
    , m_pointerLockController(PointerLockController::create(this))
    , m_historyController(adoptPtr(new HistoryController(this)))
    , m_progress(ProgressTracker::create())
    , m_undoStack(UndoStack::create())
    , m_backForwardClient(pageClients.backForwardClient)
    , m_editorClient(pageClients.editorClient)
    , m_validationMessageClient(0)
    , m_spellCheckerClient(pageClients.spellCheckerClient)
    , m_storageClient(pageClients.storageClient)
    , m_subframeCount(0)
    , m_openedByDOM(false)
    , m_tabKeyCyclesThroughElements(true)
    , m_defersLoading(false)
    , m_pageScaleFactor(1)
    , m_deviceScaleFactor(1)
    , m_group(0)
    , m_timerAlignmentInterval(DOMTimer::visiblePageAlignmentInterval())
    , m_visibilityState(PageVisibilityStateVisible)
    , m_isCursorVisible(true)
#ifndef NDEBUG
    , m_isPainting(false)
#endif
    , m_frameHost(FrameHost::create(*this))
{
    ASSERT(m_editorClient);

    ASSERT(!allPages().contains(this));
    allPages().add(this);

#ifndef NDEBUG
    pageCounter.increment();
#endif
}

Page::~Page()
{
    m_mainFrame->setView(0);
    clearPageGroup();
    allPages().remove(this);

    for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext()) {
        frame->willDetachFrameHost();
        frame->detachFromFrameHost();
    }

    m_inspectorController->inspectedPageDestroyed();

    if (m_scrollingCoordinator)
        m_scrollingCoordinator->pageDestroyed();

#ifndef NDEBUG
    pageCounter.decrement();
#endif
}

ViewportDescription Page::viewportDescription() const
{
    return mainFrame() && mainFrame()->document() ? mainFrame()->document()->viewportDescription() : ViewportDescription();
}

ScrollingCoordinator* Page::scrollingCoordinator()
{
    if (!m_scrollingCoordinator && m_settings->scrollingCoordinatorEnabled())
        m_scrollingCoordinator = ScrollingCoordinator::create(this);

    return m_scrollingCoordinator.get();
}

String Page::mainThreadScrollingReasonsAsText()
{
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
    HashSet<Page*>::iterator end = allPages().end();
    for (HashSet<Page*>::iterator it = allPages().begin(); it != end; ++it)
        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree().traverseNext())
            frame->document()->setNeedsStyleRecalc(SubtreeStyleChange);
}

void Page::setNeedsRecalcStyleInAllFrames()
{
    for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->document()->styleResolverChanged(RecalcStyleDeferred);
}

void Page::setNeedsLayoutInAllFrames()
{
    for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (FrameView* view = frame->view()) {
            view->setNeedsLayout();
            view->scheduleRelayout();
        }
    }
}

void Page::refreshPlugins(bool reload)
{
    if (allPages().isEmpty())
        return;

    PluginData::refresh();

    Vector<RefPtr<Frame> > framesNeedingReload;

    HashSet<Page*>::iterator end = allPages().end();
    for (HashSet<Page*>::iterator it = allPages().begin(); it != end; ++it) {
        Page* page = *it;

        // Clear out the page's plug-in data.
        if (page->m_pluginData)
            page->m_pluginData = 0;

        if (!reload)
            continue;

        for (Frame* frame = (*it)->mainFrame(); frame; frame = frame->tree().traverseNext()) {
            if (frame->document()->containsPlugins())
                framesNeedingReload.append(frame);
        }
    }

    for (size_t i = 0; i < framesNeedingReload.size(); ++i)
        framesNeedingReload[i]->loader().reload();
}

PluginData* Page::pluginData() const
{
    if (!mainFrame()->loader().allowPlugins(NotAboutToInstantiatePlugin))
        return 0;
    if (!m_pluginData)
        m_pluginData = PluginData::create(this);
    return m_pluginData.get();
}

static Frame* incrementFrame(Frame* curr, bool forward, bool wrapFlag)
{
    return forward
        ? curr->tree().traverseNextWithWrap(wrapFlag)
        : curr->tree().traversePreviousWithWrap(wrapFlag);
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
    m_historyController->setDefersLoading(defers);
    for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext())
        frame->loader().setDefersLoading(defers);
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
            view->viewportConstrainedVisibleContentSizeChanged(true, true);
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

void Page::allVisitedStateChanged()
{
    HashSet<Page*>::iterator pagesEnd = allPages().end();
    for (HashSet<Page*>::iterator it = allPages().begin(); it != pagesEnd; ++it) {
        Page* page = *it;
        if (page->m_group != PageGroup::sharedGroup())
            continue;
        for (Frame* frame = page->m_mainFrame.get(); frame; frame = frame->tree().traverseNext())
            frame->document()->visitedLinkState().invalidateStyleForAllLinks();
    }
}

void Page::visitedStateChanged(LinkHash linkHash)
{
    HashSet<Page*>::iterator pagesEnd = allPages().end();
    for (HashSet<Page*>::iterator it = allPages().begin(); it != pagesEnd; ++it) {
        Page* page = *it;
        if (page->m_group != PageGroup::sharedGroup())
            continue;
        for (Frame* frame = page->m_mainFrame.get(); frame; frame = frame->tree().traverseNext())
            frame->document()->visitedLinkState().invalidateStyleForLink(linkHash);
    }
}

StorageNamespace* Page::sessionStorage(bool optionalCreate)
{
    if (!m_sessionStorage && optionalCreate)
        m_sessionStorage = m_storageClient->createSessionStorageNamespace();
    return m_sessionStorage.get();
}

void Page::setTimerAlignmentInterval(double interval)
{
    if (interval == m_timerAlignmentInterval)
        return;

    m_timerAlignmentInterval = interval;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNextWithWrap(false)) {
        if (frame->document())
            frame->document()->didChangeTimerAlignmentInterval();
    }
}

double Page::timerAlignmentInterval() const
{
    return m_timerAlignmentInterval;
}

#if !ASSERT_DISABLED
void Page::checkSubframeCountConsistency() const
{
    ASSERT(m_subframeCount >= 0);

    int subframeCount = 0;
    for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext())
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
        lifecycleNotifier().notifyPageVisibilityChanged();

    if (!isInitialState && m_mainFrame)
        m_mainFrame->didChangeVisibilityState();
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

void Page::settingsChanged(SettingsDelegate::ChangeType changeType)
{
    switch (changeType) {
    case SettingsDelegate::StyleChange:
        setNeedsRecalcStyleInAllFrames();
        break;
    case SettingsDelegate::ViewportDescriptionChange:
        if (mainFrame())
            mainFrame()->document()->updateViewportDescription();
        break;
    case SettingsDelegate::MediaTypeChange:
        m_mainFrame->view()->setMediaType(AtomicString(settings().mediaTypeOverride()));
        setNeedsRecalcStyleInAllFrames();
        break;
    case SettingsDelegate::DNSPrefetchingChange:
        for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext())
            frame->document()->initDNSPrefetch();
        break;
    case SettingsDelegate::MultisamplingChange: {
        HashSet<MultisamplingChangedObserver*>::iterator stop = m_multisamplingChangedObservers.end();
        for (HashSet<MultisamplingChangedObserver*>::iterator it = m_multisamplingChangedObservers.begin(); it != stop; ++it)
            (*it)->multisamplingChanged(m_settings->openGLMultisamplingEnabled());
        break;
    }
    case SettingsDelegate::ImageLoadingChange:
        for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext()) {
            frame->document()->fetcher()->setImagesEnabled(settings().imagesEnabled());
            frame->document()->fetcher()->setAutoLoadImages(settings().loadsImagesAutomatically());
        }
        break;
    case SettingsDelegate::TextAutosizingChange:
        // FTA needs both setNeedsRecalcStyle and setNeedsLayout after a setting change.
        if (RuntimeEnabledFeatures::fastTextAutosizingEnabled()) {
            setNeedsRecalcStyleInAllFrames();
        } else {
            // FIXME: I wonder if this needs to traverse frames like in WebViewImpl::resize, or whether there is only one document per Settings instance?
            for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext()) {
                TextAutosizer* textAutosizer = frame->document()->textAutosizer();
                if (textAutosizer)
                    textAutosizer->recalculateMultipliers();
            }
        }
        // TextAutosizing updates RenderStyle during layout phase (via TextAutosizer::processSubtree).
        // We should invoke setNeedsLayout here.
        setNeedsLayoutInAllFrames();
        break;
    case SettingsDelegate::ScriptEnableChange:
        m_inspectorController->scriptsEnabled(settings().scriptEnabled());
        break;
    case SettingsDelegate::FontFamilyChange:
        for (Frame* frame = mainFrame(); frame; frame = frame->tree().traverseNext())
            frame->document()->styleEngine()->updateGenericFontFamilySettings();
        setNeedsRecalcStyleInAllFrames();
        break;
    }
}

void Page::didCommitLoad(Frame* frame)
{
    lifecycleNotifier().notifyDidCommitLoad(frame);
    if (m_mainFrame == frame) {
        useCounter().didCommitLoad();
        m_inspectorController->didCommitLoadForMainFrame();
    }
}

PageLifecycleNotifier& Page::lifecycleNotifier()
{
    return static_cast<PageLifecycleNotifier&>(LifecycleContext<Page>::lifecycleNotifier());
}

PassOwnPtr<LifecycleNotifier<Page> > Page::createLifecycleNotifier()
{
    return PageLifecycleNotifier::create(this);
}

Page::PageClients::PageClients()
    : chromeClient(0)
    , contextMenuClient(0)
    , editorClient(0)
    , dragClient(0)
    , inspectorClient(0)
    , backForwardClient(0)
    , spellCheckerClient(0)
    , storageClient(0)
{
}

Page::PageClients::~PageClients()
{
}

} // namespace WebCore
