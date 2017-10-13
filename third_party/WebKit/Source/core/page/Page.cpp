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
#include "core/css/StyleChangeReason.h"
#include "core/css/StyleEngine.h"
#include "core/css/resolver/ViewportStyleResolver.h"
#include "core/dom/VisitedLinkState.h"
#include "core/dom/events/Event.h"
#include "core/editing/DragCaret.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/frame/BrowserControls.h"
#include "core/frame/DOMTimer.h"
#include "core/frame/EventHandlerRegistry.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/PageScaleConstraints.h"
#include "core/frame/PageScaleConstraintsSet.h"
#include "core/frame/RemoteFrame.h"
#include "core/frame/RemoteFrameView.h"
#include "core/frame/Settings.h"
#include "core/frame/VisualViewport.h"
#include "core/geometry/DOMRectList.h"
#include "core/html/media/HTMLMediaElement.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/layout/TextAutosizer.h"
#include "core/page/AutoscrollController.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuController.h"
#include "core/page/DragController.h"
#include "core/page/FocusController.h"
#include "core/page/PluginsChangedObserver.h"
#include "core/page/PointerLockController.h"
#include "core/page/ScopedPageSuspender.h"
#include "core/page/ValidationMessageClient.h"
#include "core/page/scrolling/OverscrollController.h"
#include "core/page/scrolling/ScrollingCoordinator.h"
#include "core/page/scrolling/TopDocumentRootScrollerController.h"
#include "core/paint/PaintLayer.h"
#include "core/probe/CoreProbes.h"
#include "platform/WebFrameScheduler.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/loader/fetch/ResourceFetcher.h"
#include "platform/plugins/PluginData.h"
#include "platform/scroll/SmoothScrollSequencer.h"
#include "public/platform/Platform.h"
#include "public/web/WebKit.h"

namespace blink {

// Wrapper function defined in WebKit.h
void ResetPluginCache(bool reload_pages) {
  DCHECK(!reload_pages);
  Page::RefreshPlugins();
  Page::ResetPluginData();
}

// Set of all live pages; includes internal Page objects that are
// not observable from scripts.
static Page::PageSet& AllPages() {
  DEFINE_STATIC_LOCAL(Page::PageSet, pages, ());
  return pages;
}

Page::PageSet& Page::OrdinaryPages() {
  DEFINE_STATIC_LOCAL(Page::PageSet, pages, ());
  return pages;
}

float DeviceScaleFactorDeprecated(LocalFrame* frame) {
  if (!frame)
    return 1;
  Page* page = frame->GetPage();
  if (!page)
    return 1;
  return page->DeviceScaleFactorDeprecated();
}

Page* Page::CreateOrdinary(PageClients& page_clients) {
  Page* page = Create(page_clients);
  OrdinaryPages().insert(page);
  if (ScopedPageSuspender::IsActive())
    page->SetPaused(true);
  return page;
}

Page::Page(PageClients& page_clients)
    : SettingsDelegate(Settings::Create()),
      animator_(PageAnimator::Create(*this)),
      autoscroll_controller_(AutoscrollController::Create(*this)),
      chrome_client_(page_clients.chrome_client),
      drag_caret_(DragCaret::Create()),
      drag_controller_(DragController::Create(this)),
      focus_controller_(FocusController::Create(this)),
      context_menu_controller_(
          ContextMenuController::Create(this,
                                        page_clients.context_menu_client)),
      page_scale_constraints_set_(PageScaleConstraintsSet::Create()),
      pointer_lock_controller_(PointerLockController::Create(this)),
      browser_controls_(BrowserControls::Create(*this)),
      console_message_storage_(new ConsoleMessageStorage()),
      event_handler_registry_(new EventHandlerRegistry(*this)),
      global_root_scroller_controller_(
          TopDocumentRootScrollerController::Create(*this)),
      visual_viewport_(VisualViewport::Create(*this)),
      overscroll_controller_(
          OverscrollController::Create(GetVisualViewport(), GetChromeClient())),
      main_frame_(nullptr),
      plugin_data_(nullptr),
      editor_client_(page_clients.editor_client),
      spell_checker_client_(page_clients.spell_checker_client),
      use_counter_(page_clients.chrome_client &&
                           page_clients.chrome_client->IsSVGImageChromeClient()
                       ? UseCounter::kSVGImageContext
                       : UseCounter::kDefaultContext),
      opened_by_dom_(false),
      tab_key_cycles_through_elements_(true),
      paused_(false),
      device_scale_factor_(1),
      visibility_state_(kPageVisibilityStateVisible),
      is_cursor_visible_(true),
      subframe_count_(0) {
  DCHECK(editor_client_);

  DCHECK(!AllPages().Contains(this));
  AllPages().insert(this);
}

Page::~Page() {
  // willBeDestroyed() must be called before Page destruction.
  DCHECK(!main_frame_);
}

void Page::CloseSoon() {
  // Make sure this Page can no longer be found by JS.
  is_closing_ = true;

  // TODO(dcheng): Try to remove this in a followup, it's not obviously needed.
  if (main_frame_->IsLocalFrame())
    ToLocalFrame(main_frame_)->Loader().StopAllLoaders();

  GetChromeClient().CloseWindowSoon();
}

ViewportDescription Page::GetViewportDescription() const {
  return MainFrame() && MainFrame()->IsLocalFrame() &&
                 DeprecatedLocalMainFrame()->GetDocument()
             ? DeprecatedLocalMainFrame()
                   ->GetDocument()
                   ->GetViewportDescription()
             : ViewportDescription();
}

ScrollingCoordinator* Page::GetScrollingCoordinator() {
  if (!scrolling_coordinator_ && settings_->GetAcceleratedCompositingEnabled())
    scrolling_coordinator_ = ScrollingCoordinator::Create(this);

  return scrolling_coordinator_.Get();
}

PageScaleConstraintsSet& Page::GetPageScaleConstraintsSet() {
  return *page_scale_constraints_set_;
}

SmoothScrollSequencer* Page::GetSmoothScrollSequencer() {
  if (!smooth_scroll_sequencer_)
    smooth_scroll_sequencer_ = new SmoothScrollSequencer();

  return smooth_scroll_sequencer_.Get();
}

const PageScaleConstraintsSet& Page::GetPageScaleConstraintsSet() const {
  return *page_scale_constraints_set_;
}

BrowserControls& Page::GetBrowserControls() {
  return *browser_controls_;
}

const BrowserControls& Page::GetBrowserControls() const {
  return *browser_controls_;
}

ConsoleMessageStorage& Page::GetConsoleMessageStorage() {
  return *console_message_storage_;
}

const ConsoleMessageStorage& Page::GetConsoleMessageStorage() const {
  return *console_message_storage_;
}

EventHandlerRegistry& Page::GetEventHandlerRegistry() {
  return *event_handler_registry_;
}

const EventHandlerRegistry& Page::GetEventHandlerRegistry() const {
  return *event_handler_registry_;
}

TopDocumentRootScrollerController& Page::GlobalRootScrollerController() const {
  return *global_root_scroller_controller_;
}

VisualViewport& Page::GetVisualViewport() {
  return *visual_viewport_;
}

const VisualViewport& Page::GetVisualViewport() const {
  return *visual_viewport_;
}

OverscrollController& Page::GetOverscrollController() {
  return *overscroll_controller_;
}

const OverscrollController& Page::GetOverscrollController() const {
  return *overscroll_controller_;
}

DOMRectList* Page::NonFastScrollableRects(const LocalFrame* frame) {
  DisableCompositingQueryAsserts disabler;
  if (ScrollingCoordinator* scrolling_coordinator =
          this->GetScrollingCoordinator()) {
    // Hits in compositing/iframes/iframe-composited-scrolling.html
    scrolling_coordinator->UpdateAfterCompositingChangeIfNeeded();
  }

  GraphicsLayer* layer =
      frame->View()->LayoutViewportScrollableArea()->LayerForScrolling();
  if (!layer)
    return DOMRectList::Create();
  return DOMRectList::Create(layer->PlatformLayer()->NonFastScrollableRegion());
}

void Page::SetMainFrame(Frame* main_frame) {
  // Should only be called during initialization or swaps between local and
  // remote frames.
  // FIXME: Unfortunately we can't assert on this at the moment, because this
  // is called in the base constructor for both LocalFrame and RemoteFrame,
  // when the vtables for the derived classes have not yet been setup.
  main_frame_ = main_frame;
}

void Page::DocumentDetached(Document* document) {
  pointer_lock_controller_->DocumentDetached(document);
  context_menu_controller_->DocumentDetached(document);
  if (validation_message_client_)
    validation_message_client_->DocumentDetached(*document);
  hosts_using_features_.DocumentDetached(*document);
}

bool Page::OpenedByDOM() const {
  return opened_by_dom_;
}

void Page::SetOpenedByDOM() {
  opened_by_dom_ = true;
}

void Page::PlatformColorsChanged() {
  for (const Page* page : AllPages())
    for (Frame* frame = page->MainFrame(); frame;
         frame = frame->Tree().TraverseNext()) {
      if (frame->IsLocalFrame())
        ToLocalFrame(frame)->GetDocument()->PlatformColorsChanged();
    }
}

void Page::SetNeedsRecalcStyleInAllFrames() {
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (frame->IsLocalFrame())
      ToLocalFrame(frame)->GetDocument()->SetNeedsStyleRecalc(
          kSubtreeStyleChange,
          StyleChangeReasonForTracing::Create(StyleChangeReason::kSettings));
  }
}

void Page::RefreshPlugins() {
  PluginData::RefreshBrowserSidePluginCache();
}

PluginData* Page::GetPluginData(SecurityOrigin* main_frame_origin) {
  if (!plugin_data_)
    plugin_data_ = PluginData::Create();

  if (!plugin_data_->Origin() ||
      !main_frame_origin->IsSameSchemeHostPort(plugin_data_->Origin()))
    plugin_data_->UpdatePluginList(main_frame_origin);

  return plugin_data_.Get();
}

void Page::ResetPluginData() {
  for (Page* page : AllPages()) {
    if (page->plugin_data_) {
      page->plugin_data_->ResetPluginData();
      page->NotifyPluginsChanged();
    }
  }
}

void Page::SetValidationMessageClient(ValidationMessageClient* client) {
  validation_message_client_ = client;
}

void Page::SetPaused(bool paused) {
  if (paused == paused_)
    return;

  paused_ = paused;
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    LocalFrame* local_frame = ToLocalFrame(frame);
    local_frame->Loader().SetDefersLoading(paused);
    local_frame->FrameScheduler()->SetPaused(paused);
  }
}

void Page::SetDefaultPageScaleLimits(float min_scale, float max_scale) {
  PageScaleConstraints new_defaults =
      GetPageScaleConstraintsSet().DefaultConstraints();
  new_defaults.minimum_scale = min_scale;
  new_defaults.maximum_scale = max_scale;

  if (new_defaults == GetPageScaleConstraintsSet().DefaultConstraints())
    return;

  GetPageScaleConstraintsSet().SetDefaultConstraints(new_defaults);
  GetPageScaleConstraintsSet().ComputeFinalConstraints();
  GetPageScaleConstraintsSet().SetNeedsReset(true);

  if (!MainFrame() || !MainFrame()->IsLocalFrame())
    return;

  LocalFrameView* root_view = DeprecatedLocalMainFrame()->View();

  if (!root_view)
    return;

  root_view->SetNeedsLayout();
}

void Page::SetUserAgentPageScaleConstraints(
    const PageScaleConstraints& new_constraints) {
  if (new_constraints == GetPageScaleConstraintsSet().UserAgentConstraints())
    return;

  GetPageScaleConstraintsSet().SetUserAgentConstraints(new_constraints);

  if (!MainFrame() || !MainFrame()->IsLocalFrame())
    return;

  LocalFrameView* root_view = DeprecatedLocalMainFrame()->View();

  if (!root_view)
    return;

  root_view->SetNeedsLayout();
}

void Page::SetPageScaleFactor(float scale) {
  GetVisualViewport().SetScale(scale);
}

float Page::PageScaleFactor() const {
  return GetVisualViewport().Scale();
}

void Page::SetDeviceScaleFactorDeprecated(float scale_factor) {
  if (device_scale_factor_ == scale_factor)
    return;

  device_scale_factor_ = scale_factor;

  if (MainFrame() && MainFrame()->IsLocalFrame())
    DeprecatedLocalMainFrame()->DeviceScaleFactorChanged();
}

void Page::AllVisitedStateChanged(bool invalidate_visited_link_hashes) {
  for (const Page* page : OrdinaryPages()) {
    for (Frame* frame = page->main_frame_; frame;
         frame = frame->Tree().TraverseNext()) {
      if (frame->IsLocalFrame())
        ToLocalFrame(frame)
            ->GetDocument()
            ->GetVisitedLinkState()
            .InvalidateStyleForAllLinks(invalidate_visited_link_hashes);
    }
  }
}

void Page::VisitedStateChanged(LinkHash link_hash) {
  for (const Page* page : OrdinaryPages()) {
    for (Frame* frame = page->main_frame_; frame;
         frame = frame->Tree().TraverseNext()) {
      if (frame->IsLocalFrame())
        ToLocalFrame(frame)
            ->GetDocument()
            ->GetVisitedLinkState()
            .InvalidateStyleForLink(link_hash);
    }
  }
}

void Page::SetVisibilityState(PageVisibilityState visibility_state,
                              bool is_initial_state) {
  if (visibility_state_ == visibility_state)
    return;
  visibility_state_ = visibility_state;

  if (!is_initial_state)
    NotifyPageVisibilityChanged();

  if (!is_initial_state && main_frame_)
    main_frame_->DidChangeVisibilityState();
}

PageVisibilityState Page::VisibilityState() const {
  return visibility_state_;
}

bool Page::IsPageVisible() const {
  return VisibilityState() == kPageVisibilityStateVisible;
}

bool Page::IsCursorVisible() const {
  return is_cursor_visible_;
}

#if DCHECK_IS_ON()
void CheckFrameCountConsistency(int expected_frame_count, Frame* frame) {
  DCHECK_GE(expected_frame_count, 0);

  int actual_frame_count = 0;
  for (; frame; frame = frame->Tree().TraverseNext())
    ++actual_frame_count;

  DCHECK_EQ(expected_frame_count, actual_frame_count);
}
#endif

int Page::SubframeCount() const {
#if DCHECK_IS_ON()
  CheckFrameCountConsistency(subframe_count_ + 1, MainFrame());
#endif
  return subframe_count_;
}

void Page::SettingsChanged(SettingsDelegate::ChangeType change_type) {
  switch (change_type) {
    case SettingsDelegate::kStyleChange:
      SetNeedsRecalcStyleInAllFrames();
      break;
    case SettingsDelegate::kViewportDescriptionChange:
      if (MainFrame() && MainFrame()->IsLocalFrame()) {
        DeprecatedLocalMainFrame()->GetDocument()->UpdateViewportDescription();
        // The text autosizer has dependencies on the viewport.
        if (TextAutosizer* text_autosizer =
                DeprecatedLocalMainFrame()->GetDocument()->GetTextAutosizer())
          text_autosizer->UpdatePageInfoInAllFrames();
      }
      break;
    case SettingsDelegate::kDNSPrefetchingChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (frame->IsLocalFrame())
          ToLocalFrame(frame)->GetDocument()->InitDNSPrefetch();
      }
      break;
    case SettingsDelegate::kImageLoadingChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (frame->IsLocalFrame()) {
          ToLocalFrame(frame)->GetDocument()->Fetcher()->SetImagesEnabled(
              GetSettings().GetImagesEnabled());
          ToLocalFrame(frame)->GetDocument()->Fetcher()->SetAutoLoadImages(
              GetSettings().GetLoadsImagesAutomatically());
        }
      }
      break;
    case SettingsDelegate::kTextAutosizingChange:
      if (!MainFrame() || !MainFrame()->IsLocalFrame())
        break;
      if (TextAutosizer* text_autosizer =
              DeprecatedLocalMainFrame()->GetDocument()->GetTextAutosizer())
        text_autosizer->UpdatePageInfoInAllFrames();
      break;
    case SettingsDelegate::kFontFamilyChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (frame->IsLocalFrame())
          ToLocalFrame(frame)
              ->GetDocument()
              ->GetStyleEngine()
              .UpdateGenericFontFamilySettings();
      }
      break;
    case SettingsDelegate::kAcceleratedCompositingChange:
      UpdateAcceleratedCompositingSettings();
      break;
    case SettingsDelegate::kMediaQueryChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (frame->IsLocalFrame())
          ToLocalFrame(frame)->GetDocument()->MediaQueryAffectingValueChanged();
      }
      break;
    case SettingsDelegate::kAccessibilityStateChange:
      if (!MainFrame() || !MainFrame()->IsLocalFrame())
        break;
      DeprecatedLocalMainFrame()
          ->GetDocument()
          ->AxObjectCacheOwner()
          .ClearAXObjectCache();
      break;
    case SettingsDelegate::kViewportRuleChange: {
      if (!MainFrame() || !MainFrame()->IsLocalFrame())
        break;
      if (Document* doc = ToLocalFrame(MainFrame())->GetDocument())
        doc->GetStyleEngine().ViewportRulesChanged();
      break;
    }
    case SettingsDelegate::kTextTrackKindUserPreferenceChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (frame->IsLocalFrame()) {
          Document* doc = ToLocalFrame(frame)->GetDocument();
          if (doc)
            HTMLMediaElement::SetTextTrackKindUserPreferenceForAllMediaElements(
                doc);
        }
      }
      break;
    case SettingsDelegate::kDOMWorldsChange: {
      if (!GetSettings().GetForceMainWorldInitialization())
        break;
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (!frame->IsLocalFrame())
          continue;
        LocalFrame* local_frame = ToLocalFrame(frame);
        if (local_frame->Loader()
                .StateMachine()
                ->CommittedFirstRealDocumentLoad()) {
          // Forcibly instantiate WindowProxy.
          local_frame->GetScriptController().WindowProxy(
              DOMWrapperWorld::MainWorld());
        }
      }
      break;
    }
    case SettingsDelegate::kMediaControlsChange:
      for (Frame* frame = MainFrame(); frame;
           frame = frame->Tree().TraverseNext()) {
        if (!frame->IsLocalFrame())
          continue;
        Document* doc = ToLocalFrame(frame)->GetDocument();
        if (doc)
          HTMLMediaElement::OnMediaControlsEnabledChange(doc);
      }
      break;
    case SettingsDelegate::kPluginsChange: {
      NotifyPluginsChanged();
      break;
    }
  }
}

void Page::NotifyPluginsChanged() const {
  HeapVector<Member<PluginsChangedObserver>, 32> observers;
  CopyToVector(plugins_changed_observers_, observers);
  for (PluginsChangedObserver* observer : observers)
    observer->PluginsChanged();
}

void Page::UpdateAcceleratedCompositingSettings() {
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (!frame->IsLocalFrame())
      continue;
    if (LocalFrameView* view = ToLocalFrame(frame)->View())
      view->UpdateAcceleratedCompositingSettings();
  }
}

void Page::DidCommitLoad(LocalFrame* frame) {
  if (main_frame_ == frame) {
    KURL url;
    if (frame->GetDocument())
      url = frame->GetDocument()->Url();

    // TODO(rbyers): Most of this doesn't appear to take into account that each
    // SVGImage gets it's own Page instance.
    GetConsoleMessageStorage().Clear();
    GetUseCounter().DidCommitLoad(url);
    GetDeprecation().ClearSuppression();
    GetVisualViewport().SendUMAMetrics();

    // Need to reset visual viewport position here since before commit load we
    // would update the previous history item, Page::didCommitLoad is called
    // after a new history item is created in FrameLoader.
    // See crbug.com/642279
    GetVisualViewport().SetScrollOffset(ScrollOffset(), kProgrammaticScroll);
    hosts_using_features_.UpdateMeasurementsAndClear();
  }
}

void Page::AcceptLanguagesChanged() {
  HeapVector<Member<LocalFrame>> frames;

  // Even though we don't fire an event from here, the LocalDOMWindow's will
  // fire an event so we keep the frames alive until we are done.
  for (Frame* frame = MainFrame(); frame;
       frame = frame->Tree().TraverseNext()) {
    if (frame->IsLocalFrame())
      frames.push_back(ToLocalFrame(frame));
  }

  for (unsigned i = 0; i < frames.size(); ++i)
    frames[i]->DomWindow()->AcceptLanguagesChanged();
}

DEFINE_TRACE(Page) {
  visitor->Trace(animator_);
  visitor->Trace(autoscroll_controller_);
  visitor->Trace(chrome_client_);
  visitor->Trace(drag_caret_);
  visitor->Trace(drag_controller_);
  visitor->Trace(focus_controller_);
  visitor->Trace(context_menu_controller_);
  visitor->Trace(pointer_lock_controller_);
  visitor->Trace(scrolling_coordinator_);
  visitor->Trace(smooth_scroll_sequencer_);
  visitor->Trace(browser_controls_);
  visitor->Trace(console_message_storage_);
  visitor->Trace(event_handler_registry_);
  visitor->Trace(global_root_scroller_controller_);
  visitor->Trace(visual_viewport_);
  visitor->Trace(overscroll_controller_);
  visitor->Trace(main_frame_);
  visitor->Trace(plugin_data_);
  visitor->Trace(validation_message_client_);
  visitor->Trace(use_counter_);
  visitor->Trace(plugins_changed_observers_);
  Supplementable<Page>::Trace(visitor);
  PageVisibilityNotifier::Trace(visitor);
}

void Page::LayerTreeViewInitialized(WebLayerTreeView& layer_tree_view,
                                    LocalFrameView* view) {
  if (GetScrollingCoordinator())
    GetScrollingCoordinator()->LayerTreeViewInitialized(layer_tree_view, view);
}

void Page::WillCloseLayerTreeView(WebLayerTreeView& layer_tree_view,
                                  LocalFrameView* view) {
  if (scrolling_coordinator_)
    scrolling_coordinator_->WillCloseLayerTreeView(layer_tree_view, view);
}

void Page::WillBeDestroyed() {
  Frame* main_frame = main_frame_;

  // TODO(sashab): Remove this check, the call to detach() here should always
  // work.
  if (main_frame->IsAttached())
    main_frame->Detach(FrameDetachType::kRemove);

  DCHECK(AllPages().Contains(this));
  AllPages().erase(this);
  OrdinaryPages().erase(this);

  if (scrolling_coordinator_)
    scrolling_coordinator_->WillBeDestroyed();

  GetChromeClient().ChromeDestroyed();
  if (validation_message_client_)
    validation_message_client_->WillBeDestroyed();
  main_frame_ = nullptr;

  PageVisibilityNotifier::NotifyContextDestroyed();
}

void Page::RegisterPluginsChangedObserver(PluginsChangedObserver* observer) {
  plugins_changed_observers_.insert(observer);
}

Page::PageClients::PageClients()
    : chrome_client(nullptr),
      context_menu_client(nullptr),
      editor_client(nullptr),
      spell_checker_client(nullptr) {}

Page::PageClients::~PageClients() {}

template class CORE_TEMPLATE_EXPORT Supplement<Page>;

}  // namespace blink
