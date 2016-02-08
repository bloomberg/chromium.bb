// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebRemoteFrameImpl_h
#define WebRemoteFrameImpl_h

#include "core/frame/FrameOwner.h"
#include "core/frame/RemoteFrame.h"
#include "public/web/WebRemoteFrame.h"
#include "public/web/WebRemoteFrameClient.h"
#include "web/RemoteFrameClientImpl.h"
#include "web/WebFrameImplBase.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"

namespace blink {

class FrameHost;
class FrameOwner;
class RemoteFrame;

class WebRemoteFrameImpl final : public WebFrameImplBase, public WebRemoteFrame {
public:
    static WebRemoteFrameImpl* create(WebTreeScopeType, WebRemoteFrameClient*);
    ~WebRemoteFrameImpl() override;

    // WebFrame methods:
    bool isWebLocalFrame() const override;
    WebLocalFrame* toWebLocalFrame() override;
    bool isWebRemoteFrame() const override;
    WebRemoteFrame* toWebRemoteFrame() override;
    void close() override;
    WebString uniqueName() const override;
    WebString assignedName() const override;
    void setName(const WebString&) override;
    WebVector<WebIconURL> iconURLs(int iconTypesMask) const override;
    void setRemoteWebLayer(WebLayer*) override;
    void setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient*) override;
    void setCanHaveScrollbars(bool) override;
    WebSize scrollOffset() const override;
    void setScrollOffset(const WebSize&) override;
    WebSize contentsSize() const override;
    bool hasVisibleContent() const override;
    WebRect visibleContentRect() const override;
    bool hasHorizontalScrollbar() const override;
    bool hasVerticalScrollbar() const override;
    WebView* view() const override;
    void removeChild(WebFrame*) override;
    WebDocument document() const override;
    WebPerformance performance() const override;
    bool dispatchBeforeUnloadEvent() override;
    void dispatchUnloadEvent() override;
    NPObject* windowObject() const override;
    void bindToWindowObject(const WebString& name, NPObject*) override;
    void bindToWindowObject(const WebString& name, NPObject*, void*) override;
    void executeScript(const WebScriptSource&) override;
    void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sources, unsigned numSources,
        int extensionGroup) override;
    void setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin&) override;
    void setIsolatedWorldContentSecurityPolicy(int worldID, const WebString&) override;
    void addMessageToConsole(const WebConsoleMessage&) override;
    void collectGarbage() override;
    v8::Local<v8::Value> executeScriptAndReturnValue(
        const WebScriptSource&) override;
    void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sourcesIn, unsigned numSources,
        int extensionGroup, WebVector<v8::Local<v8::Value>>* results) override;
    v8::Local<v8::Value> callFunctionEvenIfScriptDisabled(
        v8::Local<v8::Function>,
        v8::Local<v8::Value>,
        int argc,
        v8::Local<v8::Value> argv[]) override;
    v8::Local<v8::Context> mainWorldScriptContext() const override;
    v8::Local<v8::Context> deprecatedMainWorldScriptContext() const override;
    void reload(bool ignoreCache) override;
    void reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache) override;
    void loadRequest(const WebURLRequest&) override;
    void loadHistoryItem(const WebHistoryItem&, WebHistoryLoadType, WebURLRequest::CachePolicy) override;
    void loadData(
        const WebData&, const WebString& mimeType, const WebString& textEncoding,
        const WebURL& baseURL, const WebURL& unreachableURL, bool replace) override;
    void loadHTMLString(
        const WebData& html, const WebURL& baseURL, const WebURL& unreachableURL,
        bool replace) override;
    void stopLoading() override;
    WebDataSource* provisionalDataSource() const override;
    WebDataSource* dataSource() const override;
    void enableViewSourceMode(bool enable) override;
    bool isViewSourceModeEnabled() const override;
    void setReferrerForRequest(WebURLRequest&, const WebURL& referrer) override;
    void dispatchWillSendRequest(WebURLRequest&) override;
    WebURLLoader* createAssociatedURLLoader(const WebURLLoaderOptions&) override;
    unsigned unloadListenerCount() const override;
    void insertText(const WebString&) override;
    void setMarkedText(const WebString&, unsigned location, unsigned length) override;
    void unmarkText() override;
    bool hasMarkedText() const override;
    WebRange markedRange() const override;
    bool firstRectForCharacterRange(unsigned location, unsigned length, WebRect&) const override;
    size_t characterIndexForPoint(const WebPoint&) const override;
    bool executeCommand(const WebString&, const WebNode& = WebNode()) override;
    bool executeCommand(const WebString&, const WebString& value, const WebNode& = WebNode()) override;
    bool isCommandEnabled(const WebString&) const override;
    void enableContinuousSpellChecking(bool) override;
    bool isContinuousSpellCheckingEnabled() const override;
    void requestTextChecking(const WebElement&) override;
    void removeSpellingMarkers() override;
    bool hasSelection() const override;
    WebRange selectionRange() const override;
    WebString selectionAsText() const override;
    WebString selectionAsMarkup() const override;
    bool selectWordAroundCaret() override;
    void selectRange(const WebPoint& base, const WebPoint& extent) override;
    void selectRange(const WebRange&) override;
    void moveRangeSelection(const WebPoint& base, const WebPoint& extent, WebFrame::TextGranularity = CharacterGranularity) override;
    void moveCaretSelection(const WebPoint&) override;
    bool setEditableSelectionOffsets(int start, int end) override;
    bool setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines) override;
    void extendSelectionAndDelete(int before, int after) override;
    void setCaretVisible(bool) override;
    int printBegin(const WebPrintParams&, const WebNode& constrainToNode) override;
    float printPage(int pageToPrint, WebCanvas*) override;
    float getPrintPageShrink(int page) override;
    void printEnd() override;
    bool isPrintScalingDisabledForPlugin(const WebNode&) override;
    bool hasCustomPageSizeStyle(int pageIndex) override;
    bool isPageBoxVisible(int pageIndex) override;
    void pageSizeAndMarginsInPixels(
        int pageIndex,
        WebSize& pageSize,
        int& marginTop,
        int& marginRight,
        int& marginBottom,
        int& marginLeft) override;
    WebString pageProperty(const WebString& propertyName, int pageIndex) override;
    void printPagesWithBoundaries(WebCanvas*, const WebSize&) override;
    void dispatchMessageEventWithOriginCheck(
        const WebSecurityOrigin& intendedTargetOrigin,
        const WebDOMEvent&) override;

    WebRect selectionBoundsRect() const override;

    bool selectionStartHasSpellingMarkerFor(int from, int length) const override;
    WebString layerTreeAsText(bool showDebugInfo = false) const override;

    WebFrameImplBase* toImplBase() { return this; }

    // WebFrameImplBase methods:
    void initializeCoreFrame(FrameHost*, FrameOwner*, const AtomicString& name, const AtomicString& fallbackName) override;
    RemoteFrame* frame() const override { return m_frame.get(); }

    void setCoreFrame(PassRefPtrWillBeRawPtr<RemoteFrame>);

    WebRemoteFrameClient* client() const { return m_client; }

    static WebRemoteFrameImpl* fromFrame(RemoteFrame&);

    // WebRemoteFrame methods:
    WebLocalFrame* createLocalChild(WebTreeScopeType, const WebString& name, WebSandboxFlags, WebFrameClient*, WebFrame* previousSibling, const WebFrameOwnerProperties&) override;
    WebRemoteFrame* createRemoteChild(WebTreeScopeType, const WebString& name, WebSandboxFlags, WebRemoteFrameClient*) override;

    void initializeFromFrame(WebLocalFrame*) const override;

    void setReplicatedOrigin(const WebSecurityOrigin&) const override;
    void setReplicatedSandboxFlags(WebSandboxFlags) const override;
    void setReplicatedName(const WebString&) const override;
    void setReplicatedShouldEnforceStrictMixedContentChecking(bool) const override;
    void DispatchLoadEventForFrameOwner() const override;

    void didStartLoading() override;
    void didStopLoading() override;

    bool isIgnoredForHitTest() const override;

#if ENABLE(OILPAN)
    DECLARE_TRACE();
#endif

private:
    WebRemoteFrameImpl(WebTreeScopeType, WebRemoteFrameClient*);

    OwnPtrWillBeMember<RemoteFrameClientImpl> m_frameClient;
    RefPtrWillBeMember<RemoteFrame> m_frame;
    WebRemoteFrameClient* m_client;

    WillBeHeapHashMap<WebFrame*, OwnPtrWillBeMember<FrameOwner>> m_ownersForChildren;

#if ENABLE(OILPAN)
    // Oilpan: WebRemoteFrameImpl must remain alive until close() is called.
    // Accomplish that by keeping a self-referential Persistent<>. It is
    // cleared upon close().
    SelfKeepAlive<WebRemoteFrameImpl> m_selfKeepAlive;
#endif
};

DEFINE_TYPE_CASTS(WebRemoteFrameImpl, WebFrame, frame, frame->isWebRemoteFrame(), frame.isWebRemoteFrame());

} // namespace blink

#endif // WebRemoteFrameImpl_h
