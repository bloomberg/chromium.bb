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
#include "core/platform/graphics/FloatRect.h"
#include "core/platform/network/ResourceError.h"
#include "core/platform/text/TextCheckerClient.h"
#include "modules/device_orientation/DeviceMotionClient.h"

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
    virtual void chromeDestroyed() { }

    virtual void* webView() const { return 0; }
    virtual void setWindowRect(const FloatRect&) { }
    virtual FloatRect windowRect() { return FloatRect(); }

    virtual FloatRect pageRect() { return FloatRect(); }

    virtual void focus() { }
    virtual void unfocus() { }

    virtual bool canTakeFocus(FocusDirection) { return false; }
    virtual void takeFocus(FocusDirection) { }

    virtual void focusedNodeChanged(Node*) { }
    virtual void focusedFrameChanged(Frame*) { }

    virtual Page* createWindow(Frame*, const FrameLoadRequest&, const WindowFeatures&, const NavigationAction&) { return 0; }
    virtual void show() { }

    virtual bool canRunModal() { return false; }
    virtual void runModal() { }

    virtual void setToolbarsVisible(bool) { }
    virtual bool toolbarsVisible() { return false; }

    virtual void setStatusbarVisible(bool) { }
    virtual bool statusbarVisible() { return false; }

    virtual void setScrollbarsVisible(bool) { }
    virtual bool scrollbarsVisible() { return false; }

    virtual void setMenubarVisible(bool) { }
    virtual bool menubarVisible() { return false; }

    virtual void setResizable(bool) { }

    virtual void addMessageToConsole(MessageSource, MessageLevel, const String&, unsigned, const String&) { }

    virtual bool canRunBeforeUnloadConfirmPanel() { return false; }
    virtual bool runBeforeUnloadConfirmPanel(const String&, Frame*) { return true; }

    virtual void closeWindowSoon() { }

    virtual void runJavaScriptAlert(Frame*, const String&) { }
    virtual bool runJavaScriptConfirm(Frame*, const String&) { return false; }
    virtual bool runJavaScriptPrompt(Frame*, const String&, const String&, String&) { return false; }
    virtual bool shouldInterruptJavaScript() { return false; }

    virtual bool selectItemWritingDirectionIsNatural() { return false; }
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() { return false; }
    virtual bool hasOpenedPopup() const OVERRIDE { return false; }
    virtual PassRefPtr<PopupMenu> createPopupMenu(PopupMenuClient*) const OVERRIDE;
    virtual PassRefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient*) const OVERRIDE;
#if ENABLE(PAGE_POPUP)
    virtual PagePopup* openPagePopup(PagePopupClient*, const IntRect&) OVERRIDE { return 0; }
    virtual void closePagePopup(PagePopup*) OVERRIDE { }
    virtual void setPagePopupDriver(PagePopupDriver*) OVERRIDE { }
    virtual void resetPagePopupDriver() OVERRIDE { }
#endif

    virtual void setStatusbarText(const String&) { }

    virtual KeyboardUIMode keyboardUIMode() { return KeyboardAccessDefault; }

    virtual IntRect windowResizerRect() const { return IntRect(); }

    virtual void invalidateRootView(const IntRect&) OVERRIDE { }
    virtual void invalidateContentsAndRootView(const IntRect&) OVERRIDE { }
    virtual void invalidateContentsForSlowScroll(const IntRect&) OVERRIDE { }
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) { }
    virtual void scheduleAnimation() { }

    virtual IntPoint screenToRootView(const IntPoint& p) const OVERRIDE { return p; }
    virtual IntRect rootViewToScreen(const IntRect& r) const OVERRIDE { return r; }
    virtual PlatformPageClient platformPageClient() const { return 0; }
    virtual void contentsSizeChanged(Frame*, const IntSize&) const { }

    virtual void scrollbarsModeDidChange() const { }
    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned) { }

    virtual void setToolTip(const String&, TextDirection) { }

    virtual void print(Frame*) { }

    virtual void exceededDatabaseQuota(Frame*, const String&, DatabaseDetails) { }

    virtual void reachedMaxAppCacheSize(int64_t) { }
    virtual void reachedApplicationCacheOriginQuota(SecurityOrigin*, int64_t) { }

    virtual void enumerateChosenDirectory(FileChooser*) { }

#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassOwnPtr<ColorChooser> createColorChooser(ColorChooserClient*, const Color&) OVERRIDE;
#endif

    virtual PassRefPtr<DateTimeChooser> openDateTimeChooser(DateTimeChooserClient*, const DateTimeChooserParameters&) OVERRIDE;

    virtual void runOpenPanel(Frame*, PassRefPtr<FileChooser>) OVERRIDE;
    virtual void loadIconForFiles(const Vector<String>&, FileIconLoader*) { }

    virtual void formStateDidChange(const Node*) { }

    virtual void elementDidFocus(const Node*) { }
    virtual void elementDidBlur(const Node*) { }

    virtual void setCursor(const Cursor&) { }
    virtual void setCursorHiddenUntilMouseMoves(bool) { }

    virtual void scrollRectIntoView(const IntRect&) const { }

    virtual void attachRootGraphicsLayer(Frame*, GraphicsLayer*) { }
    virtual void scheduleCompositingLayerFlush() { }

    virtual void needTouchEvents(bool) { }
    
    virtual void numWheelEventHandlersChanged(unsigned) OVERRIDE { }
    
    virtual bool shouldRubberBandInDirection(WebCore::ScrollDirection) const { return false; }
    
    virtual bool isEmptyChromeClient() const { return true; }

    virtual void didAssociateFormControls(const Vector<RefPtr<Element> >&) { }
    virtual bool shouldNotifyOnFormChanges() { return false; }

    virtual void popupOpened(PopupContainer* popupContainer, const IntRect& bounds,
                             bool handleExternal) { }
    virtual void popupClosed(PopupContainer* popupContainer) { }
};

class EmptyFrameLoaderClient : public FrameLoaderClient {
    WTF_MAKE_NONCOPYABLE(EmptyFrameLoaderClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyFrameLoaderClient() { }
    virtual ~EmptyFrameLoaderClient() {  }
    virtual void frameLoaderDestroyed() { }

    virtual bool hasWebView() const { return true; } // mainly for assertions

    virtual void detachedFromParent() { }

    virtual void dispatchWillSendRequest(DocumentLoader*, unsigned long, ResourceRequest&, const ResourceResponse&) { }
    virtual void dispatchDidReceiveResponse(DocumentLoader*, unsigned long, const ResourceResponse&) { }
    virtual void dispatchDidFinishLoading(DocumentLoader*, unsigned long) { }
    virtual void dispatchDidFailLoading(DocumentLoader*, unsigned long, const ResourceError&) { }
    virtual void dispatchDidLoadResourceFromMemoryCache(DocumentLoader*, const ResourceRequest&, const ResourceResponse&, int) { }

    virtual void dispatchDidHandleOnloadEvents() { }
    virtual void dispatchDidReceiveServerRedirectForProvisionalLoad() { }
    virtual void dispatchDidCancelClientRedirect() { }
    virtual void dispatchWillPerformClientRedirect(const KURL&, double, double) { }
    virtual void dispatchDidChangeLocationWithinPage() { }
    virtual void dispatchWillClose() { }
    virtual void dispatchDidStartProvisionalLoad() { }
    virtual void dispatchDidReceiveTitle(const StringWithDirection&) { }
    virtual void dispatchDidChangeIcons(IconType) { }
    virtual void dispatchDidCommitLoad() { }
    virtual void dispatchDidFailProvisionalLoad(const ResourceError&) { }
    virtual void dispatchDidFailLoad(const ResourceError&) { }
    virtual void dispatchDidFinishDocumentLoad() { }
    virtual void dispatchDidFinishLoad() { }
    virtual void dispatchDidLayout(LayoutMilestones) { }

    virtual Frame* dispatchCreatePage(const NavigationAction&) { return 0; }
    virtual void dispatchShow() { }

    virtual PolicyAction policyForNewWindowAction(const NavigationAction&, const String&) OVERRIDE;
    virtual PolicyAction decidePolicyForNavigationAction(const NavigationAction&, const ResourceRequest&) OVERRIDE;

    virtual void dispatchUnableToImplementPolicy(const ResourceError&) { }

    virtual void dispatchWillSendSubmitEvent(PassRefPtr<FormState>) OVERRIDE;
    virtual void dispatchWillSubmitForm(PassRefPtr<FormState>) OVERRIDE;

    virtual void setMainDocumentError(DocumentLoader*, const ResourceError&) { }

    virtual void postProgressStartedNotification() { }
    virtual void postProgressEstimateChangedNotification() { }
    virtual void postProgressFinishedNotification() { }

    virtual void startDownload(const ResourceRequest&, const String& suggestedName = String()) { UNUSED_PARAM(suggestedName); }

    virtual void committedLoad(DocumentLoader*, const char*, int) { }
    virtual void finishedLoading(DocumentLoader*) { }

    virtual ResourceError cancelledError(const ResourceRequest&) { ResourceError error("", 0, "", ""); error.setIsCancellation(true); return error; }
    virtual ResourceError cannotShowURLError(const ResourceRequest&) { return ResourceError("", 0, "", ""); }
    virtual ResourceError interruptedForPolicyChangeError(const ResourceRequest&) { return ResourceError("", 0, "", ""); }

    virtual ResourceError cannotShowMIMETypeError(const ResourceResponse&) { return ResourceError("", 0, "", ""); }
    virtual ResourceError fileDoesNotExistError(const ResourceResponse&) { return ResourceError("", 0, "", ""); }
    virtual ResourceError pluginWillHandleLoadError(const ResourceResponse&) { return ResourceError("", 0, "", ""); }

    virtual bool shouldFallBack(const ResourceError&) { return false; }

    virtual bool canHandleRequest(const ResourceRequest&) const { return false; }
    virtual bool canShowMIMEType(const String&) const { return false; }
    virtual String generatedMIMETypeForURLScheme(const String&) const { return ""; }

    virtual bool shouldTreatURLAsSameAsCurrent(const KURL&) const { return false; }
    virtual void didFinishLoad() { }

    virtual PassRefPtr<DocumentLoader> createDocumentLoader(const ResourceRequest&, const SubstituteData&) OVERRIDE;

    virtual String userAgent(const KURL&) { return ""; }

    virtual String doNotTrackValue() { return String(); }

    virtual void transitionToCommittedForNewPage() { }

    virtual bool shouldGoToHistoryItem(HistoryItem*) const { return false; }
    virtual bool shouldStopLoadingForHistoryItem(HistoryItem*) const { return false; }
    virtual void didDisplayInsecureContent() { }
    virtual void didRunInsecureContent(SecurityOrigin*, const KURL&) { }
    virtual void didDetectXSS(const KURL&, bool) { }
    virtual PassRefPtr<Frame> createFrame(const KURL&, const String&, HTMLFrameOwnerElement*, const String&, bool, int, int) OVERRIDE;
    virtual PassRefPtr<Widget> createPlugin(const IntSize&, HTMLPlugInElement*, const KURL&, const Vector<String>&, const Vector<String>&, const String&, bool) OVERRIDE;
    virtual PassRefPtr<Widget> createJavaAppletWidget(const IntSize&, HTMLAppletElement*, const KURL&, const Vector<String>&, const Vector<String>&) OVERRIDE;

    virtual ObjectContentType objectContentType(const KURL&, const String&, bool) { return ObjectContentType(); }

    virtual void redirectDataToPlugin(Widget*) { }
    virtual void dispatchDidClearWindowObjectInWorld(DOMWrapperWorld*) { }
    virtual void documentElementAvailable() { }

    virtual void didCreateScriptContext(v8::Handle<v8::Context>, int extensionGroup, int worldId) { }
    virtual void willReleaseScriptContext(v8::Handle<v8::Context>, int worldId) { }
    virtual bool allowScriptExtension(const String& extensionName, int extensionGroup, int worldId) { return false; }

    virtual PassRefPtr<FrameNetworkingContext> createNetworkingContext() OVERRIDE;

    virtual void didRequestAutocomplete(PassRefPtr<FormState>) OVERRIDE;
};

class EmptyTextCheckerClient : public TextCheckerClient {
public:
    virtual bool shouldEraseMarkersAfterChangeSelection(TextCheckingType) const { return true; }
    virtual void ignoreWordInSpellDocument(const String&) { }
    virtual void learnWord(const String&) { }
    virtual void checkSpellingOfString(const UChar*, int, int*, int*) { }
    virtual String getAutoCorrectSuggestionForMisspelledWord(const String&) { return String(); }
    virtual void checkGrammarOfString(const UChar*, int, Vector<GrammarDetail>&, int*, int*) { }

#if USE(UNIFIED_TEXT_CHECKING)
    virtual void checkTextOfParagraph(const UChar*, int, TextCheckingTypeMask, Vector<TextCheckingResult>&) { };
#endif

    virtual void getGuessesForWord(const String&, const String&, Vector<String>&) { }
    virtual void requestCheckingOfString(PassRefPtr<TextCheckingRequest>) OVERRIDE;
};

class EmptyEditorClient : public EditorClient {
    WTF_MAKE_NONCOPYABLE(EmptyEditorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyEditorClient() { }
    virtual ~EmptyEditorClient() { }
    virtual void pageDestroyed() { }
    virtual void frameWillDetachPage(Frame*) { }

    virtual bool shouldDeleteRange(Range*) { return false; }
    virtual bool smartInsertDeleteEnabled() { return false; }
    virtual bool isSelectTrailingWhitespaceEnabled() { return false; }
    virtual bool isContinuousSpellCheckingEnabled() { return false; }
    virtual void toggleContinuousSpellChecking() { }
    virtual bool isGrammarCheckingEnabled() { return false; }
    virtual void toggleGrammarChecking() { }
    virtual int spellCheckerDocumentTag() { return -1; }

    virtual bool selectWordBeforeMenuEvent() { return false; }
    virtual bool isEditable() { return false; }

    virtual bool shouldBeginEditing(Range*) { return false; }
    virtual bool shouldEndEditing(Range*) { return false; }
    virtual bool shouldInsertNode(Node*, Range*, EditorInsertAction) { return false; }
    virtual bool shouldInsertText(const String&, Range*, EditorInsertAction) { return false; }
    virtual bool shouldChangeSelectedRange(Range*, Range*, EAffinity, bool) { return false; }

    virtual bool shouldApplyStyle(StylePropertySet*, Range*) { return false; }
    virtual bool shouldMoveRangeAfterDelete(Range*, Range*) { return false; }

    virtual void didBeginEditing() { }
    virtual void respondToChangedContents() { }
    virtual void respondToChangedSelection(Frame*) { }
    virtual void didEndEditing() { }
    virtual void willWriteSelectionToPasteboard(Range*) { }
    virtual void didWriteSelectionToPasteboard() { }
    virtual void getClientPasteboardDataForRange(Range*, Vector<String>&, Vector<RefPtr<SharedBuffer> >&) { }
    virtual void didSetSelectionTypesForPasteboard() { }

    virtual void registerUndoStep(PassRefPtr<UndoStep>) OVERRIDE;
    virtual void registerRedoStep(PassRefPtr<UndoStep>) OVERRIDE;
    virtual void clearUndoRedoOperations() { }

    virtual bool canCopyCut(Frame*, bool defaultValue) const { return defaultValue; }
    virtual bool canPaste(Frame*, bool defaultValue) const { return defaultValue; }
    virtual bool canUndo() const { return false; }
    virtual bool canRedo() const { return false; }

    virtual void undo() { }
    virtual void redo() { }

    virtual void handleKeyboardEvent(KeyboardEvent*) { }
    virtual void handleInputMethodKeydown(KeyboardEvent*) { }

    virtual void textFieldDidBeginEditing(Element*) { }
    virtual void textFieldDidEndEditing(Element*) { }
    virtual void textDidChangeInTextField(Element*) { }
    virtual bool doTextFieldCommandFromEvent(Element*, KeyboardEvent*) { return false; }
    virtual void textWillBeDeletedInTextField(Element*) { }
    virtual void textDidChangeInTextArea(Element*) { }

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    virtual void showSubstitutionsPanel(bool) { }
    virtual bool substitutionsPanelIsShowing() { return false; }
    virtual void toggleSmartInsertDelete() { }
    virtual bool isAutomaticQuoteSubstitutionEnabled() { return false; }
    virtual void toggleAutomaticQuoteSubstitution() { }
    virtual bool isAutomaticLinkDetectionEnabled() { return false; }
    virtual void toggleAutomaticLinkDetection() { }
    virtual bool isAutomaticDashSubstitutionEnabled() { return false; }
    virtual void toggleAutomaticDashSubstitution() { }
    virtual bool isAutomaticTextReplacementEnabled() { return false; }
    virtual void toggleAutomaticTextReplacement() { }
    virtual bool isAutomaticSpellingCorrectionEnabled() { return false; }
    virtual void toggleAutomaticSpellingCorrection() { }
#endif

    TextCheckerClient* textChecker() { return &m_textCheckerClient; }

    virtual void updateSpellingUIWithGrammarString(const String&, const GrammarDetail&) { }
    virtual void updateSpellingUIWithMisspelledWord(const String&) { }
    virtual void showSpellingUI(bool) { }
    virtual bool spellingUIIsShowing() { return false; }

    virtual void willSetInputMethodState() { }
    virtual void setInputMethodState(bool) { }

private:
    EmptyTextCheckerClient m_textCheckerClient;
};

class EmptyContextMenuClient : public ContextMenuClient {
    WTF_MAKE_NONCOPYABLE(EmptyContextMenuClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyContextMenuClient() { }
    virtual ~EmptyContextMenuClient() {  }
    virtual void contextMenuDestroyed() { }

    virtual PassOwnPtr<ContextMenu> customizeMenu(PassOwnPtr<ContextMenu>) OVERRIDE;
    virtual void contextMenuItemSelected(const ContextMenuItem*, const ContextMenu*) { }

    virtual void downloadURL(const KURL&) { }
    virtual void copyImageToClipboard(const HitTestResult&) { }
    virtual void searchWithGoogle(const Frame*) { }
    virtual void lookUpInDictionary(Frame*) { }
    virtual bool isSpeaking() { return false; }
    virtual void speak(const String&) { }
    virtual void stopSpeaking() { }

#if USE(ACCESSIBILITY_CONTEXT_MENUS)
    virtual void showContextMenu() { }
#endif
};

class EmptyDragClient : public DragClient {
    WTF_MAKE_NONCOPYABLE(EmptyDragClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyDragClient() { }
    virtual ~EmptyDragClient() {}
    virtual void willPerformDragDestinationAction(DragDestinationAction, DragData*) { }
    virtual void willPerformDragSourceAction(DragSourceAction, const IntPoint&, Clipboard*) { }
    virtual DragDestinationAction actionMaskForDrag(DragData*) { return DragDestinationActionNone; }
    virtual DragSourceAction dragSourceActionMaskForPoint(const IntPoint&) { return DragSourceActionNone; }
    virtual void startDrag(DragImageRef, const IntPoint&, const IntPoint&, Clipboard*, Frame*, bool) { }
    virtual void dragControllerDestroyed() { }
};

class EmptyInspectorClient : public InspectorClient {
    WTF_MAKE_NONCOPYABLE(EmptyInspectorClient); WTF_MAKE_FAST_ALLOCATED;
public:
    EmptyInspectorClient() { }
    virtual ~EmptyInspectorClient() { }

    virtual void highlight() { }
    virtual void hideHighlight() { }
};

class EmptyDeviceClient : public DeviceClient {
public:
    virtual void startUpdating() OVERRIDE { }
    virtual void stopUpdating() OVERRIDE { }
};

class EmptyDeviceMotionClient : public DeviceMotionClient {
public:
    virtual void setController(DeviceMotionController*) { }
    virtual DeviceMotionData* lastMotion() const { return 0; }
    virtual void deviceMotionControllerDestroyed() { }
};

class EmptyDeviceOrientationClient : public DeviceOrientationClient {
public:
    virtual void setController(DeviceOrientationController*) { }
    virtual DeviceOrientationData* lastOrientation() const { return 0; }
    virtual void deviceOrientationControllerDestroyed() { }
};

class EmptyBackForwardClient : public BackForwardClient {
public:
    virtual void addItem(PassRefPtr<HistoryItem>) { }
    virtual void goToItem(HistoryItem*) { }
    virtual HistoryItem* itemAtIndex(int) { return 0; }
    virtual int backListCount() { return 0; }
    virtual int forwardListCount() { return 0; }
    virtual bool isActive() { return false; }
    virtual void close() { }
};

void fillWithEmptyClients(Page::PageClients&);

}

#endif // EmptyClients_h
