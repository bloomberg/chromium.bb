/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Apple Inc. All
 * Rights Reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#include "core/page/Page.h"

#include "bindings/core/v8/ScriptController.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/ClientRectList.h"
#include "core/dom/StyleChangeReason.h"
#include "core/dom/StyleEngine.h"
#include "core/dom/VisitedLinkState.h"
#include "core/editing/DragCaret.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/events/Event.h"
#include "core/frame/BrowserControls.h"
#include "core/frame/DOMTimer.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/PageScaleConstraints.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/html/HTMLMediaElement.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/layout/TextAutosizer.h"
#include "core/page/AutoscrollController.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DragController.h"
#include "core/page/FocusController.h"
#include "core/page/PointerLockController.h"
#include "core/page/ScopedPageSuspender.h"
#include "core/page/ValidationMessageClient.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/PaintLayer.h"
#include "platform/WebFrameScheduler.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/plugins/PluginData.h"
#include "public/platform/Platform.h"

namespace blink {

// Set of all live pages; includes internal Page objects that are
// not observable from scripts.
static Page::PageSet& allPages() {
  DEFINE_STATIC_LOCAL(Page::PageSet, pages, ());
  return pages;
}

Page::PageSet& Page::ordinaryPages() {
  DEFINE_STATIC_LOCAL(Page::PageSet, pages, ());
  return pages;
}

float deviceScaleFactorDeprecated(LocalFrame* frame) {
  if (!frame)
    return 1;
  Page* page = frame->page();
  if (!page)
    return 1;
  return page->deviceScaleFactorDeprecated();
}

Page* Page::createOrdinary(PageClients& pageClients) {
  Page* page = create(pageClients);
  ordinaryPages().insert(page);
  if (ScopedPageSuspender::isActive())
    page->setSuspended(true);
  return page;
}

Page::Page(PageClients& pageClients)
    : SettingsDelegate(Settings::create()),
      m_animator(PageAnimator::create(*this)),
      m_autoscrollController(AutoscrollController::create(*this)),
      m_chromeClient(pageClients.chromeClient),
      m_dragCaret(DragCaret::create()),
      m_dragController(DragController::create(this)),
      m_focusController(FocusController::create(this)),
      m_contextMenuController(
          ContextMenuController::create(this, pageClients.contextMenuClient)),
      m_pageScaleConstraintsSet(PageScaleConstraintsSet::create()),
      m_pointerLockController(PointerLockController::create(this)),
      m_browserControls(BrowserControls::create(*this)),
      m_consoleMessageStorage(new ConsoleMessageStorage()),
      m_eventHandlerRegistry(new EventHandlerRegistry(*this)),
      m_globalRootScrollerController(
          TopDocumentRootScrollerController::create(*this)),
      m_visualViewport(VisualViewport::create(*this)),
      m_overscrollController(
          OverscrollController::create(visualViewport(), chromeClient())),
      m_mainFrame(nullptr),
      m_editorClient(pageClients.editorClient),
      m_spellCheckerClient(pageClients.spellCheckerClient),
      m_useCounter(pageClients.chromeClient &&
                           pageClients.chromeClient->isSVGImageChromeClient()
                       ? UseCounter::SVGImageContext
                       : UseCounter::DefaultContext),
      m_openedByDOM(false),
      m_tabKeyCyclesThroughElements(true),
      m_suspended(false),
      m_deviceScaleFactor(1),
      m_visibilityState(PageVisibilityStateVisible),
      m_isCursorVisible(true),
      m_subframeCount(0),
      m_frameHost(FrameHost::create(*this)) {
  ASSERT(m_editorClient);

  ASSERT(!allPages().contains(this));
  allPages().insert(this);
}

Page::~Page() {
  // willBeDestroyed() must be called before Page destruction.
  ASSERT(!m_mainFrame);
}

void Page::closeSoon() {
  // Make sure this Page can no longer be found by JS.
  m_isClosing = true;

  // TODO(dcheng): Try to remove this in a followup, it's not obviously needed.
  if (m_mainFrame->isLocalFrame())
    toLocalFrame(m_mainFrame)->loader().stopAllLoaders();

  chromeClient().closeWindowSoon();
}

ViewportDescription Page::viewportDescription() const {
  return mainFrame() && mainFrame()->isLocalFrame() &&
                 deprecatedLocalMainFrame()->document()
             ? deprecatedLocalMainFrame()->document()->viewportDescription()
             : ViewportDescription();
}

ScrollingCoordinator* Page::scrollingCoordinator() {
  if (!m_scrollingCoordinator && m_settings->getAcceleratedCompositingEnabled())
    m_scrollingCoordinator = ScrollingCoordinator::create(this);

  return m_scrollingCoordinator.get();
}

PageScaleConstraintsSet& Page::pageScaleConstraintsSet() {
  return *m_pageScaleConstraintsSet;
}

const PageScaleConstraintsSet& Page::pageScaleConstraintsSet() const {
  return *m_pageScaleConstraintsSet;
}

BrowserControls& Page::browserControls() {
  return *m_browserControls;
}

const BrowserControls& Page::browserControls() const {
  return *m_browserControls;
}

ConsoleMessageStorage& Page::consoleMessageStorage() {
  return *m_consoleMessageStorage;
}

const ConsoleMessageStorage& Page::consoleMessageStorage() const {
  return *m_consoleMessageStorage;
}

EventHandlerRegistry& Page::eventHandlerRegistry() {
  return *m_eventHandlerRegistry;
}

const EventHandlerRegistry& Page::eventHandlerRegistry() const {
  return *m_eventHandlerRegistry;
}

TopDocumentRootScrollerController& Page::globalRootScrollerController() const {
  return *m_globalRootScrollerController;
}

VisualViewport& Page::visualViewport() {
  return *m_visualViewport;
}

const VisualViewport& Page::visualViewport() const {
  return *m_visualViewport;
}

OverscrollController& Page::overscrollController() {
  return *m_overscrollController;
}

const OverscrollController& Page::overscrollController() const {
  return *m_overscrollController;
}

ClientRectList* Page::nonFastScrollableRects(const LocalFrame* frame) {
  DisableCompositingQueryAsserts disabler;
  if (ScrollingCoordinator* scrollingCoordinator =
          this->scrollingCoordinator()) {
    // Hits in compositing/iframes/iframe-composited-scrolling.html
    scrollingCoordinator->updateAfterCompositingChangeIfNeeded();
  }

  GraphicsLayer* layer =
      frame->view()->layoutViewportScrollableArea()->layerForScrolling();
  if (!layer)
    return ClientRectList::create();
  return ClientRectList::create(
      layer->platformLayer()->nonFastScrollableRegion());
}

void Page::setMainFrame(Frame* mainFrame) {
  // Should only be called during initialization or swaps between local and
  // remote frames.
  // FIXME: Unfortunately we can't assert on this at the moment, because this
  // is called in the base constructor for both LocalFrame and RemoteFrame,
  // when the vtables for the derived classes have not yet been setup.
  m_mainFrame = mainFrame;
}

void Page::willUnloadDocument(const Document& document) {
  if (m_validationMessageClient)
    m_validationMessageClient->willUnloadDocument(document);
}

void Page::documentDetached(Document* document) {
  m_pointerLockController->documentDetached(document);
  m_contextMenuController->documentDetached(document);
  if (m_validationMessageClient)
    m_validationMessageClient->documentDetached(*document);
  m_hostsUsingFeatures.documentDetached(*document);
}

bool Page::openedByDOM() const {
  return m_openedByDOM;
}

void Page::setOpenedByDOM() {
  m_openedByDOM = true;
}

void Page::platformColorsChanged() {
  for (const Page* page : allPages())
    for (Frame* frame = page->mainFrame(); frame;
         frame = frame->tree().traverseNext()) {
      if (frame->isLocalFrame())
        toLocalFrame(frame)->document()->platformColorsChanged();
    }
}

void Page::setNeedsRecalcStyleInAllFrames() {
  for (Frame* frame = mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (frame->isLocalFrame())
      toLocalFrame(frame)->document()->setNeedsStyleRecalc(
          SubtreeStyleChange,
          StyleChangeReasonForTracing::create(StyleChangeReason::Settings));
  }
}

void Page::refreshPlugins() {
  PluginData::refreshBrowserSidePluginCache();

  for (const Page* page : allPages()) {
    // Clear out the page's plugin data.
    if (page->m_pluginData)
      page->m_pluginData = nullptr;
  }
}

PluginData* Page::pluginData(SecurityOrigin* mainFrameOrigin) const {
  if (!m_pluginData ||
      !mainFrameOrigin->isSameSchemeHostPort(m_pluginData->origin()))
    m_pluginData = PluginData::create(mainFrameOrigin);
  return m_pluginData.get();
}

void Page::setValidationMessageClient(ValidationMessageClient* client) {
  m_validationMessageClient = client;
}

void Page::setSuspended(bool suspended) {
  if (suspended == m_suspended)
    return;

  m_suspended = suspended;
  for (Frame* frame = mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    LocalFrame* localFrame = toLocalFrame(frame);
    localFrame->loader().setDefersLoading(suspended);
    localFrame->frameScheduler()->setSuspended(suspended);
  }
}

void Page::setDefaultPageScaleLimits(float minScale, float maxScale) {
  PageScaleConstraints newDefaults =
      pageScaleConstraintsSet().defaultConstraints();
  newDefaults.minimumScale = minScale;
  newDefaults.maximumScale = maxScale;

  if (newDefaults == pageScaleConstraintsSet().defaultConstraints())
    return;

  pageScaleConstraintsSet().setDefaultConstraints(newDefaults);
  pageScaleConstraintsSet().computeFinalConstraints();
  pageScaleConstraintsSet().setNeedsReset(true);

  if (!mainFrame() || !mainFrame()->isLocalFrame())
    return;

  FrameView* rootView = deprecatedLocalMainFrame()->view();

  if (!rootView)
    return;

  rootView->setNeedsLayout();
}

void Page::setUserAgentPageScaleConstraints(
    const PageScaleConstraints& newConstraints) {
  if (newConstraints == pageScaleConstraintsSet().userAgentConstraints())
    return;

  pageScaleConstraintsSet().setUserAgentConstraints(newConstraints);

  if (!mainFrame() || !mainFrame()->isLocalFrame())
    return;

  FrameView* rootView = deprecatedLocalMainFrame()->view();

  if (!rootView)
    return;

  rootView->setNeedsLayout();
}

void Page::setPageScaleFactor(float scale) {
  visualViewport().setScale(scale);
}

float Page::pageScaleFactor() const {
  return visualViewport().scale();
}

void Page::setDeviceScaleFactorDeprecated(float scaleFactor) {
  if (m_deviceScaleFactor == scaleFactor)
    return;

  m_deviceScaleFactor = scaleFactor;

  if (mainFrame() && mainFrame()->isLocalFrame())
    deprecatedLocalMainFrame()->deviceScaleFactorChanged();
}

void Page::allVisitedStateChanged(bool invalidateVisitedLinkHashes) {
  for (const Page* page : ordinaryPages()) {
    for (Frame* frame = page->m_mainFrame; frame;
         frame = frame->tree().traverseNext()) {
      if (frame->isLocalFrame())
        toLocalFrame(frame)
            ->document()
            ->visitedLinkState()
            .invalidateStyleForAllLinks(invalidateVisitedLinkHashes);
    }
  }
}

void Page::visitedStateChanged(LinkHash linkHash) {
  for (const Page* page : ordinaryPages()) {
    for (Frame* frame = page->m_mainFrame; frame;
         frame = frame->tree().traverseNext()) {
      if (frame->isLocalFrame())
        toLocalFrame(frame)
            ->document()
            ->visitedLinkState()
            .invalidateStyleForLink(linkHash);
    }
  }
}

void Page::setVisibilityState(PageVisibilityState visibilityState,
                              bool isInitialState) {
  if (m_visibilityState == visibilityState)
    return;
  m_visibilityState = visibilityState;

  if (!isInitialState)
    notifyPageVisibilityChanged();

  if (!isInitialState && m_mainFrame)
    m_mainFrame->didChangeVisibilityState();
}

PageVisibilityState Page::visibilityState() const {
  return m_visibilityState;
}

bool Page::isPageVisible() const {
  return visibilityState() == PageVisibilityStateVisible;
}

bool Page::isCursorVisible() const {
  return m_isCursorVisible;
}

#if DCHECK_IS_ON()
void checkFrameCountConsistency(int expectedFrameCount, Frame* frame) {
  DCHECK_GE(expectedFrameCount, 0);

  int actualFrameCount = 0;
  for (; frame; frame = frame->tree().traverseNext())
    ++actualFrameCount;

  DCHECK_EQ(expectedFrameCount, actualFrameCount);
}
#endif

int Page::subframeCount() const {
#if DCHECK_IS_ON()
  checkFrameCountConsistency(m_subframeCount + 1, mainFrame());
#endif
  return m_subframeCount;
}

void Page::settingsChanged(SettingsDelegate::ChangeType changeType) {
  switch (changeType) {
    case SettingsDelegate::StyleChange:
      setNeedsRecalcStyleInAllFrames();
      break;
    case SettingsDelegate::ViewportDescriptionChange:
      if (mainFrame() && mainFrame()->isLocalFrame()) {
        deprecatedLocalMainFrame()->document()->updateViewportDescription();
        // The text autosizer has dependencies on the viewport.
        if (TextAutosizer* textAutosizer =
                deprecatedLocalMainFrame()->document()->textAutosizer())
          textAutosizer->updatePageInfoInAllFrames();
      }
      break;
    case SettingsDelegate::DNSPrefetchingChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
          toLocalFrame(frame)->document()->initDNSPrefetch();
      }
      break;
    case SettingsDelegate::ImageLoadingChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame()) {
          toLocalFrame(frame)->document()->fetcher()->setImagesEnabled(
              settings().getImagesEnabled());
          toLocalFrame(frame)->document()->fetcher()->setAutoLoadImages(
              settings().getLoadsImagesAutomatically());
        }
      }
      break;
    case SettingsDelegate::TextAutosizingChange:
      if (!mainFrame() || !mainFrame()->isLocalFrame())
        break;
      if (TextAutosizer* textAutosizer =
              deprecatedLocalMainFrame()->document()->textAutosizer())
        textAutosizer->updatePageInfoInAllFrames();
      break;
    case SettingsDelegate::FontFamilyChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
          toLocalFrame(frame)
              ->document()
              ->styleEngine()
              .updateGenericFontFamilySettings();
      }
      break;
    case SettingsDelegate::AcceleratedCompositingChange:
      updateAcceleratedCompositingSettings();
      break;
    case SettingsDelegate::MediaQueryChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame())
          toLocalFrame(frame)->document()->mediaQueryAffectingValueChanged();
      }
      break;
    case SettingsDelegate::AccessibilityStateChange:
      if (!mainFrame() || !mainFrame()->isLocalFrame())
        break;
      deprecatedLocalMainFrame()
          ->document()
          ->axObjectCacheOwner()
          .clearAXObjectCache();
      break;
    case SettingsDelegate::ViewportRuleChange: {
      if (!mainFrame() || !mainFrame()->isLocalFrame())
        break;
      if (Document* doc = toLocalFrame(mainFrame())->document())
        doc->styleEngine().viewportRulesChanged();
    } break;
    case SettingsDelegate::TextTrackKindUserPreferenceChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (frame->isLocalFrame()) {
          Document* doc = toLocalFrame(frame)->document();
          if (doc)
            HTMLMediaElement::setTextTrackKindUserPreferenceForAllMediaElements(
                doc);
        }
      }
      break;
    case SettingsDelegate::DOMWorldsChange: {
      if (!settings().getForceMainWorldInitialization())
        break;
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
          continue;
        LocalFrame* localFrame = toLocalFrame(frame);
        if (localFrame->loader()
                .stateMachine()
                ->committedFirstRealDocumentLoad()) {
          // Forcibly instantiate WindowProxy.
          localFrame->script().windowProxy(DOMWrapperWorld::mainWorld());
        }
      }
    } break;
    case SettingsDelegate::MediaControlsChange:
      for (Frame* frame = mainFrame(); frame;
           frame = frame->tree().traverseNext()) {
        if (!frame->isLocalFrame())
          continue;
        Document* doc = toLocalFrame(frame)->document();
        if (doc)
          HTMLMediaElement::onMediaControlsEnabledChange(doc);
      }
      break;
  }
}

void Page::updateAcceleratedCompositingSettings() {
  for (Frame* frame = mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (!frame->isLocalFrame())
      continue;
    if (FrameView* view = toLocalFrame(frame)->view())
      view->updateAcceleratedCompositingSettings();
  }
}

void Page::didCommitLoad(LocalFrame* frame) {
  if (m_mainFrame == frame) {
    KURL url;
    if (frame->document())
      url = frame->document()->url();

    // TODO(rbyers): Most of this doesn't appear to take into account that each
    // SVGImage gets it's own Page instance.
    consoleMessageStorage().clear();
    useCounter().didCommitLoad(url);
    deprecation().clearSuppression();
    visualViewport().sendUMAMetrics();

    // Need to reset visual viewport position here since before commit load we
    // would update the previous history item, Page::didCommitLoad is called
    // after a new history item is created in FrameLoader.
    // See crbug.com/642279
    visualViewport().setScrollOffset(ScrollOffset(), ProgrammaticScroll);
    m_hostsUsingFeatures.updateMeasurementsAndClear();
  }
}

void Page::acceptLanguagesChanged() {
  HeapVector<Member<LocalFrame>> frames;

  // Even though we don't fire an event from here, the LocalDOMWindow's will
  // fire an event so we keep the frames alive until we are done.
  for (Frame* frame = mainFrame(); frame;
       frame = frame->tree().traverseNext()) {
    if (frame->isLocalFrame())
      frames.push_back(toLocalFrame(frame));
  }

  for (unsigned i = 0; i < frames.size(); ++i)
    frames[i]->domWindow()->acceptLanguagesChanged();
}

DEFINE_TRACE(Page) {
  visitor->trace(m_animator);
  visitor->trace(m_autoscrollController);
  visitor->trace(m_chromeClient);
  visitor->trace(m_dragCaret);
  visitor->trace(m_dragController);
  visitor->trace(m_focusController);
  visitor->trace(m_contextMenuController);
  visitor->trace(m_pointerLockController);
  visitor->trace(m_scrollingCoordinator);
  visitor->trace(m_browserControls);
  visitor->trace(m_consoleMessageStorage);
  visitor->trace(m_eventHandlerRegistry);
  visitor->trace(m_globalRootScrollerController);
  visitor->trace(m_visualViewport);
  visitor->trace(m_overscrollController);
  visitor->trace(m_mainFrame);
  visitor->trace(m_validationMessageClient);
  visitor->trace(m_useCounter);
  visitor->trace(m_frameHost);
  Supplementable<Page>::trace(visitor);
  PageVisibilityNotifier::trace(visitor);
}

void Page::layerTreeViewInitialized(WebLayerTreeView& layerTreeView,
                                    FrameView* view) {
  if (scrollingCoordinator())
    scrollingCoordinator()->layerTreeViewInitialized(layerTreeView, view);
}

void Page::willCloseLayerTreeView(WebLayerTreeView& layerTreeView,
                                  FrameView* view) {
  if (m_scrollingCoordinator)
    m_scrollingCoordinator->willCloseLayerTreeView(layerTreeView, view);
}

void Page::willBeDestroyed() {
  Frame* mainFrame = m_mainFrame;

  // TODO(sashab): Remove this check, the call to detach() here should always
  // work.
  if (mainFrame->isAttached())
    mainFrame->detach(FrameDetachType::Remove);

  ASSERT(allPages().contains(this));
  allPages().erase(this);
  ordinaryPages().erase(this);

  if (m_scrollingCoordinator)
    m_scrollingCoordinator->willBeDestroyed();

  chromeClient().chromeDestroyed();
  if (m_validationMessageClient)
    m_validationMessageClient->willBeDestroyed();
  m_mainFrame = nullptr;

  PageVisibilityNotifier::notifyContextDestroyed();
}

Page::PageClients::PageClients()
    : chromeClient(nullptr),
      contextMenuClient(nullptr),
      editorClient(nullptr),
      spellCheckerClient(nullptr) {}

Page::PageClients::~PageClients() {}

template class CORE_TEMPLATE_EXPORT Supplement<Page>;

}  // namespace blink
