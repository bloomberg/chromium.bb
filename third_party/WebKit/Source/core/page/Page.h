/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2013 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#ifndef Page_h
#define Page_h

#include "core/CoreExport.h"
#include "core/dom/ViewportDescription.h"
#include "core/frame/Deprecation.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/SettingsDelegate.h"
#include "core/frame/UseCounter.h"
#include "core/page/Page.h"
#include "core/page/PageAnimator.h"
#include "core/page/PageVisibilityNotifier.h"
#include "core/page/PageVisibilityObserver.h"
#include "core/page/PageVisibilityState.h"
#include "platform/Supplementable.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/Region.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"
#include "public/web/WebWindowFeatures.h"

namespace blink {

class AutoscrollController;
class BrowserControls;
class ChromeClient;
class ContextMenuClient;
class ContextMenuController;
class Document;
class DOMRectList;
class DragCaret;
class DragController;
class EditorClient;
class EventHandlerRegistry;
class FocusController;
class Frame;
class OverscrollController;
struct PageScaleConstraints;
class PageScaleConstraintsSet;
class PluginData;
class PluginsChangedObserver;
class PointerLockController;
class ScopedPageSuspender;
class ScrollingCoordinator;
class SmoothScrollSequencer;
class Settings;
class ConsoleMessageStorage;
class SpellCheckerClient;
class TopDocumentRootScrollerController;
class ValidationMessageClient;
class VisualViewport;
class WebLayerTreeView;

typedef uint64_t LinkHash;

float DeviceScaleFactorDeprecated(LocalFrame*);

class CORE_EXPORT Page final : public GarbageCollectedFinalized<Page>,
                               public Supplementable<Page>,
                               public PageVisibilityNotifier,
                               public SettingsDelegate {
  USING_GARBAGE_COLLECTED_MIXIN(Page);
  WTF_MAKE_NONCOPYABLE(Page);
  friend class Settings;

 public:
  // It is up to the platform to ensure that non-null clients are provided where
  // required.
  struct CORE_EXPORT PageClients final {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(PageClients);

   public:
    PageClients();
    ~PageClients();

    Member<ChromeClient> chrome_client;
    ContextMenuClient* context_menu_client;
    EditorClient* editor_client;
    SpellCheckerClient* spell_checker_client;
  };

  static Page* Create(PageClients& page_clients) {
    return new Page(page_clients);
  }

  // An "ordinary" page is a fully-featured page owned by a web view.
  static Page* CreateOrdinary(PageClients&);

  ~Page() override;

  void CloseSoon();
  bool IsClosing() const { return is_closing_; }

  using PageSet = PersistentHeapHashSet<WeakMember<Page>>;

  // Return the current set of full-fledged, ordinary pages.
  // Each created and owned by a WebView.
  //
  // This set does not include Pages created for other, internal purposes
  // (SVGImages, inspector overlays, page popups etc.)
  static PageSet& OrdinaryPages();

  static void PlatformColorsChanged();

  void SetNeedsRecalcStyleInAllFrames();
  void UpdateAcceleratedCompositingSettings();

  ViewportDescription GetViewportDescription() const;

  // Returns the plugin data associated with |main_frame_origin|.
  PluginData* GetPluginData(SecurityOrigin* main_frame_origin);

  // Refreshes the browser-side plugin cache.
  static void RefreshPlugins();

  // Resets the plugin data for all pages in the renderer process and notifies
  // PluginsChangedObservers.
  static void ResetPluginData();

  EditorClient& GetEditorClient() const { return *editor_client_; }
  SpellCheckerClient& GetSpellCheckerClient() const {
    return *spell_checker_client_;
  }

  void SetMainFrame(Frame*);
  Frame* MainFrame() const { return main_frame_; }
  // Escape hatch for existing code that assumes that the root frame is
  // always a LocalFrame. With OOPI, this is not always the case. Code that
  // depends on this will generally have to be rewritten to propagate any
  // necessary state through all renderer processes for that page and/or
  // coordinate/rely on the browser process to help dispatch/coordinate work.
  LocalFrame* DeprecatedLocalMainFrame() const {
    return ToLocalFrame(main_frame_);
  }

  void DocumentDetached(Document*);

  bool OpenedByDOM() const;
  void SetOpenedByDOM();

  PageAnimator& Animator() { return *animator_; }
  ChromeClient& GetChromeClient() const { return *chrome_client_; }
  AutoscrollController& GetAutoscrollController() const {
    return *autoscroll_controller_;
  }
  DragCaret& GetDragCaret() const { return *drag_caret_; }
  DragController& GetDragController() const { return *drag_controller_; }
  FocusController& GetFocusController() const { return *focus_controller_; }
  ContextMenuController& GetContextMenuController() const {
    return *context_menu_controller_;
  }
  PointerLockController& GetPointerLockController() const {
    return *pointer_lock_controller_;
  }
  ValidationMessageClient& GetValidationMessageClient() const {
    return *validation_message_client_;
  }
  void SetValidationMessageClient(ValidationMessageClient*);

  ScrollingCoordinator* GetScrollingCoordinator();

  SmoothScrollSequencer* GetSmoothScrollSequencer();

  DOMRectList* NonFastScrollableRects(const LocalFrame*);

  Settings& GetSettings() const { return *settings_; }

  UseCounter& GetUseCounter() { return use_counter_; }
  Deprecation& GetDeprecation() { return deprecation_; }
  HostsUsingFeatures& GetHostsUsingFeatures() { return hosts_using_features_; }

  void SetWindowFeatures(const WebWindowFeatures& features) {
    window_features_ = features;
  }
  const WebWindowFeatures& GetWindowFeatures() const {
    return window_features_;
  }

  PageScaleConstraintsSet& GetPageScaleConstraintsSet();
  const PageScaleConstraintsSet& GetPageScaleConstraintsSet() const;

  BrowserControls& GetBrowserControls();
  const BrowserControls& GetBrowserControls() const;

  ConsoleMessageStorage& GetConsoleMessageStorage();
  const ConsoleMessageStorage& GetConsoleMessageStorage() const;

  EventHandlerRegistry& GetEventHandlerRegistry();
  const EventHandlerRegistry& GetEventHandlerRegistry() const;

  TopDocumentRootScrollerController& GlobalRootScrollerController() const;

  VisualViewport& GetVisualViewport();
  const VisualViewport& GetVisualViewport() const;

  OverscrollController& GetOverscrollController();
  const OverscrollController& GetOverscrollController() const;

  void SetTabKeyCyclesThroughElements(bool b) {
    tab_key_cycles_through_elements_ = b;
  }
  bool TabKeyCyclesThroughElements() const {
    return tab_key_cycles_through_elements_;
  }

  // Suspension is used to implement the "Optionally, pause while waiting for
  // the user to acknowledge the message" step of simple dialog processing:
  // https://html.spec.whatwg.org/multipage/webappapis.html#simple-dialogs
  //
  // Per https://html.spec.whatwg.org/multipage/webappapis.html#pause, no loads
  // are allowed to start/continue in this state, and all background processing
  // is also suspended.
  bool Suspended() const { return suspended_; }

  void SetPageScaleFactor(float);
  float PageScaleFactor() const;

  // Corresponds to pixel density of the device where this Page is
  // being displayed. In multi-monitor setups this can vary between pages.
  // This value does not account for Page zoom, use LocalFrame::devicePixelRatio
  // instead.  This is to be deprecated. Use this with caution.
  // 1) If you need to scale the content per device scale factor, this is still
  //    valid.  In use-zoom-for-dsf mode, this is always 1, and will be remove
  //    when transition is complete.
  // 2) If you want to compute the device related measure (such as device pixel
  //    height, or the scale factor for drag image), use
  //    ChromeClient::screenInfo() instead.
  float DeviceScaleFactorDeprecated() const { return device_scale_factor_; }
  void SetDeviceScaleFactorDeprecated(float);

  static void AllVisitedStateChanged(bool invalidate_visited_link_hashes);
  static void VisitedStateChanged(LinkHash visited_hash);

  void SetVisibilityState(PageVisibilityState, bool);
  PageVisibilityState VisibilityState() const;
  bool IsPageVisible() const;

  bool IsCursorVisible() const;
  void SetIsCursorVisible(bool is_visible) { is_cursor_visible_ = is_visible; }

  // Don't allow more than a certain number of frames in a page.
  // This seems like a reasonable upper bound, and otherwise mutually
  // recursive frameset pages can quickly bring the program to its knees
  // with exponential growth in the number of frames.
  static const int kMaxNumberOfFrames = 1000;
  void IncrementSubframeCount() { ++subframe_count_; }
  void DecrementSubframeCount() {
    DCHECK_GT(subframe_count_, 0);
    --subframe_count_;
  }
  int SubframeCount() const;

  void SetDefaultPageScaleLimits(float min_scale, float max_scale);
  void SetUserAgentPageScaleConstraints(
      const PageScaleConstraints& new_constraints);

#if DCHECK_IS_ON()
  void SetIsPainting(bool painting) { is_painting_ = painting; }
  bool IsPainting() const { return is_painting_; }
#endif

  void DidCommitLoad(LocalFrame*);

  void AcceptLanguagesChanged();

  DECLARE_TRACE();

  void LayerTreeViewInitialized(WebLayerTreeView&, LocalFrameView*);
  void WillCloseLayerTreeView(WebLayerTreeView&, LocalFrameView*);

  void WillBeDestroyed();

  void RegisterPluginsChangedObserver(PluginsChangedObserver*);

 private:
  friend class ScopedPageSuspender;

  explicit Page(PageClients&);

  void InitGroup();

  // SettingsDelegate overrides.
  void SettingsChanged(SettingsDelegate::ChangeType) override;

  // ScopedPageSuspender helpers.
  void SetSuspended(bool);

  // Notify |plugins_changed_observers_| that plugins have changed.
  void NotifyPluginsChanged() const;

  Member<PageAnimator> animator_;
  const Member<AutoscrollController> autoscroll_controller_;
  Member<ChromeClient> chrome_client_;
  const Member<DragCaret> drag_caret_;
  const Member<DragController> drag_controller_;
  const Member<FocusController> focus_controller_;
  const Member<ContextMenuController> context_menu_controller_;
  const std::unique_ptr<PageScaleConstraintsSet> page_scale_constraints_set_;
  const Member<PointerLockController> pointer_lock_controller_;
  Member<ScrollingCoordinator> scrolling_coordinator_;
  Member<SmoothScrollSequencer> smooth_scroll_sequencer_;
  const Member<BrowserControls> browser_controls_;
  const Member<ConsoleMessageStorage> console_message_storage_;
  const Member<EventHandlerRegistry> event_handler_registry_;
  const Member<TopDocumentRootScrollerController>
      global_root_scroller_controller_;
  const Member<VisualViewport> visual_viewport_;
  const Member<OverscrollController> overscroll_controller_;

  // Typically, the main frame and Page should both be owned by the embedder,
  // which must call Page::willBeDestroyed() prior to destroying Page. This
  // call detaches the main frame and clears this pointer, thus ensuring that
  // this field only references a live main frame.
  //
  // However, there are several locations (InspectorOverlay, SVGImage, and
  // WebPagePopupImpl) which don't hold a reference to the main frame at all
  // after creating it. These are still safe because they always create a
  // Frame with a LocalFrameView. LocalFrameView and Frame hold references to
  // each other, thus keeping each other alive. The call to willBeDestroyed()
  // breaks this cycle, so the frame is still properly destroyed once no
  // longer needed.
  Member<Frame> main_frame_;

  Member<PluginData> plugin_data_;

  EditorClient* const editor_client_;
  SpellCheckerClient* const spell_checker_client_;
  Member<ValidationMessageClient> validation_message_client_;

  UseCounter use_counter_;
  Deprecation deprecation_;
  HostsUsingFeatures hosts_using_features_;
  WebWindowFeatures window_features_;

  bool opened_by_dom_;
  // Set to true when window.close() has been called and the Page will be
  // destroyed. The browsing contexts in this page should no longer be
  // discoverable via JS.
  // TODO(dcheng): Try to remove |DOMWindow::m_windowIsClosing| in favor of
  // this. However, this depends on resolving https://crbug.com/674641
  bool is_closing_;

  bool tab_key_cycles_through_elements_;
  bool suspended_;

  float device_scale_factor_;

  PageVisibilityState visibility_state_;

  bool is_cursor_visible_;

#if DCHECK_IS_ON()
  bool is_painting_ = false;
#endif

  int subframe_count_;

  HeapHashSet<WeakMember<PluginsChangedObserver>> plugins_changed_observers_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<Page>;

}  // namespace blink

#endif  // Page_h
