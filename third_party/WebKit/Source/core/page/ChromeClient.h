/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple, Inc. All rights
 * reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef ChromeClient_h
#define ChromeClient_h

#include <memory>
#include "base/gtest_prod_util.h"
#include "core/CoreExport.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/AnimationWorkletProxyClient.h"
#include "core/dom/SandboxFlags.h"
#include "core/html/forms/PopupMenu.h"
#include "core/inspector/ConsoleTypes.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/NavigationPolicy.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/Cursor.h"
#include "platform/PlatformChromeClient.h"
#include "platform/WebFrameScheduler.h"
#include "platform/graphics/TouchAction.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/text/TextDirection.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Vector.h"
#include "public/platform/BlameContext.h"
#include "public/platform/WebDragOperation.h"
#include "public/platform/WebEventListenerProperties.h"
#include "public/platform/WebFocusType.h"
#include "public/platform/WebScrollBoundaryBehavior.h"

// To avoid conflicts with the CreateWindow macro from the Windows SDK...
#undef CreateWindow

namespace blink {

class ColorChooser;
class ColorChooserClient;
class CompositorAnimationTimeline;
class DateTimeChooser;
class DateTimeChooserClient;
class Element;
class FileChooser;
class FloatPoint;
class Frame;
class GraphicsLayer;
class HTMLFormControlElement;
class HTMLInputElement;
class HTMLSelectElement;
class HitTestResult;
class IntRect;
class KeyboardEvent;
class LocalFrame;
class Node;
class Page;
class PagePopup;
class PagePopupClient;
class PopupOpeningObserver;
class WebDragData;
class WebImage;
class WebLayer;
class WebLayerTreeView;
class WebTappedInfo;
class WebViewImpl;

struct CompositedSelection;
struct DateTimeChooserParameters;
struct FrameLoadRequest;
struct ViewportDescription;
struct WebCursorInfo;
struct WebPoint;
struct WebScreenInfo;
struct WebWindowFeatures;

class CORE_EXPORT ChromeClient : public PlatformChromeClient {
 public:
  virtual void ChromeDestroyed() = 0;

  // The specified rectangle is adjusted for the minimum window size and the
  // screen, then setWindowRect with the adjusted rectangle is called.
  void SetWindowRectWithAdjustment(const IntRect&, LocalFrame&);
  virtual IntRect RootWindowRect() = 0;

  virtual IntRect PageRect() = 0;

  virtual void Focus() = 0;

  virtual bool CanTakeFocus(WebFocusType) = 0;
  virtual void TakeFocus(WebFocusType) = 0;

  virtual void FocusedNodeChanged(Node*, Node*) = 0;

  virtual bool HadFormInteraction() const = 0;

  virtual void BeginLifecycleUpdates() = 0;

  // Start a system drag and drop operation.
  virtual void StartDragging(LocalFrame*,
                             const WebDragData&,
                             WebDragOperationsMask,
                             const WebImage& drag_image,
                             const WebPoint& drag_image_offset) = 0;
  virtual bool AcceptsLoadDrops() const = 0;

  // The LocalFrame pointer provides the ChromeClient with context about which
  // LocalFrame wants to create the new Page. Also, the newly created window
  // should not be shown to the user until the ChromeClient of the newly
  // created Page has its show method called.
  // The FrameLoadRequest parameter is only for ChromeClient to check if the
  // request could be fulfilled. The ChromeClient should not load the request.
  virtual Page* CreateWindow(LocalFrame*,
                             const FrameLoadRequest&,
                             const WebWindowFeatures&,
                             NavigationPolicy,
                             SandboxFlags) = 0;
  virtual void Show(NavigationPolicy) = 0;

  // All the parameters should be in viewport space. That is, if an event
  // scrolls by 10 px, but due to a 2X page scale we apply a 5px scroll to the
  // root frame, all of which is handled as overscroll, we should return 10px
  // as the overscrollDelta.
  virtual void DidOverscroll(const FloatSize& overscroll_delta,
                             const FloatSize& accumulated_overscroll,
                             const FloatPoint& position_in_viewport,
                             const FloatSize& velocity_in_viewport,
                             const WebScrollBoundaryBehavior&) = 0;

  virtual bool ShouldReportDetailedMessageForSource(LocalFrame&,
                                                    const String& source) = 0;
  virtual void AddMessageToConsole(LocalFrame*,
                                   MessageSource,
                                   MessageLevel,
                                   const String& message,
                                   unsigned line_number,
                                   const String& source_id,
                                   const String& stack_trace) = 0;

  virtual bool CanOpenBeforeUnloadConfirmPanel() = 0;
  bool OpenBeforeUnloadConfirmPanel(const String& message,
                                    LocalFrame*,
                                    bool is_reload);

  virtual void CloseWindowSoon() = 0;

  bool OpenJavaScriptAlert(LocalFrame*, const String&);
  bool OpenJavaScriptConfirm(LocalFrame*, const String&);
  bool OpenJavaScriptPrompt(LocalFrame*,
                            const String& message,
                            const String& default_value,
                            String& result);
  virtual bool TabsToLinks() = 0;

  virtual WebViewImpl* GetWebView() const = 0;

  // Methods used by PlatformChromeClient.
  virtual WebScreenInfo GetScreenInfo() const = 0;
  virtual void SetCursor(const Cursor&, LocalFrame* local_root) = 0;
  // End methods used by PlatformChromeClient.

  virtual void SetCursorOverridden(bool) = 0;

  virtual void AutoscrollStart(WebFloatPoint position, LocalFrame*) {}
  virtual void AutoscrollFling(WebFloatSize velocity, LocalFrame*) {}
  virtual void AutoscrollEnd(LocalFrame*) {}

  virtual Cursor LastSetCursorForTesting() const = 0;
  Node* LastSetTooltipNodeForTesting() const {
    return last_mouse_over_node_.Get();
  }

  virtual void SetCursorForPlugin(const WebCursorInfo&, LocalFrame*) = 0;

  // Returns a custom visible content rect if a viewport override is active.
  virtual WTF::Optional<IntRect> VisibleContentRectForPainting() const {
    return WTF::nullopt;
  }

  virtual void DispatchViewportPropertiesDidChange(
      const ViewportDescription&) const {}

  virtual void ContentsSizeChanged(LocalFrame*, const IntSize&) const = 0;
  virtual void PageScaleFactorChanged() const {}
  virtual float ClampPageScaleFactorToLimits(float scale) const {
    return scale;
  }
  virtual void MainFrameScrollOffsetChanged() const {}
  virtual void ResizeAfterLayout() const {}
  virtual void LayoutUpdated() const {}

  void MouseDidMoveOverElement(LocalFrame&, const HitTestResult&);
  virtual void SetToolTip(LocalFrame&, const String&, TextDirection) = 0;
  void ClearToolTip(LocalFrame&);

  bool Print(LocalFrame*);

  virtual ColorChooser* OpenColorChooser(LocalFrame*,
                                         ColorChooserClient*,
                                         const Color&) = 0;

  // This function is used for:
  //  - Mandatory date/time choosers if InputMultipleFieldsUI flag is not set
  //  - Date/time choosers for types for which
  //    LayoutTheme::SupportsCalendarPicker returns true, if
  //    InputMultipleFieldsUI flag is set
  //  - <datalist> UI for date/time input types regardless of
  //    InputMultipleFieldsUI flag
  virtual DateTimeChooser* OpenDateTimeChooser(
      DateTimeChooserClient*,
      const DateTimeChooserParameters&) = 0;

  virtual void OpenTextDataListChooser(HTMLInputElement&) = 0;

  virtual void OpenFileChooser(LocalFrame*, scoped_refptr<FileChooser>) = 0;

  // Asychronous request to enumerate all files in a directory chosen by the
  // user.
  virtual void EnumerateChosenDirectory(FileChooser*) = 0;

  // Pass nullptr as the GraphicsLayer to detach the root layer.
  // This sets the graphics layer for the LocalFrame's WebWidget, if it has
  // one. Otherwise it sets it for the WebViewImpl.
  virtual void AttachRootGraphicsLayer(GraphicsLayer*,
                                       LocalFrame* local_root) = 0;

  // Pass nullptr as the WebLayer to detach the root layer.
  // This sets the WebLayer for the LocalFrame's WebWidget, if it has
  // one. Otherwise it sets it for the WebViewImpl.
  virtual void AttachRootLayer(WebLayer*, LocalFrame* local_root) = 0;

  virtual void AttachCompositorAnimationTimeline(CompositorAnimationTimeline*,
                                                 LocalFrame* local_root) {}
  virtual void DetachCompositorAnimationTimeline(CompositorAnimationTimeline*,
                                                 LocalFrame* local_root) {}

  virtual void EnterFullscreen(LocalFrame&) {}
  virtual void ExitFullscreen(LocalFrame&) {}
  virtual void FullscreenElementChanged(Element* old_element,
                                        Element* new_element) {}

  virtual void ClearCompositedSelection(LocalFrame*) {}
  virtual void UpdateCompositedSelection(LocalFrame*,
                                         const CompositedSelection&) {}

  virtual void SetEventListenerProperties(LocalFrame*,
                                          WebEventListenerClass,
                                          WebEventListenerProperties) = 0;
  virtual WebEventListenerProperties EventListenerProperties(
      LocalFrame*,
      WebEventListenerClass) const = 0;
  virtual void UpdateEventRectsForSubframeIfNecessary(LocalFrame*) = 0;
  virtual void SetHasScrollEventHandlers(LocalFrame*, bool) = 0;
  virtual void SetNeedsLowLatencyInput(LocalFrame*, bool) = 0;

  virtual void SetTouchAction(LocalFrame*, TouchAction) = 0;

  // Checks if there is an opened popup, called by LayoutMenuList::showPopup().
  virtual bool HasOpenedPopup() const = 0;
  virtual PopupMenu* OpenPopupMenu(LocalFrame&, HTMLSelectElement&) = 0;
  virtual PagePopup* OpenPagePopup(PagePopupClient*) = 0;
  virtual void ClosePagePopup(PagePopup*) = 0;
  virtual DOMWindow* PagePopupWindowForTesting() const = 0;

  virtual void SetBrowserControlsState(float top_height,
                                       float bottom_height,
                                       bool shrinks_layout){};
  virtual void SetBrowserControlsShownRatio(float){};

  virtual String AcceptLanguages() = 0;

  enum DialogType {
    kAlertDialog = 0,
    kConfirmDialog = 1,
    kPromptDialog = 2,
    kHTMLDialog = 3,
    kPrintDialog = 4
  };
  virtual bool ShouldOpenModalDialogDuringPageDismissal(
      LocalFrame&,
      DialogType,
      const String&,
      Document::PageDismissalType) const {
    return true;
  }

  virtual bool IsSVGImageChromeClient() const { return false; }

  virtual bool RequestPointerLock(LocalFrame*) { return false; }
  virtual void RequestPointerUnlock(LocalFrame*) {}

  virtual IntSize MinimumWindowSize() const { return IntSize(100, 100); }

  virtual bool IsChromeClientImpl() const { return false; }

  virtual void DidAssociateFormControlsAfterLoad(LocalFrame*) {}
  virtual void DidChangeValueInTextField(HTMLFormControlElement&) {}
  virtual void DidEndEditingOnTextField(HTMLInputElement&) {}
  virtual void HandleKeyboardEventOnTextField(HTMLInputElement&,
                                              KeyboardEvent&) {}
  virtual void TextFieldDataListChanged(HTMLInputElement&) {}
  virtual void AjaxSucceeded(LocalFrame*) {}

  // Input method editor related functions.
  virtual void ShowVirtualKeyboardOnElementFocus(LocalFrame&) {}

  virtual void RegisterViewportLayers() const {}

  virtual void ShowUnhandledTapUIIfNeeded(WebTappedInfo&) {}

  virtual void OnMouseDown(Node&) {}

  virtual void DidUpdateBrowserControls() const {}

  virtual void SetScrollBoundaryBehavior(const WebScrollBoundaryBehavior&) {}

  virtual void RegisterPopupOpeningObserver(PopupOpeningObserver*) = 0;
  virtual void UnregisterPopupOpeningObserver(PopupOpeningObserver*) = 0;
  virtual void NotifyPopupOpeningObservers() const = 0;

  virtual FloatSize ElasticOverscroll() const { return FloatSize(); }

  virtual std::unique_ptr<WebFrameScheduler> CreateFrameScheduler(
      BlameContext*,
      WebFrameScheduler::FrameType) = 0;

  // Returns the time of the beginning of the last beginFrame, in seconds, if
  // any, and 0.0 otherwise.
  virtual double LastFrameTimeMonotonic() const { return 0.0; }

  virtual void InstallSupplements(LocalFrame&);

  virtual WebLayerTreeView* GetWebLayerTreeView(LocalFrame*) { return nullptr; }

  virtual void RequestDecode(LocalFrame*,
                             const PaintImage& image,
                             WTF::Function<void(bool)> callback) {
    callback(false);
  }

  void Trace(blink::Visitor*);

 protected:
  ~ChromeClient() override {}

  virtual void ShowMouseOverURL(const HitTestResult&) = 0;
  virtual void SetWindowRect(const IntRect&, LocalFrame&) = 0;
  virtual bool OpenBeforeUnloadConfirmPanelDelegate(LocalFrame*,
                                                    bool is_reload) = 0;
  virtual bool OpenJavaScriptAlertDelegate(LocalFrame*, const String&) = 0;
  virtual bool OpenJavaScriptConfirmDelegate(LocalFrame*, const String&) = 0;
  virtual bool OpenJavaScriptPromptDelegate(LocalFrame*,
                                            const String& message,
                                            const String& default_value,
                                            String& result) = 0;
  virtual void PrintDelegate(LocalFrame*) = 0;

 private:
  bool CanOpenModalIfDuringPageDismissal(Frame& main_frame,
                                         DialogType,
                                         const String& message);
  void SetToolTip(LocalFrame&, const HitTestResult&);

  WeakMember<Node> last_mouse_over_node_;
  LayoutPoint last_tool_tip_point_;
  String last_tool_tip_text_;

  FRIEND_TEST_ALL_PREFIXES(ChromeClientTest, SetToolTipFlood);
};

}  // namespace blink

#endif  // ChromeClient_h
