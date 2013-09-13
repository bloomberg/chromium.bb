/*
 * Copyright (C) 2006 Eric Seidel (eric@webkit.org)
 * Copyright (C) 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef EmptyClients_h
#define EmptyClients_h

#include "core/dom/DeviceOrientationClient.h"
#include "core/history/BackForwardClient.h"
#include "core/inspector/InspectorClient.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/ChromeClient.h"
#include "core/page/ContextMenuClient.h"
#include "core/page/DragClient.h"
#include "core/page/EditorClient.h"
#include "core/page/FocusDirection.h"
#include "core/page/Page.h"
#include "core/platform/DragImage.h"
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/network/ResourceError.h"
#include "core/platform/text/TextCheckerClient.h"
#include "public/platform/WebScreenInfo.h"
#include "wtf/Forward.h"

#include <v8.h>

/*
 This file holds empty Client stubs for use by WebCore.
 Viewless element needs to create a dummy Page->Frame->FrameView tree for use in parsing or executing JavaScript.
 This tree depends heavily on Clients (usually provided by WebKit classes).

 This file was first created for SVGImage as it had no way to access the current Page (nor should it,
 since Images are not tied to a page).
 See http://bugs.webkit.org/show_bug.cgi?id=5971 for the original discussion about this file.

 Ideally, whenever you change a Client class, you should add a stub here.
 Brittle, yes.  Unfortunate, yes.  Hopefully temporary.
*/

namespace WebCore {

class GraphicsContext3D;

class EmptyChromeClient : public ChromeClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~EmptyChromeClient() { }
    virtual void chromeDestroyed() OVERRIDE { }

    virtual void* webView() const OVERRIDE { return 0; }
    virtual void setWindowRect(const FloatRect&) OVERRIDE { }
    virtual FloatRect windowRect() OVERRIDE { return FloatRect(); }

    virtual FloatRect pageRect() OVERRIDE { return FloatRect(); }

    virtual void focus() OVERRIDE { }
    virtual void unfocus() OVERRIDE { }

    virtual bool canTakeFocus(FocusDirection) OVERRIDE { return false; }
    virtual void takeFocus(FocusDirection) OVERRIDE { }

    virtual void focusedNodeChanged(Node*) OVERRIDE { }
    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, NavigationPolicy) OVERRIDE { return 0; }
    virtual void show(NavigationPolicy) OVERRIDE { }

    virtual bool canRunModal() OVERRIDE { return false; }
    virtual void runModal() OVERRIDE { }

    virtual void setToolbarsVisible(bool) OVERRIDE { }
    virtual bool toolbarsVisible() OVERRIDE { return false; }

    virtual void setStatusbarVisible(bool) OVERRIDE { }
    virtual bool statusbarVisible() OVERRIDE { return false; }

    virtual void setScrollbarsVisible(bool) OVERRIDE { }
    virtual bool scrollbarsVisible() OVERRIDE { return false; }

    virtual void setMenubarVisible(bool) OVERRIDE { }
    virtual bool menubarVisible() OVERRIDE { return false; }

    virtual void setResizable(bool) OVERRIDE { }

    virtual bool shouldReportDetailedMessageForSource(const String&) OVERRIDE { return false; }
    virtual void addMessageToConsole(MessageSource, MessageLevel, const String&, unsigned, const String&, const String&) OVERRIDE { }

    virtual bool canRunBeforeUnloadConfirmPanel() OVERRIDE { return false; }
    virtual bool runBeforeUnloadConfirmPanel(const String&, Frame*) OVERRIDE { return true; }

    virtual void closeWindowSoon() OVERRIDE { }

    virtual void runJavaScriptAlert(Frame*, const String&) OVERRIDE { }
    virtual bool runJavaScriptConfirm(Frame*, const String&) OVERRIDE { return false; }
    virtual bool runJavaScriptPrompt(Frame*, const String&, const String&, String&) OVERRIDE { return false; }

    virtual bool hasOpenedPopup() const OVERRIDE { return false; }
    virtual PassRefPtr<PopupMenu> createPopupMenu(Frame&, PopupMenuClient*) const OVERRIDE;
    virtual PagePopup* openPagePopup(PagePopupClient*, const IntRect&) OVERRIDE { return 0; }
    virtual void closePagePopup(PagePopup*) OVERRIDE { }
    virtual void setPagePopupDriver(PagePopupDriver*) OVERRIDE { }
    virtual void resetPagePopupDriver() OVERRIDE { }

    virtual void setStatusbarText(const String&) OVERRIDE { }

    virtual bool tabsToLinks() OVERRIDE { return false; }

    virtual IntRect windowResizerRect() const OVERRIDE { return IntRect(); }

    virtual void invalidateContentsAndRootView(const IntRect&) OVERRIDE { }
    virtual void invalidateContentsForSlowScroll(const IntRect&) OVERRIDE { }
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) OVERRIDE { }
    virtual void scheduleAnimation() OVERRIDE { }

    virtual IntPoint screenToRootView(const IntPoint& p) const OVERRIDE { return p; }
    virtual IntRect rootViewToScreen(const IntRect& r) const OVERRIDE { return r; }
    virtual WebKit::WebScreenInfo screenInfo() const OVERRIDE { return WebKit::WebScreenInfo(); }
    virtual void contentsSizeChanged(Frame*, const IntSize&) const OVERRIDE { }

    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned) OVERRIDE { }

    virtual void setToolTip(const String&, TextDirection) OVERRIDE { }

    virtual void print(Frame*) OVERRIDE { }

    virtual void enumerateChosenDirectory(FileChooser*) OVERRIDE { }

    virtual PassOwnPtr<ColorChooser> createColorChooser(ColorChooserClient*, const Color&) OVERRIDE;

    virtual PassRefPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) OVERRIDE;

    virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>) OVERRIDE;

    virtual void formStateDidChange(const Node*) OVERRIDE { }

    virtual void setCursor(const Cursor&) OVERRIDE { }

    virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*) OVERRIDE { }
    virtual void scheduleCompositingLayerFlush() OVERRIDE { }

    virtual void needTouchEvents(bool) OVERRIDE { }

    virtual void numWheelEventHandlersChanged(unsigned) OVERRIDE { }

    virtual bool shouldRubberBandInDirection(WebCore::ScrollDirection) const OVERRIDE { return false; }

    virtual bool isEmptyChromeClient() const OVERRIDE { return true; }

    virtual void didAssociateFormControls(const Vector<RefPtr<Element> >&) OVERRIDE { }

    virtual void popupOpened(PopupContainer* popupContainer, const IntRect& bounds,
                             bool handleExternal) { }
    virtual void popupClosed(PopupContainer* popupContainer) OVERRIDE { }

    virtual void annotatedRegionsChanged() OVERRIDE { }
    virtual bool paintCustomOverhangArea(GraphicsContext*, const IntRect&, const IntRect&, const IntRect&) OVERRIDE { return false; }
    virtual String acceptLanguages() OVERRIDE;
};

class EmptyFrameLoaderClient : public FrameLoaderClient {
    WTF_MAKE_NONCOPYABLE(EmptyFrameLoaderClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyFrameLoaderClient() { }
    virtual ~EmptyFrameLoaderClient() {  }
    virtual void frameLoaderDestroyed() OVERRIDE { }

    virtual bool hasWebView() const OVERRIDE { return true; } // mainly for assertions

    virtual void detachedFromParent() OVERRIDE { }

    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&) OVERRIDE { }
    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse&) OVERRIDE { }
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long) OVERRIDE { }
    virtual void dispatchDidLoadResourceFromMemoryCache(const ResourceRequest&, const ResourceResponse&) OVERRIDE { }

    virtual void dispatchDidHandleOnloadEvents() OVERRIDE { }
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() OVERRIDE { }
    virtual void dispatchWillClose() OVERRIDE { }
    virtual void dispatchDidStartProvisionalLoad() OVERRIDE { }
    virtual void dispatchDidReceiveTitle(const String&) OVERRIDE { }
    virtual void dispatchDidChangeIcons(IconType) OVERRIDE { }
    virtual void dispatchDidCommitLoad() OVERRIDE { }
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&) OVERRIDE { }
    virtual void dispatchDidFailLoad(const ResourceError&) OVERRIDE { }
    virtual void dispatchDidFinishDocumentLoad() OVERRIDE { }
    virtual void dispatchDidFinishLoad() OVERRIDE { }
    virtual void dispatchDidLayout(LayoutMilestones) OVERRIDE { }

    virtual NavigationPolicy decidePolicyForNavigation(const ResourceRequest&, DocumentLoader*, NavigationPolicy) OVERRIDE;

    virtual void dispatchWillSendSubmitEvent(PassRefPtr<FormState>) OVERRIDE;
    virtual void dispatchWillSubmitForm(PassRefPtr<FormState>) OVERRIDE;

    virtual void postProgressStartedNotification() OVERRIDE { }
    virtual void postProgressEstimateChangedNotification() OVERRIDE { }
    virtual void postProgressFinishedNotification() OVERRIDE { }

    virtual void loadURLExternally(const ResourceRequest&, NavigationPolicy, const String& = String()) OVERRIDE { }

    virtual PassRefPtr<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&) OVERRIDE;

    virtual String userAgent(const KURL&) OVERRIDE { return ""; }

    virtual String doNotTrackValue() OVERRIDE { return String(); }

    virtual void transitionToCommittedForNewPage() OVERRIDE { }

    virtual void navigateBackForward(int offset) const OVERRIDE { }
    virtual void didDisplayInsecureContent() OVERRIDE { }
    virtual void didRunInsecureContent(SecurityOrigin*, const KURL&) OVERRIDE { }
    virtual void didDetectXSS(const KURL&, bool) OVERRIDE { }
    virtual void didDispatchPingLoader(const KURL&) OVERRIDE { }
    virtual PassRefPtr<Frame> createFrame(const KURL&, const String&, const String&, HTMLFrameOwnerElement*) OVERRIDE;
    virtual PassRefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool) OVERRIDE;
    virtual PassRefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL&, const Vector<String>&, const Vector<String>&) OVERRIDE;

    virtual ObjectContentType objectContentType(const KURL&, const String&, bool) OVERRIDE { return ObjectContentType(); }

    virtual void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld*) OVERRIDE { }
    virtual void documentElementAvailable() OVERRIDE { }

    virtual void didCreateScriptContext(v8::Handle<v8::Context>, int extensionGroup, int worldId) OVERRIDE { }
    virtual void willReleaseScriptContext(v8::Handle<v8::Context>, int worldId) OVERRIDE { }
    virtual bool allowScriptExtension(const String& extensionName, int extensionGroup, int worldId) OVERRIDE { return false; }

    virtual WebKit::WebCookieJar* cookieJar() const { return 0; }

    virtual void didRequestAutocomplete(PassRefPtr<FormState>) OVERRIDE;
    virtual WebKit::WebNavigationControllerRegistry* navigationControllerRegistry() OVERRIDE { return 0; }
};

class EmptyTextCheckerClient : public TextCheckerClient {
public:
    virtual bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const OVERRIDE { return true; }
    virtual void checkSpellingOfString(const String&, int*, int*) OVERRIDE { }
    virtual String getAutoCorrectSuggestionForMisspelledWord(const String&) OVERRIDE { return String(); }
    virtual void checkGrammarOfString(const String&, Vector<GrammarDetail>&, int*, int*) OVERRIDE { }
    virtual void requestCheckingOfString(PassRefPtr<TextCheckingRequest>) OVERRIDE;
};

class EmptyEditorClient : public EditorClient {
    WTF_MAKE_NONCOPYABLE(EmptyEditorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyEditorClient() { }
    virtual ~EmptyEditorClient() { }

    virtual bool shouldDeleteRange(Range*) OVERRIDE { return false; }
    virtual bool smartInsertDeleteEnabled() OVERRIDE { return false; }
    virtual bool isSelectTrailingWhitespaceEnabled() OVERRIDE { return false; }
    virtual bool isContinuousSpellCheckingEnabled() OVERRIDE { return false; }
    virtual void toggleContinuousSpellChecking() OVERRIDE { }
    virtual bool isGrammarCheckingEnabled() OVERRIDE { return false; }

    virtual bool shouldBeginEditing(Range*) OVERRIDE { return false; }
    virtual bool shouldEndEditing(Range*) OVERRIDE { return false; }
    virtual bool shouldInsertNode(Node*, Range*, EditorInsertAction) OVERRIDE { return false; }
    virtual bool shouldInsertText(const String&, Range*, EditorInsertAction) OVERRIDE { return false; }
    virtual bool shouldChangeSelectedRange(Range*, Range*, EAffinity, bool) OVERRIDE { return false; }

    virtual bool shouldApplyStyle(StylePropertySet*, Range*) OVERRIDE { return false; }

    virtual void didBeginEditing() OVERRIDE { }
    virtual void respondToChangedContents() OVERRIDE { }
    virtual void respondToChangedSelection(Frame*) OVERRIDE { }
    virtual void didEndEditing() OVERRIDE { }
    virtual void didCancelCompositionOnSelectionChange() OVERRIDE { }

    virtual void registerUndoStep(PassRefPtr<UndoStep>) OVERRIDE;
    virtual void registerRedoStep(PassRefPtr<UndoStep>) OVERRIDE;
    virtual void clearUndoRedoOperations() OVERRIDE { }

    virtual bool canCopyCut(Frame*, bool defaultValue) const OVERRIDE { return defaultValue; }
    virtual bool canPaste(Frame*, bool defaultValue) const OVERRIDE { return defaultValue; }
    virtual bool canUndo() const OVERRIDE { return false; }
    virtual bool canRedo() const OVERRIDE { return false; }

    virtual void undo() OVERRIDE { }
    virtual void redo() OVERRIDE { }

    virtual void handleKeyboardEvent(KeyboardEvent*) OVERRIDE { }

    virtual void textFieldDidEndEditing(Element*) OVERRIDE { }
    virtual void textDidChangeInTextField(Element*) OVERRIDE { }
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) OVERRIDE { return false; }

    TextCheckerClient& textChecker() { return m_textCheckerClient; }

    virtual void updateSpellingUIWithMisspelledWord(const String&) OVERRIDE { }
    virtual void showSpellingUI(bool) OVERRIDE { }
    virtual bool spellingUIIsShowing() OVERRIDE { return false; }

    virtual void willSetInputMethodState() OVERRIDE { }

private:
    EmptyTextCheckerClient m_textCheckerClient;
};

class EmptyContextMenuClient : public ContextMenuClient {
    WTF_MAKE_NONCOPYABLE(EmptyContextMenuClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyContextMenuClient() { }
    virtual ~EmptyContextMenuClient() {  }
    virtual void showContextMenu(const ContextMenu*) OVERRIDE { }
    virtual void clearContextMenu() OVERRIDE { }
};

class EmptyDragClient : public DragClient {
    WTF_MAKE_NONCOPYABLE(EmptyDragClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyDragClient() { }
    virtual ~EmptyDragClient() {}
    virtual DragDestinationAction actionMaskForDrag(DragData*) OVERRIDE { return DragDestinationActionNone; }
    virtual void startDrag(DragImage*, const IntPoint&, const IntPoint&, Clipboard*, Frame*, bool) OVERRIDE { }
};

class EmptyInspectorClient : public InspectorClient {
    WTF_MAKE_NONCOPYABLE(EmptyInspectorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyInspectorClient() { }
    virtual ~EmptyInspectorClient() { }

    virtual void highlight() OVERRIDE { }
    virtual void hideHighlight() OVERRIDE { }
};

class EmptyDeviceClient : public DeviceClient {
public:
    virtual void startUpdating() OVERRIDE { }
    virtual void stopUpdating() OVERRIDE { }
};

class EmptyDeviceOrientationClient : public DeviceOrientationClient {
public:
    virtual void setController(DeviceOrientationController*) OVERRIDE { }
    virtual DeviceOrientationData* lastOrientation() const OVERRIDE { return 0; }
    virtual void deviceOrientationControllerDestroyed() OVERRIDE { }
};

class EmptyBackForwardClient : public BackForwardClient {
public:
    virtual void didAddItem() OVERRIDE { }
    virtual int backListCount() OVERRIDE { return 0; }
    virtual int forwardListCount() OVERRIDE { return 0; }
};

void fillWithEmptyClients(Page::PageClients&);

}

#endif // EmptyClients_h
