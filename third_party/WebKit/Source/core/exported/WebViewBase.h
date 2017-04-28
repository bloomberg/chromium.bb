// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.#ifndef WebViewBase_h

#ifndef WebViewBase_h
#define WebViewBase_h

#include "platform/transforms/TransformationMatrix.h"
#include "public/platform/WebDisplayMode.h"
#include "public/platform/WebInputEventResult.h"
#include "public/web/WebElement.h"
#include "public/web/WebView.h"

namespace blink {

class CompositorAnimationTimeline;
class ContextMenuProvider;
class DevToolsEmulator;
class Frame;
class Page;
class PageScaleConstraintsSet;
class WebInputEvent;
class WebKeyboardEvent;
class WebLocalFrameImpl;
class WebLayerTreeView;
class WebPagePopupImpl;
class WebSettingsImpl;
struct WebRect;

// WebViewBase is a temporary class introduced to decouple the defintion of
// WebViewImpl from the concrete implementation. It was not possible to move the
// defintion of these methods to WebView as we did not want to pollute the
// public API surface area.
//
// Once WebViewImpl is moved from web to core/exported then this class should be
// removed and clients can once again depend on WebViewImpl.
class WebViewBase : public WebView {
 public:
  virtual void SetBaseBackgroundColor(WebColor) = 0;
  virtual void SetBaseBackgroundColorOverride(WebColor) = 0;
  virtual void ClearBaseBackgroundColorOverride() = 0;
  virtual void SetBackgroundColorOverride(WebColor) = 0;
  virtual void ClearBackgroundColorOverride() = 0;
  virtual void SetZoomFactorOverride(float) = 0;
  virtual void SetCompositorDeviceScaleFactorOverride(float) = 0;
  virtual void SetDeviceEmulationTransform(const TransformationMatrix&) = 0;
  virtual void SetShowDebugBorders(bool) = 0;

  virtual Page* GetPage() const = 0;
  virtual Frame* FocusedCoreFrame() const = 0;

  // Returns the main frame associated with this view. This may be null when
  // the page is shutting down, but will be valid at all other times.
  virtual WebLocalFrameImpl* MainFrameImpl() const = 0;

  virtual float DefaultMinimumPageScaleFactor() const = 0;
  virtual float DefaultMaximumPageScaleFactor() const = 0;
  virtual float MinimumPageScaleFactor() const = 0;
  virtual float MaximumPageScaleFactor() const = 0;
  virtual float ClampPageScaleFactorToLimits(float) const = 0;
  virtual void ResetScaleStateImmediately() = 0;

  virtual WebLayerTreeView* LayerTreeView() const = 0;
  virtual WebViewClient* Client() = 0;

  virtual void ZoomToFindInPageRect(const WebRect&) = 0;

  virtual PageScaleConstraintsSet& GetPageScaleConstraintsSet() const = 0;
  virtual Color BaseBackgroundColor() const = 0;
  virtual bool BackgroundColorOverrideEnabled() const = 0;
  virtual WebColor BackgroundColorOverride() const = 0;

  virtual void DidChangeContentsSize() = 0;
  virtual void PageScaleFactorChanged() = 0;
  virtual void MainFrameScrollOffsetChanged() = 0;
  virtual void UpdateMainFrameLayoutSize() = 0;

  virtual DevToolsEmulator* GetDevToolsEmulator() const = 0;

  // Notifies the WebView that a load has been committed. isNewNavigation
  // will be true if a new session history item should be created for that
  // load. isNavigationWithinPage will be true if the navigation does
  // not take the user away from the current page.
  virtual void DidCommitLoad(bool is_new_navigation,
                             bool is_navigation_within_page) = 0;

  virtual void ShowContextMenuAtPoint(float x,
                                      float y,
                                      ContextMenuProvider*) = 0;
  virtual void ShowContextMenuForElement(WebElement) = 0;

  virtual IntSize MainFrameSize() = 0;
  virtual bool ShouldAutoResize() const = 0;
  virtual IntSize MinAutoSize() const = 0;
  virtual IntSize MaxAutoSize() const = 0;
  virtual WebDisplayMode DisplayMode() const = 0;

  virtual void DidUpdateFullscreenSize() = 0;
  virtual void SetLastHiddenPagePopup(WebPagePopupImpl*) = 0;
  virtual WebInputEventResult SendContextMenuEvent(const WebKeyboardEvent&) = 0;

  virtual WebSpellCheckClient* SpellCheckClient() = 0;

  virtual CompositorAnimationTimeline* LinkHighlightsTimeline() const = 0;

  virtual WebSettingsImpl* SettingsImpl() = 0;

  static const WebInputEvent* CurrentInputEvent();
};
}

#endif
