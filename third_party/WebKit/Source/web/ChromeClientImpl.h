/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef ChromeClientImpl_h
#define ChromeClientImpl_h

#include "WebNavigationPolicy.h"
#include "core/page/ChromeClient.h"
#include "core/platform/PopupMenu.h"
#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "public/platform/WebColor.h"
#include "wtf/PassOwnPtr.h"

namespace WebCore {
class AccessibilityObject;
class ColorChooser;
class ColorChooserClient;
class Element;
class FileChooser;
class GraphicsLayerFactory;
class PopupContainer;
class PopupMenuClient;
class RenderBox;
class SecurityOrigin;
class DateTimeChooser;
class DateTimeChooserClient;
struct WindowFeatures;
}

namespace WebKit {
class WebColorChooser;
class WebColorChooserClient;
class WebViewImpl;
struct WebCursorInfo;
struct WebPopupMenuInfo;

// Handles window-level notifications from WebCore on behalf of a WebView.
class ChromeClientImpl : public WebCore::ChromeClient {
public:
    explicit ChromeClientImpl(WebViewImpl* webView);
    virtual ~ChromeClientImpl();

    virtual void* webView() const;

    // ChromeClient methods:
    virtual void chromeDestroyed();
    virtual void setWindowRect(const WebCore::FloatRect&);
    virtual WebCore::FloatRect windowRect();
    virtual WebCore::FloatRect pageRect();
    virtual void focus();
    virtual void unfocus();
    virtual bool canTakeFocus(WebCore::FocusDirection);
    virtual void takeFocus(WebCore::FocusDirection);
    virtual void focusedNodeChanged(WebCore::Node*);
    virtual WebCore::Page* createWindow(
        WebCore::Frame*, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&, WebCore::NavigationPolicy);
    virtual void show(WebCore::NavigationPolicy);
    virtual bool canRunModal();
    virtual void runModal();
    virtual void setToolbarsVisible(bool);
    virtual bool toolbarsVisible();
    virtual void setStatusbarVisible(bool);
    virtual bool statusbarVisible();
    virtual void setScrollbarsVisible(bool);
    virtual bool scrollbarsVisible();
    virtual void setMenubarVisible(bool);
    virtual bool menubarVisible();
    virtual void setResizable(bool);
    virtual void addMessageToConsole(
        WebCore::MessageSource, WebCore::MessageLevel,
        const WTF::String& message, unsigned lineNumber,
        const WTF::String& sourceID);
    virtual bool canRunBeforeUnloadConfirmPanel();
    virtual bool runBeforeUnloadConfirmPanel(
        const WTF::String& message, WebCore::Frame*);
    virtual void closeWindowSoon();
    virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String&);
    virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&);
    virtual bool runJavaScriptPrompt(
        WebCore::Frame*, const WTF::String& message,
        const WTF::String& defaultValue, WTF::String& result);
    virtual void setStatusbarText(const WTF::String& message);
    virtual bool tabsToLinks();
    virtual WebCore::IntRect windowResizerRect() const;
    virtual void invalidateContentsAndRootView(const WebCore::IntRect&);
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&);
    virtual void scheduleAnimation();
    virtual void scroll(
        const WebCore::IntSize& scrollDelta, const WebCore::IntRect& rectToScroll,
        const WebCore::IntRect& clipRect);
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const;
    virtual WebScreenInfo screenInfo() const;
    virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const;
    virtual void deviceOrPageScaleFactorChanged() const;
    virtual void didProgrammaticallyScroll(WebCore::Frame*, const WebCore::IntPoint&) const;
    virtual void layoutUpdated(WebCore::Frame*) const;
    virtual void mouseDidMoveOverElement(
        const WebCore::HitTestResult& result, unsigned modifierFlags);
    virtual void setToolTip(const WTF::String& tooltipText, WebCore::TextDirection);
    virtual void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const;
    virtual void print(WebCore::Frame*);
    virtual void annotatedRegionsChanged();
    virtual bool paintCustomOverhangArea(WebCore::GraphicsContext*, const WebCore::IntRect&, const WebCore::IntRect&, const WebCore::IntRect&);
    virtual PassOwnPtr<WebCore::ColorChooser> createColorChooser(WebCore::ColorChooserClient*, const WebCore::Color&) OVERRIDE;
    PassOwnPtr<WebColorChooser> createWebColorChooser(WebColorChooserClient*, const WebColor&);
    virtual PassRefPtr<WebCore::DateTimeChooser> openDateTimeChooser(WebCore::DateTimeChooserClient*, const WebCore::DateTimeChooserParameters&) OVERRIDE;
    virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>);
    virtual void enumerateChosenDirectory(WebCore::FileChooser*);
    virtual void setCursor(const WebCore::Cursor&);
    virtual void formStateDidChange(const WebCore::Node*);
    virtual void needTouchEvents(bool needTouchEvents) OVERRIDE;

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() const OVERRIDE;

    // Pass 0 as the GraphicsLayer to detatch the root layer.
    virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*);

    // Sets a flag to specify that the view needs to be updated, so we need
    // to do an eager layout before the drawing.
    virtual void scheduleCompositingLayerFlush();

    virtual CompositingTriggerFlags allowedCompositingTriggers() const;

    virtual void enterFullScreenForElement(WebCore::Element*);
    virtual void exitFullScreenForElement(WebCore::Element*);

    // ChromeClient methods:
    virtual void popupOpened(WebCore::PopupContainer* popupContainer,
                             const WebCore::IntRect& bounds,
                             bool handleExternally);
    virtual void popupClosed(WebCore::PopupContainer* popupContainer);
    virtual void postAccessibilityNotification(WebCore::AccessibilityObject*, WebCore::AXObjectCache::AXNotification);
    virtual String acceptLanguages() OVERRIDE;

    // ChromeClientImpl:
    void setCursorForPlugin(const WebCursorInfo&);
    void setNewWindowNavigationPolicy(WebNavigationPolicy);

    virtual bool hasOpenedPopup() const OVERRIDE;
    virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::Frame&, WebCore::PopupMenuClient*) const;
    virtual WebCore::PagePopup* openPagePopup(WebCore::PagePopupClient*, const WebCore::IntRect&) OVERRIDE;
    virtual void closePagePopup(WebCore::PagePopup*) OVERRIDE;
    virtual void setPagePopupDriver(WebCore::PagePopupDriver*) OVERRIDE;
    virtual void resetPagePopupDriver() OVERRIDE;

    virtual bool isPasswordGenerationEnabled() const OVERRIDE;
    virtual void openPasswordGenerator(WebCore::HTMLInputElement*) OVERRIDE;

    virtual bool shouldRunModalDialogDuringPageDismissal(const DialogType&, const String& dialogMessage, WebCore::FrameLoader::PageDismissalType) const;

    virtual bool shouldRubberBandInDirection(WebCore::ScrollDirection) const;
    virtual void numWheelEventHandlersChanged(unsigned);

    virtual bool requestPointerLock();
    virtual void requestPointerUnlock();
    virtual bool isPointerLocked();

    virtual void didAssociateFormControls(const Vector<RefPtr<WebCore::Element> >&) OVERRIDE;

private:
    WebNavigationPolicy getNavigationPolicy();
    void getPopupMenuInfo(WebCore::PopupContainer*, WebPopupMenuInfo*);
    void setCursor(const WebCursorInfo&);

    WebViewImpl* m_webView;  // weak pointer
    bool m_toolbarsVisible;
    bool m_statusbarVisible;
    bool m_scrollbarsVisible;
    bool m_menubarVisible;
    bool m_resizable;

    WebCore::PagePopupDriver* m_pagePopupDriver;
};

#if ENABLE(NAVIGATOR_CONTENT_UTILS)
class NavigatorContentUtilsClientImpl : public WebCore::NavigatorContentUtilsClient {
public:
    static PassOwnPtr<NavigatorContentUtilsClientImpl> create(WebViewImpl*);
    ~NavigatorContentUtilsClientImpl() { }

    virtual void registerProtocolHandler(const String& scheme, const String& baseURL, const String& url, const String& title) OVERRIDE;

private:
    explicit NavigatorContentUtilsClientImpl(WebViewImpl*);

    WebViewImpl* m_webView;
};
#endif

} // namespace WebKit

#endif
