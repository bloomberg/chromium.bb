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

#include "core/page/ChromeClient.h"
#include "modules/navigatorcontentutils/NavigatorContentUtilsClient.h"
#include "platform/PopupMenu.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/WebColor.h"
#include "public/web/WebNavigationPolicy.h"
#include "wtf/PassOwnPtr.h"

namespace blink {
class AXObject;
class ColorChooser;
class ColorChooserClient;
class Element;
class Event;
class FileChooser;
class GraphicsLayerFactory;
class HTMLFormControlElement;
class HTMLInputElement;
class KeyboardEvent;
class PopupMenuClient;
class RenderBox;
class SecurityOrigin;
class DateTimeChooser;
class DateTimeChooserClient;
class WebColorChooser;
class WebColorChooserClient;
class WebViewImpl;
struct WebCursorInfo;
struct WebPopupMenuInfo;
struct WindowFeatures;

// Handles window-level notifications from WebCore on behalf of a WebView.
class ChromeClientImpl FINAL : public ChromeClient {
public:
    explicit ChromeClientImpl(WebViewImpl* webView);
    virtual ~ChromeClientImpl();

    virtual void* webView() const OVERRIDE;

    // ChromeClient methods:
    virtual void chromeDestroyed() OVERRIDE;
    virtual void setWindowRect(const FloatRect&) OVERRIDE;
    virtual FloatRect windowRect() OVERRIDE;
    virtual FloatRect pageRect() OVERRIDE;
    virtual void focus() OVERRIDE;
    virtual bool canTakeFocus(FocusType) OVERRIDE;
    virtual void takeFocus(FocusType) OVERRIDE;
    virtual void focusedNodeChanged(Node*) OVERRIDE;
    virtual void focusedFrameChanged(LocalFrame*) OVERRIDE;
    virtual Page* createWindow(
        LocalFrame*, const FrameLoadRequest&, const WindowFeatures&, NavigationPolicy, ShouldSendReferrer) OVERRIDE;
    virtual void show(NavigationPolicy) OVERRIDE;
    virtual bool canRunModal() OVERRIDE;
    virtual void runModal() OVERRIDE;
    virtual void setToolbarsVisible(bool) OVERRIDE;
    virtual bool toolbarsVisible() OVERRIDE;
    virtual void setStatusbarVisible(bool) OVERRIDE;
    virtual bool statusbarVisible() OVERRIDE;
    virtual void setScrollbarsVisible(bool) OVERRIDE;
    virtual bool scrollbarsVisible() OVERRIDE;
    virtual void setMenubarVisible(bool) OVERRIDE;
    virtual bool menubarVisible() OVERRIDE;
    virtual void setResizable(bool) OVERRIDE;
    virtual bool shouldReportDetailedMessageForSource(const WTF::String&) OVERRIDE;
    virtual void addMessageToConsole(
        LocalFrame*, MessageSource, MessageLevel,
        const WTF::String& message, unsigned lineNumber,
        const WTF::String& sourceID, const WTF::String& stackTrace) OVERRIDE;
    virtual bool canRunBeforeUnloadConfirmPanel() OVERRIDE;
    virtual bool runBeforeUnloadConfirmPanel(
        const WTF::String& message, LocalFrame*) OVERRIDE;
    virtual void closeWindowSoon() OVERRIDE;
    virtual void runJavaScriptAlert(LocalFrame*, const WTF::String&) OVERRIDE;
    virtual bool runJavaScriptConfirm(LocalFrame*, const WTF::String&) OVERRIDE;
    virtual bool runJavaScriptPrompt(
        LocalFrame*, const WTF::String& message,
        const WTF::String& defaultValue, WTF::String& result) OVERRIDE;
    virtual void setStatusbarText(const WTF::String& message) OVERRIDE;
    virtual bool tabsToLinks() OVERRIDE;
    virtual IntRect windowResizerRect() const OVERRIDE;
    virtual void invalidateContentsAndRootView(const IntRect&) OVERRIDE;
    virtual void invalidateContentsForSlowScroll(const IntRect&) OVERRIDE;
    virtual void scheduleAnimation() OVERRIDE;
    virtual IntRect rootViewToScreen(const IntRect&) const OVERRIDE;
    virtual WebScreenInfo screenInfo() const OVERRIDE;
    virtual void contentsSizeChanged(LocalFrame*, const IntSize&) const OVERRIDE;
    virtual void deviceOrPageScaleFactorChanged() const OVERRIDE;
    virtual void layoutUpdated(LocalFrame*) const OVERRIDE;
    virtual void mouseDidMoveOverElement(
        const HitTestResult&, unsigned modifierFlags) OVERRIDE;
    virtual void setToolTip(const WTF::String& tooltipText, TextDirection) OVERRIDE;
    virtual void dispatchViewportPropertiesDidChange(const ViewportDescription&) const OVERRIDE;
    virtual void print(LocalFrame*) OVERRIDE;
    virtual void annotatedRegionsChanged() OVERRIDE;
    virtual bool paintCustomOverhangArea(GraphicsContext*, const IntRect&, const IntRect&, const IntRect&) OVERRIDE;
    virtual PassOwnPtr<ColorChooser> createColorChooser(LocalFrame*, ColorChooserClient*, const Color&) OVERRIDE;
    virtual PassRefPtrWillBeRawPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) OVERRIDE;
    virtual void openTextDataListChooser(HTMLInputElement&) OVERRIDE;
    virtual void runOpenPanel(LocalFrame*, PassRefPtr<FileChooser>) OVERRIDE;
    virtual void enumerateChosenDirectory(FileChooser*) OVERRIDE;
    virtual void setCursor(const Cursor&) OVERRIDE;
    virtual void needTouchEvents(bool needTouchEvents) OVERRIDE;
    virtual void setTouchAction(TouchAction) OVERRIDE;

    virtual GraphicsLayerFactory* graphicsLayerFactory() const OVERRIDE;

    // Pass 0 as the GraphicsLayer to detatch the root layer.
    virtual void attachRootGraphicsLayer(GraphicsLayer*) OVERRIDE;

    virtual void enterFullScreenForElement(Element*) OVERRIDE;
    virtual void exitFullScreenForElement(Element*) OVERRIDE;

    virtual void clearCompositedSelectionBounds() OVERRIDE;

    // ChromeClient methods:
    virtual void postAccessibilityNotification(AXObject*, AXObjectCache::AXNotification) OVERRIDE;
    virtual String acceptLanguages() OVERRIDE;

    // ChromeClientImpl:
    void setCursorForPlugin(const WebCursorInfo&);
    void setNewWindowNavigationPolicy(WebNavigationPolicy);

    virtual bool hasOpenedPopup() const OVERRIDE;
    virtual PassRefPtr<PopupMenu> createPopupMenu(LocalFrame&, PopupMenuClient*) const OVERRIDE;
    PagePopup* openPagePopup(PagePopupClient*, const IntRect&);
    void closePagePopup(PagePopup*);
    virtual void setPagePopupDriver(PagePopupDriver*) OVERRIDE;
    virtual void resetPagePopupDriver() OVERRIDE;

    virtual bool shouldRunModalDialogDuringPageDismissal(const DialogType&, const String& dialogMessage, Document::PageDismissalType) const OVERRIDE;

    virtual bool requestPointerLock() OVERRIDE;
    virtual void requestPointerUnlock() OVERRIDE;

    virtual void didAssociateFormControls(const WillBeHeapVector<RefPtrWillBeMember<Element> >&) OVERRIDE;
    virtual void didChangeValueInTextField(HTMLFormControlElement&) OVERRIDE;
    virtual void didEndEditingOnTextField(HTMLInputElement&) OVERRIDE;
    virtual void handleKeyboardEventOnTextField(HTMLInputElement&, KeyboardEvent&) OVERRIDE;

    // FIXME: Remove this method once we have input routing in the browser
    // process. See http://crbug.com/339659.
    virtual void forwardInputEvent(Frame*, Event*) OVERRIDE;

    virtual void didCancelCompositionOnSelectionChange() OVERRIDE;
    virtual void willSetInputMethodState() OVERRIDE;
    virtual void didUpdateTextOfFocusedElementByNonUserInput() OVERRIDE;
    virtual void showImeIfNeeded() OVERRIDE;

private:
    virtual bool isChromeClientImpl() const OVERRIDE { return true; }

    WebNavigationPolicy getNavigationPolicy();
    void setCursor(const WebCursorInfo&);

    WebViewImpl* m_webView;  // weak pointer
    bool m_toolbarsVisible;
    bool m_statusbarVisible;
    bool m_scrollbarsVisible;
    bool m_menubarVisible;
    bool m_resizable;

    PagePopupDriver* m_pagePopupDriver;
};

DEFINE_TYPE_CASTS(ChromeClientImpl, ChromeClient, client, client->isChromeClientImpl(), client.isChromeClientImpl());

} // namespace blink

#endif
