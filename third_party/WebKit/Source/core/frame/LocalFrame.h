/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
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

#ifndef LocalFrame_h
#define LocalFrame_h

#include <memory>
#include "core/CoreExport.h"
#include "core/dom/WeakIdentifierMap.h"
#include "core/frame/Frame.h"
#include "core/frame/LocalFrameView.h"
#include "core/loader/FrameLoader.h"
#include "core/page/FrameTree.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"

namespace service_manager {
class InterfaceProvider;
}

namespace blink {

class Color;
class ContentSettingsClient;
class Document;
class Editor;
template <typename Traversal>
class EditingAlgorithm;
class Element;
template <typename Strategy>
class EphemeralRangeTemplate;
class EventHandler;
class FetchParameters;
class FloatSize;
class FrameConsole;
class FrameResourceCoordinator;
class FrameSelection;
class InputMethodController;
class CoreProbeSink;
class InterfaceRegistry;
class IntPoint;
class IntSize;
class LayoutView;
class LayoutViewItem;
class LocalDOMWindow;
class LocalWindowProxy;
class LocalFrameClient;
class NavigationScheduler;
class Node;
class NodeTraversal;
class PerformanceMonitor;
template <typename Strategy>
class PositionWithAffinityTemplate;
class PluginData;
class ResourceRequest;
class ScriptController;
class SpellChecker;
class TextSuggestionController;
class WebFrameScheduler;
class WebPluginContainerImpl;
class WebTaskRunner;
class WebURLLoader;

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<LocalFrame>;

class CORE_EXPORT LocalFrame final : public Frame,
                                     public Supplementable<LocalFrame> {
  USING_GARBAGE_COLLECTED_MIXIN(LocalFrame);

 public:
  static LocalFrame* Create(LocalFrameClient*,
                            Page&,
                            FrameOwner*,
                            InterfaceRegistry* = nullptr);

  void Init();
  void SetView(LocalFrameView*);
  void CreateView(const IntSize&,
                  const Color&,
                  ScrollbarMode = kScrollbarAuto,
                  bool horizontal_lock = false,
                  ScrollbarMode = kScrollbarAuto,
                  bool vertical_lock = false);

  // Frame overrides:
  ~LocalFrame() override;
  DECLARE_VIRTUAL_TRACE();
  void Navigate(Document& origin_document,
                const KURL&,
                bool replace_current_item,
                UserGestureStatus) override;
  void Navigate(const FrameLoadRequest&) override;
  void Reload(FrameLoadType, ClientRedirectPolicy) override;
  void Detach(FrameDetachType) override;
  bool ShouldClose() override;
  SecurityContext* GetSecurityContext() const override;
  void PrintNavigationErrorMessage(const Frame&, const char* reason) override;
  void PrintNavigationWarning(const String&) override;
  bool PrepareForCommit() override;
  void DidChangeVisibilityState() override;
  // This sets the is_inert_ flag and also recurses through this frame's
  // subtree, updating the inert bit on all descendant frames.
  void SetIsInert(bool) override;

  void DetachChildren();
  void DocumentAttached();

  Frame* FindFrameForNavigation(const AtomicString& name,
                                LocalFrame& active_frame,
                                const KURL& destination_url);

  // Note: these two functions are not virtual but intentionally shadow the
  // corresponding method in the Frame base class to return the
  // LocalFrame-specific subclass.
  LocalWindowProxy* WindowProxy(DOMWrapperWorld&);
  LocalDOMWindow* DomWindow() const;
  void SetDOMWindow(LocalDOMWindow*);
  LocalFrameView* View() const override;
  Document* GetDocument() const;
  void SetPagePopupOwner(Element&);
  Element* PagePopupOwner() const { return page_popup_owner_.Get(); }

  // Root of the layout tree for the document contained in this frame.
  LayoutView* ContentLayoutObject() const;
  LayoutViewItem ContentLayoutItem() const;

  Editor& GetEditor() const;
  EventHandler& GetEventHandler() const;
  FrameLoader& Loader() const;
  NavigationScheduler& GetNavigationScheduler() const;
  FrameSelection& Selection() const;
  InputMethodController& GetInputMethodController() const;
  TextSuggestionController& GetTextSuggestionController() const;
  ScriptController& GetScriptController() const;
  SpellChecker& GetSpellChecker() const;
  FrameConsole& Console() const;

  // This method is used to get the highest level LocalFrame in this
  // frame's in-process subtree.
  // FIXME: This is a temporary hack to support RemoteFrames, and callers
  // should be updated to avoid storing things on the main frame.
  LocalFrame& LocalFrameRoot() const;

  // Note that the result of this function should not be cached: a frame is
  // not necessarily detached when it is navigated, so the return value can
  // change.
  // In addition, this function will always return true for a detached frame.
  // TODO(dcheng): Move this to LocalDOMWindow and figure out the right
  // behavior for detached windows.
  bool IsCrossOriginSubframe() const;

  CoreProbeSink* GetProbeSink() { return probe_sink_.Get(); }

  // =========================================================================
  // All public functions below this point are candidates to move out of
  // LocalFrame into another class.

  // See GraphicsLayerClient.h for accepted flags.
  String GetLayerTreeAsTextForTesting(unsigned flags = 0) const;

  void SetPrinting(bool printing,
                   const FloatSize& page_size,
                   const FloatSize& original_page_size,
                   float maximum_shrink_ratio);
  bool ShouldUsePrintingLayout() const;
  FloatSize ResizePageRectsKeepingRatio(const FloatSize& original_size,
                                        const FloatSize& expected_size) const;

  bool InViewSourceMode() const;
  void SetInViewSourceMode(bool = true);

  void SetPageZoomFactor(float);
  float PageZoomFactor() const { return page_zoom_factor_; }
  void SetTextZoomFactor(float);
  float TextZoomFactor() const { return text_zoom_factor_; }
  void SetPageAndTextZoomFactors(float page_zoom_factor,
                                 float text_zoom_factor);

  void DeviceScaleFactorChanged();
  double DevicePixelRatio() const;

  String SelectedText() const;
  String SelectedTextForClipboard() const;

  PositionWithAffinityTemplate<EditingAlgorithm<NodeTraversal>>
  PositionForPoint(const IntPoint& frame_point);
  Document* DocumentAtPoint(const IntPoint&);
  EphemeralRangeTemplate<EditingAlgorithm<NodeTraversal>> RangeForPoint(
      const IntPoint& frame_point);

  bool ShouldReuseDefaultView(const KURL&) const;
  void RemoveSpellingMarkersUnderWords(const Vector<String>& words);

  bool ShouldThrottleRendering() const;

  // Returns the frame scheduler, creating one if needed.
  WebFrameScheduler* FrameScheduler();
  void ScheduleVisualUpdateUnlessThrottled();

  bool IsNavigationAllowed() const { return navigation_disable_count_ == 0; }

  // destination_url is only used when a navigation is blocked due to
  // framebusting defenses, in order to give the option of restarting the
  // navigation at a later time.
  bool CanNavigate(const Frame&, const KURL& destination_url = KURL());

  service_manager::InterfaceProvider& GetInterfaceProvider();
  InterfaceRegistry* GetInterfaceRegistry() { return interface_registry_; }

  LocalFrameClient* Client() const;

  ContentSettingsClient* GetContentSettingsClient();

  // GetFrameResourceCoordinator may return nullptr when it can not hook up to
  // services/resource_coordinator.
  FrameResourceCoordinator* GetFrameResourceCoordinator();

  PluginData* GetPluginData() const;

  PerformanceMonitor* GetPerformanceMonitor() { return performance_monitor_; }

  // Convenience function to allow loading image placeholders for the request if
  // either the flag in Settings() for using image placeholders is set, or if
  // the embedder decides that Client Lo-Fi should be used for this request.
  void MaybeAllowImagePlaceholder(FetchParameters&) const;

  std::unique_ptr<WebURLLoader> CreateURLLoader(const ResourceRequest&,
                                                WebTaskRunner*);

  bool IsInert() const { return is_inert_; }

  // If the frame hosts a PluginDocument, this method returns the
  // WebPluginContainerImpl that hosts the plugin. If the provided node is a
  // plugin, then it returns its WebPluginContainerImpl. Otherwise, uses the
  // currently focused element (if any).
  // TODO(slangley): Refactor this method to extract the logic of looking up
  // focused element or passed node into explicit methods.
  WebPluginContainerImpl* GetWebPluginContainer(Node* = nullptr) const;

  // Called on a view for a LocalFrame with a RemoteFrame parent. This makes
  // viewport intersection available that accounts for remote ancestor frames
  // and their respective scroll positions, clips, etc.
  void SetViewportIntersectionFromParent(const IntRect&);
  IntRect RemoteViewportIntersection() { return remote_viewport_intersection_; }

 private:
  friend class FrameNavigationDisabler;

  LocalFrame(LocalFrameClient*,
             Page&,
             FrameOwner*,
             InterfaceRegistry*);

  // Intentionally private to prevent redundant checks when the type is
  // already LocalFrame.
  bool IsLocalFrame() const override { return true; }
  bool IsRemoteFrame() const override { return false; }

  void EnableNavigation() { --navigation_disable_count_; }
  void DisableNavigation() { ++navigation_disable_count_; }

  bool CanNavigateWithoutFramebusting(const Frame&, String& error_reason);

  void PropagateInertToChildFrames();

  std::unique_ptr<WebFrameScheduler> frame_scheduler_;

  mutable FrameLoader loader_;
  Member<NavigationScheduler> navigation_scheduler_;

  // Cleared by LocalFrame::detach(), so as to keep the observable lifespan
  // of LocalFrame::view().
  Member<LocalFrameView> view_;
  // Usually 0. Non-null if this is the top frame of PagePopup.
  Member<Element> page_popup_owner_;

  const Member<ScriptController> script_controller_;
  const Member<Editor> editor_;
  const Member<SpellChecker> spell_checker_;
  const Member<FrameSelection> selection_;
  const Member<EventHandler> event_handler_;
  const Member<FrameConsole> console_;
  const Member<InputMethodController> input_method_controller_;
  const Member<TextSuggestionController> text_suggestion_controller_;

  int navigation_disable_count_;

  float page_zoom_factor_;
  float text_zoom_factor_;

  bool in_view_source_mode_;

  Member<CoreProbeSink> probe_sink_;
  Member<PerformanceMonitor> performance_monitor_;

  InterfaceRegistry* const interface_registry_;

  IntRect remote_viewport_intersection_;
  Member<FrameResourceCoordinator> frame_resource_coordinator_;
};

inline FrameLoader& LocalFrame::Loader() const {
  return loader_;
}

inline NavigationScheduler& LocalFrame::GetNavigationScheduler() const {
  DCHECK(navigation_scheduler_);
  return *navigation_scheduler_.Get();
}

inline LocalFrameView* LocalFrame::View() const {
  return view_.Get();
}

inline ScriptController& LocalFrame::GetScriptController() const {
  return *script_controller_;
}

inline FrameSelection& LocalFrame::Selection() const {
  return *selection_;
}

inline Editor& LocalFrame::GetEditor() const {
  return *editor_;
}

inline SpellChecker& LocalFrame::GetSpellChecker() const {
  return *spell_checker_;
}

inline FrameConsole& LocalFrame::Console() const {
  return *console_;
}

inline InputMethodController& LocalFrame::GetInputMethodController() const {
  return *input_method_controller_;
}

inline TextSuggestionController& LocalFrame::GetTextSuggestionController()
    const {
  return *text_suggestion_controller_;
}

inline bool LocalFrame::InViewSourceMode() const {
  return in_view_source_mode_;
}

inline void LocalFrame::SetInViewSourceMode(bool mode) {
  in_view_source_mode_ = mode;
}

inline EventHandler& LocalFrame::GetEventHandler() const {
  DCHECK(event_handler_);
  return *event_handler_;
}

DEFINE_TYPE_CASTS(LocalFrame,
                  Frame,
                  localFrame,
                  localFrame->IsLocalFrame(),
                  localFrame.IsLocalFrame());

DECLARE_WEAK_IDENTIFIER_MAP(LocalFrame);

class FrameNavigationDisabler {
  WTF_MAKE_NONCOPYABLE(FrameNavigationDisabler);
  STACK_ALLOCATED();

 public:
  explicit FrameNavigationDisabler(LocalFrame&);
  ~FrameNavigationDisabler();

 private:
  Member<LocalFrame> frame_;
};

// A helper class for attributing cost inside a scope to a LocalFrame, with
// output written to the trace log. The class is irrelevant to the core logic
// of LocalFrame.  Sample usage:
//
// void foo(LocalFrame* frame)
// {
//     ScopedFrameBlamer frameBlamer(frame);
//     TRACE_EVENT0("blink", "foo");
//     // Do some real work...
// }
//
// In Trace Viewer, we can find the cost of slice |foo| attributed to |frame|.
// Design doc:
// https://docs.google.com/document/d/15BB-suCb9j-nFt55yCFJBJCGzLg2qUm3WaSOPb8APtI/edit?usp=sharing
//
// This class is used in performance-sensitive code (like V8 entry), so care
// should be taken to ensure that it has an efficient fast path (for the common
// case where we are not tracking this).
class ScopedFrameBlamer {
  WTF_MAKE_NONCOPYABLE(ScopedFrameBlamer);
  STACK_ALLOCATED();

 public:
  explicit ScopedFrameBlamer(LocalFrame*);
  ~ScopedFrameBlamer() {
    if (UNLIKELY(frame_))
      LeaveContext();
  }

 private:
  void LeaveContext();

  Member<LocalFrame> frame_;
};

}  // namespace blink

#endif  // LocalFrame_h
