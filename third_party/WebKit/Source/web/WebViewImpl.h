/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebViewImpl_h
#define WebViewImpl_h

#include "BackForwardClientImpl.h"
#include "ChromeClientImpl.h"
#include "ContextMenuClientImpl.h"
#include "DragClientImpl.h"
#include "EditorClientImpl.h"
#include "InspectorClientImpl.h"
#include "NotificationPresenterImpl.h"
#include "PageOverlayList.h"
#include "PageWidgetDelegate.h"
#include "UserMediaClientImpl.h"
#include "WebInputEvent.h"
#include "WebNavigationPolicy.h"
#include "WebView.h"
#include "core/page/PagePopupDriver.h"
#include "core/page/PageScaleConstraintsSet.h"
#include "core/platform/Timer.h"
#include "core/platform/graphics/FloatSize.h"
#include "core/platform/graphics/GraphicsContext3D.h"
#include "core/platform/graphics/GraphicsLayer.h"
#include "core/platform/graphics/IntPoint.h"
#include "core/platform/graphics/IntRect.h"
#include "public/platform/WebFloatQuad.h"
#include "public/platform/WebGestureCurveTarget.h"
#include "public/platform/WebLayer.h"
#include "public/platform/WebPoint.h"
#include "public/platform/WebRect.h"
#include "public/platform/WebSize.h"
#include "public/platform/WebString.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {
class ChromiumDataObject;
class Color;
class DocumentLoader;
class FloatSize;
class Frame;
class GraphicsContext3D;
class GraphicsLayerFactory;
class HistoryItem;
class HitTestResult;
class KeyboardEvent;
class Page;
class PageGroup;
class PagePopup;
class PagePopupClient;
class PlatformKeyboardEvent;
class PopupContainer;
class PopupMenuClient;
class Range;
class RenderLayerCompositor;
class RenderTheme;
class Widget;
}

namespace WebKit {
class AutocompletePopupMenuClient;
class AutofillPopupMenuClient;
class ContextFeaturesClientImpl;
class ContextMenuClientImpl;
class DeviceOrientationClientProxy;
class GeolocationClientProxy;
class LinkHighlight;
class MIDIClientProxy;
class PinchViewports;
class PrerendererClientImpl;
class SpeechInputClientImpl;
class SpeechRecognitionClientProxy;
class UserMediaClientImpl;
class ValidationMessageClientImpl;
class WebAXObject;
class WebActiveGestureAnimation;
class WebCompositorImpl;
class WebDevToolsAgentClient;
class WebDevToolsAgentPrivate;
class WebDocument;
class WebFrameImpl;
class WebGestureEvent;
class WebHelperPluginImpl;
class WebImage;
class WebKeyboardEvent;
class WebLayerTreeView;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebPagePopupImpl;
class WebPrerendererClient;
class WebSettingsImpl;
class WebTouchEvent;
class FullscreenController;

class WebViewImpl : public WebView
    , public RefCounted<WebViewImpl>
    , public WebGestureCurveTarget
    , public WebCore::PagePopupDriver
    , public PageWidgetEventHandler {
public:

    // WebWidget methods:
    virtual void close();
    virtual WebSize size();
    virtual void willStartLiveResize();
    virtual void resize(const WebSize&);
    virtual void willEndLiveResize();
    virtual void willEnterFullScreen();
    virtual void didEnterFullScreen();
    virtual void willExitFullScreen();
    virtual void didExitFullScreen();
    virtual void animate(double);
    virtual void layout();
    virtual void enterForceCompositingMode(bool enable) OVERRIDE;
    virtual void paint(WebCanvas*, const WebRect&, PaintOptions = ReadbackFromCompositorIfAvailable);
    virtual bool isTrackingRepaints() const OVERRIDE;
    virtual void themeChanged();
    virtual void setNeedsRedraw();
    virtual bool handleInputEvent(const WebInputEvent&);
    virtual void setCursorVisibilityState(bool isVisible);
    virtual bool hasTouchEventHandlersAt(const WebPoint&);
    virtual void applyScrollAndScale(const WebSize&, float);
    virtual void mouseCaptureLost();
    virtual void setFocus(bool enable);
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart,
        int selectionEnd);
    virtual bool confirmComposition();
    virtual bool confirmComposition(ConfirmCompositionBehavior selectionBehavior);
    virtual bool confirmComposition(const WebString& text);
    virtual bool compositionRange(size_t* location, size_t* length);
    virtual WebTextInputInfo textInputInfo();
    virtual bool setEditableSelectionOffsets(int start, int end);
    virtual bool setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines);
    virtual void extendSelectionAndDelete(int before, int after);
    virtual bool isSelectionEditable() const;
    virtual WebColor backgroundColor() const;
    virtual bool selectionBounds(WebRect& anchor, WebRect& focus) const;
    virtual bool selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const;
    virtual bool isSelectionAnchorFirst() const;
    virtual bool caretOrSelectionRange(size_t* location, size_t* length);
    virtual void setTextDirection(WebTextDirection direction);
    virtual bool isAcceleratedCompositingActive() const;
    virtual void willCloseLayerTreeView();
    virtual void didAcquirePointerLock();
    virtual void didNotAcquirePointerLock();
    virtual void didLosePointerLock();
    virtual void didChangeWindowResizerRect();
    virtual void didExitCompositingMode();

    // WebView methods:
    virtual void initializeMainFrame(WebFrameClient*);
    virtual void initializeHelperPluginFrame(WebFrameClient*);
    virtual void setAutofillClient(WebAutofillClient*);
    virtual void setDevToolsAgentClient(WebDevToolsAgentClient*);
    virtual void setPermissionClient(WebPermissionClient*);
    virtual void setPrerendererClient(WebPrerendererClient*) OVERRIDE;
    virtual void setSpellCheckClient(WebSpellCheckClient*);
    virtual void setValidationMessageClient(WebValidationMessageClient*) OVERRIDE;
    virtual void setPasswordGeneratorClient(WebPasswordGeneratorClient*) OVERRIDE;
    virtual WebSettings* settings();
    virtual WebString pageEncoding() const;
    virtual void setPageEncoding(const WebString& encoding);
    virtual bool isTransparent() const;
    virtual void setIsTransparent(bool value);
    virtual void setBaseBackgroundColor(WebColor);
    virtual bool tabsToLinks() const;
    virtual void setTabsToLinks(bool value);
    virtual bool tabKeyCyclesThroughElements() const;
    virtual void setTabKeyCyclesThroughElements(bool value);
    virtual bool isActive() const;
    virtual void setIsActive(bool value);
    virtual void setDomainRelaxationForbidden(bool, const WebString& scheme);
    virtual void setWindowFeatures(const WebWindowFeatures&);
    virtual bool dispatchBeforeUnloadEvent();
    virtual void dispatchUnloadEvent();
    virtual WebFrame* mainFrame();
    virtual WebFrame* findFrameByName(
        const WebString& name, WebFrame* relativeToFrame);
    virtual WebFrame* focusedFrame();
    virtual void setFocusedFrame(WebFrame* frame);
    virtual void setInitialFocus(bool reverse);
    virtual void clearFocusedNode();
    virtual void scrollFocusedNodeIntoView();
    virtual void scrollFocusedNodeIntoRect(const WebRect&);
    virtual void zoomToFindInPageRect(const WebRect&);
    virtual void advanceFocus(bool reverse);
    virtual double zoomLevel();
    virtual double setZoomLevel(double);
    virtual double setZoomLevel(bool textOnly, double zoomLevel);
    virtual void zoomLimitsChanged(double minimumZoomLevel,
                                   double maximumZoomLevel);
    virtual float textZoomFactor();
    virtual float setTextZoomFactor(float);
    virtual void setInitialPageScaleOverride(float);
    virtual bool zoomToMultipleTargetsRect(const WebRect&);
    virtual float pageScaleFactor() const;
    virtual void setPageScaleFactorPreservingScrollOffset(float);
    virtual void setPageScaleFactor(float scaleFactor, const WebPoint& origin);
    virtual void setPageScaleFactorLimits(float minPageScale, float maxPageScale);
    virtual float minimumPageScaleFactor() const;
    virtual float maximumPageScaleFactor() const;
    virtual void saveScrollAndScaleState();
    virtual void restoreScrollAndScaleState();
    virtual void resetScrollAndScaleState();
    virtual void setIgnoreViewportTagScaleLimits(bool);
    virtual WebSize contentsPreferredMinimumSize();

    virtual float deviceScaleFactor() const;
    virtual void setDeviceScaleFactor(float);
    virtual bool isFixedLayoutModeEnabled() const;
    virtual void enableFixedLayoutMode(bool enable);
    virtual WebSize fixedLayoutSize() const;
    virtual void setFixedLayoutSize(const WebSize&);
    virtual void enableAutoResizeMode(
        const WebSize& minSize,
        const WebSize& maxSize);
    virtual void disableAutoResizeMode();
    virtual void performMediaPlayerAction(
        const WebMediaPlayerAction& action,
        const WebPoint& location);
    virtual void performPluginAction(
        const WebPluginAction&,
        const WebPoint&);
    virtual WebHitTestResult hitTestResultAt(const WebPoint&);
    virtual void copyImageAt(const WebPoint& point);
    virtual void dragSourceEndedAt(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperation operation);
    virtual void dragSourceMovedTo(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperation operation);
    virtual void dragSourceSystemDragEnded();
    virtual WebDragOperation dragTargetDragEnter(
        const WebDragData&,
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed,
        int keyModifiers);
    virtual WebDragOperation dragTargetDragOver(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed,
        int keyModifiers);
    virtual void dragTargetDragLeave();
    virtual void dragTargetDrop(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        int keyModifiers);
    virtual void spellingMarkers(WebVector<uint32_t>* markers);
    virtual unsigned long createUniqueIdentifierForRequest();
    virtual void inspectElementAt(const WebPoint& point);
    virtual WebString inspectorSettings() const;
    virtual void setInspectorSettings(const WebString& settings);
    virtual bool inspectorSetting(const WebString& key, WebString* value) const;
    virtual void setInspectorSetting(const WebString& key,
                                     const WebString& value);
    virtual WebDevToolsAgent* devToolsAgent();
    virtual WebAXObject accessibilityObject();
    virtual void applyAutofillSuggestions(
        const WebNode&,
        const WebVector<WebString>& names,
        const WebVector<WebString>& labels,
        const WebVector<WebString>& icons,
        const WebVector<int>& itemIDs,
        int separatorIndex);
    virtual void hidePopups();
    virtual void selectAutofillSuggestionAtIndex(unsigned listIndex);
    virtual void setScrollbarColors(unsigned inactiveColor,
                                    unsigned activeColor,
                                    unsigned trackColor);
    virtual void setSelectionColors(unsigned activeBackgroundColor,
                                    unsigned activeForegroundColor,
                                    unsigned inactiveBackgroundColor,
                                    unsigned inactiveForegroundColor);
    virtual void performCustomContextMenuAction(unsigned action);
    virtual void showContextMenu();
    virtual void addPageOverlay(WebPageOverlay*, int /* zOrder */);
    virtual void removePageOverlay(WebPageOverlay*);
    virtual void transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters&);
    virtual void setShowPaintRects(bool);
    virtual void setShowDebugBorders(bool);
    virtual void setShowFPSCounter(bool);
    virtual void setContinuousPaintingEnabled(bool);
    virtual void setShowScrollBottleneckRects(bool);

    // WebViewImpl

    void suppressInvalidations(bool enable);
    void invalidateRect(const WebCore::IntRect&);

    void setIgnoreInputEvents(bool newValue);
    WebDevToolsAgentPrivate* devToolsAgentPrivate() { return m_devToolsAgent.get(); }

    WebCore::Color baseBackgroundColor() const { return m_baseBackgroundColor; }

    PageOverlayList* pageOverlays() const { return m_pageOverlays.get(); }

    void setOverlayLayer(WebCore::GraphicsLayer*);

    const WebPoint& lastMouseDownPoint() const
    {
        return m_lastMouseDownPoint;
    }

    WebCore::Frame* focusedWebCoreFrame() const;

    // Returns the currently focused Element or null if no element has focus.
    WebCore::Element* focusedElement();

    static WebViewImpl* fromPage(WebCore::Page*);

    WebViewClient* client()
    {
        return m_client;
    }

    WebAutofillClient* autofillClient()
    {
        return m_autofillClient;
    }

    WebPermissionClient* permissionClient()
    {
        return m_permissionClient;
    }

    WebSpellCheckClient* spellCheckClient()
    {
        return m_spellCheckClient;
    }

    WebPasswordGeneratorClient* passwordGeneratorClient() const
    {
        return m_passwordGeneratorClient;
    }

    // Returns the page object associated with this view. This may be null when
    // the page is shutting down, but will be valid at all other times.
    WebCore::Page* page() const
    {
        return m_page.get();
    }

    // Returns the main frame associated with this view. This may be null when
    // the page is shutting down, but will be valid at all other times.
    WebFrameImpl* mainFrameImpl();

    // History related methods:
    void observeNewNavigation();

    // Event related methods:
    void mouseContextMenu(const WebMouseEvent&);
    void mouseDoubleClick(const WebMouseEvent&);

    bool detectContentOnTouch(const WebPoint&);
    bool startPageScaleAnimation(const WebCore::IntPoint& targetPosition, bool useAnchor, float newScale, double durationInSeconds);

    void numberOfWheelEventHandlersChanged(unsigned);
    void hasTouchEventHandlers(bool);

    // WebGestureCurveTarget implementation for fling.
    virtual void scrollBy(const WebFloatSize&);

    // Handles context menu events orignated via the the keyboard. These
    // include the VK_APPS virtual key and the Shift+F10 combine. Code is
    // based on the Webkit function bool WebView::handleContextMenuEvent(WPARAM
    // wParam, LPARAM lParam) in webkit\webkit\win\WebView.cpp. The only
    // significant change in this function is the code to convert from a
    // Keyboard event to the Right Mouse button down event.
    bool sendContextMenuEvent(const WebKeyboardEvent&);

    // Notifies the WebView that a load has been committed. isNewNavigation
    // will be true if a new session history item should be created for that
    // load. isNavigationWithinPage will be true if the navigation does
    // not take the user away from the current page.
    void didCommitLoad(bool* isNewNavigation, bool isNavigationWithinPage);

    // Indicates two things:
    //   1) This view may have a new layout now.
    //   2) Calling layout() is a no-op.
    // After calling WebWidget::layout(), expect to get this notification
    // unless the view did not need a layout.
    void layoutUpdated(WebFrameImpl*);

    void didChangeContentsSize();
    void deviceOrPageScaleFactorChanged();

    // Returns true if popup menus should be rendered by the browser, false if
    // they should be rendered by WebKit (which is the default).
    static bool useExternalPopupMenus();

    bool contextMenuAllowed() const
    {
        return m_contextMenuAllowed;
    }

    bool shouldAutoResize() const
    {
        return m_shouldAutoResize;
    }

    WebCore::IntSize minAutoSize() const
    {
        return m_minAutoSize;
    }

    WebCore::IntSize maxAutoSize() const
    {
        return m_maxAutoSize;
    }

    void updatePageDefinedPageScaleConstraints(const WebCore::ViewportArguments&);

    // Start a system drag and drop operation.
    void startDragging(
        WebCore::Frame*,
        const WebDragData& dragData,
        WebDragOperationsMask mask,
        const WebImage& dragImage,
        const WebPoint& dragImageOffset);

    void autofillPopupDidHide()
    {
        m_autofillPopupShowing = false;
    }

    // Returns the provider of desktop notifications.
    NotificationPresenterImpl* notificationPresenterImpl();

    // Tries to scroll a frame or any parent of a frame. Returns true if the view
    // was scrolled.
    bool propagateScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity);

    // Notification that a popup was opened/closed.
    void popupOpened(WebCore::PopupContainer* popupContainer);
    void popupClosed(WebCore::PopupContainer* popupContainer);
    // PagePopupDriver functions.
    virtual WebCore::PagePopup* openPagePopup(WebCore::PagePopupClient*, const WebCore::IntRect& originBoundsInRootView) OVERRIDE;
    virtual void closePagePopup(WebCore::PagePopup*) OVERRIDE;

    void hideAutofillPopup();

    // Creates a Helper Plugin of |pluginType| for |hostDocument|.
    WebHelperPluginImpl* createHelperPlugin(const String& pluginType, const WebDocument& hostDocument);
    void closeHelperPluginSoon(PassRefPtr<WebHelperPluginImpl>);

    // Returns the input event we're currently processing. This is used in some
    // cases where the WebCore DOM event doesn't have the information we need.
    static const WebInputEvent* currentInputEvent()
    {
        return m_currentInputEvent;
    }

    WebCore::GraphicsLayer* rootGraphicsLayer();
    bool allowsAcceleratedCompositing();
    void setRootGraphicsLayer(WebCore::GraphicsLayer*);
    void scheduleCompositingLayerSync();
    void scrollRootLayerRect(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& clipRect);
    WebCore::GraphicsLayerFactory* graphicsLayerFactory() const;
    WebCore::RenderLayerCompositor* compositor() const;
    void registerForAnimations(WebLayer*);
    void scheduleAnimation();

    void didProgrammaticallyScroll(const WebCore::IntPoint& scrollPoint);

    virtual void setVisibilityState(WebPageVisibilityState, bool);

    WebCore::PopupContainer* selectPopup() const { return m_selectPopup.get(); }
    bool hasOpenedPopup() const { return m_selectPopup || m_pagePopup; }

    // Returns true if the event leads to scrolling.
    static bool mapKeyCodeForScroll(int keyCode,
                                   WebCore::ScrollDirection* scrollDirection,
                                   WebCore::ScrollGranularity* scrollGranularity);

    // Called by a full frame plugin inside this view to inform it that its
    // zoom level has been updated.  The plugin should only call this function
    // if the zoom change was triggered by the browser, it's only needed in case
    // a plugin can update its own zoom, say because of its own UI.
    void fullFramePluginZoomLevelChanged(double zoomLevel);

    void computeScaleAndScrollForBlockRect(const WebRect& blockRect, float padding, float defaultScaleWhenAlreadyLegible, float& scale, WebPoint& scroll);
    WebCore::Node* bestTapNode(const WebCore::PlatformGestureEvent& tapEvent);
    void enableTapHighlightAtPoint(const WebCore::PlatformGestureEvent& tapEvent);
    void enableTapHighlights(Vector<WebCore::Node*>&);
    void computeScaleAndScrollForFocusedNode(WebCore::Node* focusedNode, float& scale, WebCore::IntPoint& scroll, bool& needAnimation);

    void animateDoubleTapZoom(const WebCore::IntPoint&);

    void enableFakePageScaleAnimationForTesting(bool);
    bool fakeDoubleTapAnimationPendingForTesting() const { return m_doubleTapZoomPending; }
    WebCore::IntPoint fakePageScaleAnimationTargetPositionForTesting() const { return m_fakePageScaleAnimationTargetPosition; }
    float fakePageScaleAnimationPageScaleForTesting() const { return m_fakePageScaleAnimationPageScaleFactor; }
    bool fakePageScaleAnimationUseAnchorForTesting() const { return m_fakePageScaleAnimationUseAnchor; }

    void enterFullScreenForElement(WebCore::Element*);
    void exitFullScreenForElement(WebCore::Element*);

    // Exposed for the purpose of overriding device metrics.
    void sendResizeEventAndRepaint();

    // Exposed for testing purposes.
    bool hasHorizontalScrollbar();
    bool hasVerticalScrollbar();

    // Pointer Lock calls allow a page to capture all mouse events and
    // disable the system cursor.
    virtual bool requestPointerLock();
    virtual void requestPointerUnlock();
    virtual bool isPointerLocked();

    // Heuristic-based function for determining if we should disable workarounds
    // for viewing websites that are not optimized for mobile devices.
    bool shouldDisableDesktopWorkarounds();

    // Exposed for tests.
    unsigned numLinkHighlights() { return m_linkHighlights.size(); }
    LinkHighlight* linkHighlight(int i) { return m_linkHighlights[i].get(); }

    WebSettingsImpl* settingsImpl();

    // Returns the bounding box of the block type node touched by the WebRect.
    WebRect computeBlockBounds(const WebRect&, bool ignoreClipping);

    WebCore::IntPoint clampOffsetAtScale(const WebCore::IntPoint& offset, float scale);

    // Exposed for tests.
    WebVector<WebCompositionUnderline> compositionUnderlines() const;

    WebLayerTreeView* layerTreeView() const { return m_layerTreeView; };

private:
    float legibleScale() const;
    void refreshPageScaleFactorAfterLayout();
    void setUserAgentPageScaleConstraints(WebCore::PageScaleConstraints newConstraints);
    float clampPageScaleFactorToLimits(float) const;
    WebCore::IntSize contentsSize() const;

    void resetSavedScrollAndScaleState();

    void updateMainFrameScrollPosition(const WebCore::IntPoint& scrollPosition, bool programmaticScroll);

    friend class WebView;  // So WebView::Create can call our constructor
    friend class WTF::RefCounted<WebViewImpl>;
    friend void setCurrentInputEventForTest(const WebInputEvent*);

    enum DragAction {
      DragEnter,
      DragOver
    };

    WebViewImpl(WebViewClient*);
    virtual ~WebViewImpl();

    WebTextInputType textInputType();

    WebString inputModeOfFocusedElement();

    // Returns true if the event was actually processed.
    bool keyEventDefault(const WebKeyboardEvent&);

    // Returns true if the autocomple has consumed the event.
    bool autocompleteHandleKeyEvent(const WebKeyboardEvent&);

    // Repaints the Autofill popup. Should be called when the suggestions
    // have changed. Note that this should only be called when the Autofill
    // popup is showing.
    void refreshAutofillPopup();

    bool confirmComposition(const WebString& text, ConfirmCompositionBehavior);

    // Returns true if the view was scrolled.
    bool scrollViewWithKeyboard(int keyCode, int modifiers);

    void hideSelectPopup();

    // Converts |pos| from window coordinates to contents coordinates and gets
    // the HitTestResult for it.
    WebCore::HitTestResult hitTestResultForWindowPos(const WebCore::IntPoint&);

    // Consolidate some common code between starting a drag over a target and
    // updating a drag over a target. If we're starting a drag, |isEntering|
    // should be true.
    WebDragOperation dragTargetDragEnterOrOver(const WebPoint& clientPoint,
                                               const WebPoint& screenPoint,
                                               DragAction,
                                               int keyModifiers);

    void configureAutoResizeMode();

    void setIsAcceleratedCompositingActive(bool);
    void doComposite();
    void doPixelReadbackToCanvas(WebCanvas*, const WebCore::IntRect&);
    void reallocateRenderer();
    void updateLayerTreeViewport();

    // Helper function: Widens the width of |source| by the specified margins
    // while keeping it smaller than page width.
    WebRect widenRectWithinPageBounds(const WebRect& source, int targetMargin, int minimumMargin);

    void pointerLockMouseEvent(const WebInputEvent&);

    // PageWidgetEventHandler functions
    virtual void handleMouseLeave(WebCore::Frame&, const WebMouseEvent&) OVERRIDE;
    virtual void handleMouseDown(WebCore::Frame&, const WebMouseEvent&) OVERRIDE;
    virtual void handleMouseUp(WebCore::Frame&, const WebMouseEvent&) OVERRIDE;
    virtual bool handleMouseWheel(WebCore::Frame&, const WebMouseWheelEvent&) OVERRIDE;
    virtual bool handleGestureEvent(const WebGestureEvent&) OVERRIDE;
    virtual bool handleKeyEvent(const WebKeyboardEvent&) OVERRIDE;
    virtual bool handleCharEvent(const WebKeyboardEvent&) OVERRIDE;

    void closePendingHelperPlugins(WebCore::Timer<WebViewImpl>*);

    WebViewClient* m_client; // Can be 0 (e.g. unittests, shared workers, etc.)
    WebAutofillClient* m_autofillClient;
    WebPermissionClient* m_permissionClient;
    WebSpellCheckClient* m_spellCheckClient;
    WebPasswordGeneratorClient* m_passwordGeneratorClient;

    ChromeClientImpl m_chromeClientImpl;
    ContextMenuClientImpl m_contextMenuClientImpl;
    DragClientImpl m_dragClientImpl;
    EditorClientImpl m_editorClientImpl;
    InspectorClientImpl m_inspectorClientImpl;
    BackForwardClientImpl m_backForwardClientImpl;

    WebSize m_size;
    bool m_fixedLayoutSizeLock;
    // If true, automatically resize the render view around its content.
    bool m_shouldAutoResize;
    // The lower bound on the size when auto-resizing.
    WebCore::IntSize m_minAutoSize;
    // The upper bound on the size when auto-resizing.
    WebCore::IntSize m_maxAutoSize;

    OwnPtr<WebCore::Page> m_page;

    // This flag is set when a new navigation is detected. It is used to satisfy
    // the corresponding argument to WebFrameClient::didCommitProvisionalLoad.
    bool m_observedNewNavigation;
#ifndef NDEBUG
    // Used to assert that the new navigation we observed is the same navigation
    // when we make use of m_observedNewNavigation.
    const WebCore::DocumentLoader* m_newNavigationLoader;
#endif

    // An object that can be used to manipulate m_page->settings() without linking
    // against WebCore. This is lazily allocated the first time GetWebSettings()
    // is called.
    OwnPtr<WebSettingsImpl> m_webSettings;

    // A copy of the web drop data object we received from the browser.
    RefPtr<WebCore::ChromiumDataObject> m_currentDragData;

    // The point relative to the client area where the mouse was last pressed
    // down. This is used by the drag client to determine what was under the
    // mouse when the drag was initiated. We need to track this here in
    // WebViewImpl since DragClient::startDrag does not pass the position the
    // mouse was at when the drag was initiated, only the current point, which
    // can be misleading as it is usually not over the element the user actually
    // dragged by the time a drag is initiated.
    WebPoint m_lastMouseDownPoint;

    // Keeps track of the current zoom level. 0 means no zoom, positive numbers
    // mean zoom in, negative numbers mean zoom out.
    double m_zoomLevel;

    double m_minimumZoomLevel;

    double m_maximumZoomLevel;

    WebCore::PageScaleConstraintsSet m_pageScaleConstraintsSet;

    // Saved page scale state.
    float m_savedPageScaleFactor; // 0 means that no page scale factor is saved.
    WebCore::IntSize m_savedScrollOffset;

    // The scale moved to by the latest double tap zoom, if any.
    float m_doubleTapZoomPageScaleFactor;
    // Have we sent a double-tap zoom and not yet heard back the scale?
    bool m_doubleTapZoomPending;

    // Used for testing purposes.
    bool m_enableFakePageScaleAnimationForTesting;
    WebCore::IntPoint m_fakePageScaleAnimationTargetPosition;
    float m_fakePageScaleAnimationPageScaleFactor;
    bool m_fakePageScaleAnimationUseAnchor;

    bool m_contextMenuAllowed;

    bool m_doingDragAndDrop;

    bool m_ignoreInputEvents;

    // Webkit expects keyPress events to be suppressed if the associated keyDown
    // event was handled. Safari implements this behavior by peeking out the
    // associated WM_CHAR event if the keydown was handled. We emulate
    // this behavior by setting this flag if the keyDown was handled.
    bool m_suppressNextKeypressEvent;

    // Represents whether or not this object should process incoming IME events.
    bool m_imeAcceptEvents;

    // The available drag operations (copy, move link...) allowed by the source.
    WebDragOperation m_operationsAllowed;

    // The current drag operation as negotiated by the source and destination.
    // When not equal to DragOperationNone, the drag data can be dropped onto the
    // current drop target in this WebView (the drop target can accept the drop).
    WebDragOperation m_dragOperation;

    // Context-based feature switches.
    OwnPtr<ContextFeaturesClientImpl> m_featureSwitchClient;

    // Whether an Autofill popup is currently showing.
    bool m_autofillPopupShowing;

    // The Autofill popup client.
    OwnPtr<AutofillPopupMenuClient> m_autofillPopupClient;

    // The Autofill popup.
    RefPtr<WebCore::PopupContainer> m_autofillPopup;

    // The popup associated with a select element.
    RefPtr<WebCore::PopupContainer> m_selectPopup;

    // The popup associated with an input element.
    RefPtr<WebPagePopupImpl> m_pagePopup;

    OwnPtr<WebDevToolsAgentPrivate> m_devToolsAgent;
    OwnPtr<PageOverlayList> m_pageOverlays;

    // Whether the webview is rendering transparently.
    bool m_isTransparent;

    // Whether the user can press tab to focus links.
    bool m_tabsToLinks;

    // Inspector settings.
    WebString m_inspectorSettings;

    typedef HashMap<WTF::String, WTF::String> SettingsMap;
    OwnPtr<SettingsMap> m_inspectorSettingsMap;

    // The provider of desktop notifications;
    NotificationPresenterImpl m_notificationPresenter;

    // If set, the (plugin) node which has mouse capture.
    RefPtr<WebCore::Node> m_mouseCaptureNode;

    WebCore::IntRect m_rootLayerScrollDamage;
    WebLayerTreeView* m_layerTreeView;
    WebLayer* m_rootLayer;
    WebCore::GraphicsLayer* m_rootGraphicsLayer;
    OwnPtr<WebCore::GraphicsLayerFactory> m_graphicsLayerFactory;
    bool m_isAcceleratedCompositingActive;
    bool m_layerTreeViewCommitsDeferred;
    bool m_compositorCreationFailed;
    // If true, the graphics context is being restored.
    bool m_recreatingGraphicsContext;
    static const WebInputEvent* m_currentInputEvent;
    OwnPtr<PinchViewports> m_pinchViewports;

#if ENABLE(INPUT_SPEECH)
    OwnPtr<SpeechInputClientImpl> m_speechInputClient;
#endif
    OwnPtr<SpeechRecognitionClientProxy> m_speechRecognitionClient;

    OwnPtr<DeviceOrientationClientProxy> m_deviceOrientationClientProxy;
    OwnPtr<GeolocationClientProxy> m_geolocationClientProxy;

    UserMediaClientImpl m_userMediaClientImpl;
    OwnPtr<MIDIClientProxy> m_midiClientProxy;
#if ENABLE(NAVIGATOR_CONTENT_UTILS)
    OwnPtr<NavigatorContentUtilsClientImpl> m_navigatorContentUtilsClient;
#endif
    OwnPtr<WebActiveGestureAnimation> m_gestureAnimation;
    WebPoint m_positionOnFlingStart;
    WebPoint m_globalPositionOnFlingStart;
    int m_flingModifier;
    bool m_flingSourceDevice;
    Vector<OwnPtr<LinkHighlight> > m_linkHighlights;
    OwnPtr<ValidationMessageClientImpl> m_validationMessage;
    OwnPtr<FullscreenController> m_fullscreenController;

    bool m_showFPSCounter;
    bool m_showPaintRects;
    bool m_showDebugBorders;
    bool m_continuousPaintingEnabled;
    bool m_showScrollBottleneckRects;
    WebColor m_baseBackgroundColor;

    WebCore::Timer<WebViewImpl> m_helperPluginCloseTimer;
    Vector<RefPtr<WebHelperPluginImpl> > m_helperPluginsPendingClose;
};

inline WebViewImpl* toWebViewImpl(WebView* webView)
{
    // We have no ways to check if the specified WebView is an instance of
    // WebViewImpl because WebViewImpl is the only implementation of WebView.
    return static_cast<WebViewImpl*>(webView);
}

} // namespace WebKit

#endif
