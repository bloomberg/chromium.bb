// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrameImpl_h
#define WebRemoteFrameImpl_h

#include "platform/heap/Handle.h"
#include "public/web/WebRemoteFrame.h"
#include "public/web/WebRemoteFrameClient.h"
#include "web/RemoteFrameClientImpl.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class FrameHost;
class FrameOwner;
class RemoteFrame;
class WebViewImpl;

class WebRemoteFrameImpl final : public RefCountedWillBeGarbageCollectedFinalized<WebRemoteFrameImpl>, public WebRemoteFrame {
public:
    explicit WebRemoteFrameImpl(WebRemoteFrameClient*);
    virtual ~WebRemoteFrameImpl();

    // WebRemoteFrame methods.
    virtual bool isWebLocalFrame() const override;
    virtual WebLocalFrame* toWebLocalFrame() override;
    virtual bool isWebRemoteFrame() const override;
    virtual WebRemoteFrame* toWebRemoteFrame() override;
    virtual void close() override;
    virtual WebString uniqueName() const override;
    virtual WebString assignedName() const override;
    virtual void setName(const WebString&) override;
    virtual WebVector<WebIconURL> iconURLs(int iconTypesMask) const override;
    virtual void setRemoteWebLayer(WebLayer*) override;
    virtual void setPermissionClient(WebPermissionClient*) override;
    virtual void setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient*) override;
    virtual void setCanHaveScrollbars(bool) override;
    virtual WebSize scrollOffset() const override;
    virtual void setScrollOffset(const WebSize&) override;
    virtual WebSize minimumScrollOffset() const override;
    virtual WebSize maximumScrollOffset() const override;
    virtual WebSize contentsSize() const override;
    virtual bool hasVisibleContent() const override;
    virtual WebRect visibleContentRect() const override;
    virtual bool hasHorizontalScrollbar() const override;
    virtual bool hasVerticalScrollbar() const override;
    virtual WebView* view() const override;
    virtual void removeChild(WebFrame*) override;
    virtual WebDocument document() const override;
    virtual WebPerformance performance() const override;
    virtual bool dispatchBeforeUnloadEvent() override;
    virtual void dispatchUnloadEvent() override;
    virtual NPObject* windowObject() const override;
    virtual void bindToWindowObject(const WebString& name, NPObject*) override;
    virtual void bindToWindowObject(const WebString& name, NPObject*, void*) override;
    virtual void executeScript(const WebScriptSource&) override;
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sources, unsigned numSources,
        int extensionGroup) override;
    virtual void setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin&) override;
    virtual void setIsolatedWorldContentSecurityPolicy(int worldID, const WebString&) override;
    virtual void addMessageToConsole(const WebConsoleMessage&) override;
    virtual void collectGarbage() override;
    virtual bool checkIfRunInsecureContent(const WebURL&) const override;
    virtual v8::Handle<v8::Value> executeScriptAndReturnValue(
        const WebScriptSource&) override;
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sourcesIn, unsigned numSources,
        int extensionGroup, WebVector<v8::Local<v8::Value> >* results) override;
    virtual v8::Handle<v8::Value> callFunctionEvenIfScriptDisabled(
        v8::Handle<v8::Function>,
        v8::Handle<v8::Value>,
        int argc,
        v8::Handle<v8::Value> argv[]) override;
    virtual v8::Local<v8::Context> mainWorldScriptContext() const override;
    virtual void reload(bool ignoreCache) override;
    virtual void reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache) override;
    virtual void loadRequest(const WebURLRequest&) override;
    virtual void loadHistoryItem(const WebHistoryItem&, WebHistoryLoadType, WebURLRequest::CachePolicy) override;
    virtual void loadData(
        const WebData&, const WebString& mimeType, const WebString& textEncoding,
        const WebURL& baseURL, const WebURL& unreachableURL, bool replace) override;
    virtual void loadHTMLString(
        const WebData& html, const WebURL& baseURL, const WebURL& unreachableURL,
        bool replace) override;
    virtual void stopLoading() override;
    virtual WebDataSource* provisionalDataSource() const override;
    virtual WebDataSource* dataSource() const override;
    virtual void enableViewSourceMode(bool enable) override;
    virtual bool isViewSourceModeEnabled() const override;
    virtual void setReferrerForRequest(WebURLRequest&, const WebURL& referrer) override;
    virtual void dispatchWillSendRequest(WebURLRequest&) override;
    virtual WebURLLoader* createAssociatedURLLoader(const WebURLLoaderOptions&) override;
    virtual unsigned unloadListenerCount() const override;
    virtual void replaceSelection(const WebString&) override;
    virtual void insertText(const WebString&) override;
    virtual void setMarkedText(const WebString&, unsigned location, unsigned length) override;
    virtual void unmarkText() override;
    virtual bool hasMarkedText() const override;
    virtual WebRange markedRange() const override;
    virtual bool firstRectForCharacterRange(unsigned location, unsigned length, WebRect&) const override;
    virtual size_t characterIndexForPoint(const WebPoint&) const override;
    virtual bool executeCommand(const WebString&, const WebNode& = WebNode()) override;
    virtual bool executeCommand(const WebString&, const WebString& value, const WebNode& = WebNode()) override;
    virtual bool isCommandEnabled(const WebString&) const override;
    virtual void enableContinuousSpellChecking(bool) override;
    virtual bool isContinuousSpellCheckingEnabled() const override;
    virtual void requestTextChecking(const WebElement&) override;
    virtual void replaceMisspelledRange(const WebString&) override;
    virtual void removeSpellingMarkers() override;
    virtual bool hasSelection() const override;
    virtual WebRange selectionRange() const override;
    virtual WebString selectionAsText() const override;
    virtual WebString selectionAsMarkup() const override;
    virtual bool selectWordAroundCaret() override;
    virtual void selectRange(const WebPoint& base, const WebPoint& extent) override;
    virtual void selectRange(const WebRange&) override;
    virtual void moveRangeSelection(const WebPoint& base, const WebPoint& extent) override;
    virtual void moveCaretSelection(const WebPoint&) override;
    virtual bool setEditableSelectionOffsets(int start, int end) override;
    virtual bool setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines) override;
    virtual void extendSelectionAndDelete(int before, int after) override;
    virtual void setCaretVisible(bool) override;
    virtual int printBegin(const WebPrintParams&, const WebNode& constrainToNode) override;
    virtual float printPage(int pageToPrint, WebCanvas*) override;
    virtual float getPrintPageShrink(int page) override;
    virtual void printEnd() override;
    virtual bool isPrintScalingDisabledForPlugin(const WebNode&) override;
    virtual int getPrintCopiesForPlugin(const WebNode&) override;
    virtual bool hasCustomPageSizeStyle(int pageIndex) override;
    virtual bool isPageBoxVisible(int pageIndex) override;
    virtual void pageSizeAndMarginsInPixels(
        int pageIndex,
        WebSize& pageSize,
        int& marginTop,
        int& marginRight,
        int& marginBottom,
        int& marginLeft) override;
    virtual WebString pageProperty(const WebString& propertyName, int pageIndex) override;
    virtual void printPagesWithBoundaries(WebCanvas*, const WebSize&) override;
    virtual bool find(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool wrapWithinFrame, WebRect* selectionRect) override;
    virtual void stopFinding(bool clearSelection) override;
    virtual void scopeStringMatches(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool reset) override;
    virtual void cancelPendingScopingEffort() override;
    virtual void increaseMatchCount(int count, int identifier) override;
    virtual void resetMatchCount() override;
    virtual int findMatchMarkersVersion() const override;
    virtual WebFloatRect activeFindMatchRect() override;
    virtual void findMatchRects(WebVector<WebFloatRect>&) override;
    virtual int selectNearestFindMatch(const WebFloatPoint&, WebRect* selectionRect) override;
    virtual void setTickmarks(const WebVector<WebRect>&) override;
    virtual void dispatchMessageEventWithOriginCheck(
        const WebSecurityOrigin& intendedTargetOrigin,
        const WebDOMEvent&) override;

    virtual WebString contentAsText(size_t maxChars) const override;
    virtual WebString contentAsMarkup() const override;
    virtual WebString renderTreeAsText(RenderAsTextControls toShow = RenderAsTextNormal) const override;
    virtual WebString markerTextForListItem(const WebElement&) const override;
    virtual WebRect selectionBoundsRect() const override;

    virtual bool selectionStartHasSpellingMarkerFor(int from, int length) const override;
    virtual WebString layerTreeAsText(bool showDebugInfo = false) const override;

    virtual WebLocalFrame* createLocalChild(const WebString& name, WebFrameClient*) override;
    virtual WebRemoteFrame* createRemoteChild(const WebString& name, WebRemoteFrameClient*) override;

    void initializeCoreFrame(FrameHost*, FrameOwner*, const AtomicString& name);

    void setCoreFrame(PassRefPtrWillBeRawPtr<RemoteFrame>);
    RemoteFrame* frame() const { return m_frame.get(); }

    WebRemoteFrameClient* client() const { return m_client; }

    static WebRemoteFrameImpl* fromFrame(RemoteFrame&);

    virtual void initializeFromFrame(WebLocalFrame*) const override;

#if ENABLE(OILPAN)
    void trace(Visitor*);
#endif

private:
    RemoteFrameClientImpl m_frameClient;
    RefPtrWillBeMember<RemoteFrame> m_frame;
    WebRemoteFrameClient* m_client;

    WillBeHeapHashMap<WebFrame*, OwnPtrWillBeMember<FrameOwner> > m_ownersForChildren;

#if ENABLE(OILPAN)
    // Oilpan: to provide the guarantee of having the frame live until
    // close() is called, an instance keep a self-persistent. It is
    // cleared upon calling close(). This avoids having to assume that
    // an embedder's WebFrame references are all discovered via thread
    // state (stack, registers) should an Oilpan GC strike while we're
    // in the process of detaching.
    GC_PLUGIN_IGNORE("340522")
    Persistent<WebRemoteFrameImpl> m_selfKeepAlive;
#endif
};

DEFINE_TYPE_CASTS(WebRemoteFrameImpl, WebFrame, frame, frame->isWebRemoteFrame(), frame.isWebRemoteFrame());

} // namespace blink

#endif // WebRemoteFrameImpl_h
