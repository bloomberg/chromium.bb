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
class ChromeClientImpl final : public ChromeClient {
public:
    explicit ChromeClientImpl(WebViewImpl* webView);
    virtual ~ChromeClientImpl();

    virtual void* webView() const override;

    // ChromeClient methods:
    virtual void chromeDestroyed() override;
    virtual void setWindowRect(const FloatRect&) override;
    virtual FloatRect windowRect() override;
    virtual FloatRect pageRect() override;
    virtual void focus() override;
    virtual bool canTakeFocus(FocusType) override;
    virtual void takeFocus(FocusType) override;
    virtual void focusedNodeChanged(Node*) override;
    virtual void focusedFrameChanged(LocalFrame*) override;
    virtual Page* createWindow(
        LocalFrame*, const FrameLoadRequest&, const WindowFeatures&, NavigationPolicy, ShouldSendReferrer) override;
    virtual void show(NavigationPolicy) override;
    virtual bool canRunModal() override;
    virtual void runModal() override;
    virtual void setToolbarsVisible(bool) override;
    virtual bool toolbarsVisible() override;
    virtual void setStatusbarVisible(bool) override;
    virtual bool statusbarVisible() override;
    virtual void setScrollbarsVisible(bool) override;
    virtual bool scrollbarsVisible() override;
    virtual void setMenubarVisible(bool) override;
    virtual bool menubarVisible() override;
    virtual void setResizable(bool) override;
    virtual bool shouldReportDetailedMessageForSource(const WTF::String&) override;
    virtual void addMessageToConsole(
        LocalFrame*, MessageSource, MessageLevel,
        const WTF::String& message, unsigned lineNumber,
        const WTF::String& sourceID, const WTF::String& stackTrace) override;
    virtual bool canRunBeforeUnloadConfirmPanel() override;
    virtual bool runBeforeUnloadConfirmPanel(
        const WTF::String& message, LocalFrame*) override;
    virtual void closeWindowSoon() override;
    virtual void runJavaScriptAlert(LocalFrame*, const WTF::String&) override;
    virtual bool runJavaScriptConfirm(LocalFrame*, const WTF::String&) override;
    virtual bool runJavaScriptPrompt(
        LocalFrame*, const WTF::String& message,
        const WTF::String& defaultValue, WTF::String& result) override;
    virtual void setStatusbarText(const WTF::String& message) override;
    virtual bool tabsToLinks() override;
    virtual IntRect windowResizerRect() const override;
    virtual void invalidateContentsAndRootView(const IntRect&) override;
    virtual void invalidateContentsForSlowScroll(const IntRect&) override;
    virtual void scheduleAnimation() override;
    virtual IntRect rootViewToScreen(const IntRect&) const override;
    virtual WebScreenInfo screenInfo() const override;
    virtual void contentsSizeChanged(LocalFrame*, const IntSize&) const override;
    virtual void deviceOrPageScaleFactorChanged() const override;
    virtual void layoutUpdated(LocalFrame*) const override;
    virtual void mouseDidMoveOverElement(
        const HitTestResult&, unsigned modifierFlags) override;
    virtual void setToolTip(const WTF::String& tooltipText, TextDirection) override;
    virtual void dispatchViewportPropertiesDidChange(const ViewportDescription&) const override;
    virtual void print(LocalFrame*) override;
    virtual void annotatedRegionsChanged() override;
    virtual bool paintCustomOverhangArea(GraphicsContext*, const IntRect&, const IntRect&, const IntRect&) override;
    virtual PassOwnPtr<ColorChooser> createColorChooser(LocalFrame*, ColorChooserClient*, const Color&) override;
    virtual PassRefPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) override;
    virtual void openTextDataListChooser(HTMLInputElement&) override;
    virtual void runOpenPanel(LocalFrame*, PassRefPtr<FileChooser>) override;
    virtual void enumerateChosenDirectory(FileChooser*) override;
    virtual void setCursor(const Cursor&) override;
    virtual void needTouchEvents(bool needTouchEvents) override;
    virtual void setTouchAction(TouchAction) override;

    virtual GraphicsLayerFactory* graphicsLayerFactory() const override;

    // Pass 0 as the GraphicsLayer to detatch the root layer.
    virtual void attachRootGraphicsLayer(GraphicsLayer*) override;

    virtual void enterFullScreenForElement(Element*) override;
    virtual void exitFullScreenForElement(Element*) override;

    virtual void clearCompositedSelectionBounds() override;
    virtual void updateCompositedSelectionBounds(const blink::CompositedSelectionBound& anchor, const blink::CompositedSelectionBound& focus) override;

    // ChromeClient methods:
    virtual void postAccessibilityNotification(AXObject*, AXObjectCache::AXNotification) override;
    virtual String acceptLanguages() override;

    // ChromeClientImpl:
    void setCursorForPlugin(const WebCursorInfo&);
    void setNewWindowNavigationPolicy(WebNavigationPolicy);

    virtual bool hasOpenedPopup() const override;
    virtual PassRefPtrWillBeRawPtr<PopupMenu> createPopupMenu(LocalFrame&, PopupMenuClient*) const override;
    PagePopup* openPagePopup(PagePopupClient*, const IntRect&);
    void closePagePopup(PagePopup*);
    virtual void setPagePopupDriver(PagePopupDriver*) override;
    virtual void resetPagePopupDriver() override;
    virtual PagePopupDriver* pagePopupDriver() const override { return m_pagePopupDriver; }

    virtual bool shouldRunModalDialogDuringPageDismissal(const DialogType&, const String& dialogMessage, Document::PageDismissalType) const override;

    virtual bool requestPointerLock() override;
    virtual void requestPointerUnlock() override;

    virtual void didAssociateFormControls(const WillBeHeapVector<RefPtrWillBeMember<Element> >&) override;
    virtual void didChangeValueInTextField(HTMLFormControlElement&) override;
    virtual void didEndEditingOnTextField(HTMLInputElement&) override;
    virtual void handleKeyboardEventOnTextField(HTMLInputElement&, KeyboardEvent&) override;

    // FIXME: Remove this method once we have input routing in the browser
    // process. See http://crbug.com/339659.
    virtual void forwardInputEvent(Frame*, Event*) override;

    virtual void didCancelCompositionOnSelectionChange() override;
    virtual void willSetInputMethodState() override;
    virtual void didUpdateTextOfFocusedElementByNonUserInput() override;
    virtual void showImeIfNeeded() override;

private:
    virtual bool isChromeClientImpl() const override { return true; }

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
