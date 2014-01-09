/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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

#ifndef WebFrameImpl_h
#define WebFrameImpl_h

#include "WebFrame.h"

#include "FrameLoaderClientImpl.h"
#include "core/frame/Frame.h"
#include "platform/geometry/FloatRect.h"
#include "public/platform/WebFileSystemType.h"
#include "wtf/Compiler.h"
#include "wtf/OwnPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/text/WTFString.h"

namespace WebCore {
class GraphicsContext;
class HTMLInputElement;
class HistoryItem;
class IntSize;
class KURL;
class Node;
class Range;
class SubstituteData;
struct FrameLoadRequest;
struct WindowFeatures;
}

namespace blink {
class ChromePrintContext;
class SharedWorkerRepositoryClientImpl;
class TextFinder;
class WebDataSourceImpl;
class WebInputElement;
class WebFrameClient;
class WebPerformance;
class WebPluginContainerImpl;
class WebView;
class WebViewImpl;
struct WebPrintParams;

template <typename T> class WebVector;

// Implementation of WebFrame, note that this is a reference counted object.
class WebFrameImpl
    : public WebFrame
    , public RefCounted<WebFrameImpl> {
public:
    // WebFrame methods:
    virtual void close();
    virtual WebString uniqueName() const;
    virtual WebString assignedName() const;
    virtual void setName(const WebString&);
    virtual long long embedderIdentifier() const;
    virtual WebVector<WebIconURL> iconURLs(int iconTypesMask) const;
    virtual void setRemoteWebLayer(WebLayer*);
    virtual void setPermissionClient(WebPermissionClient*);
    virtual void setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient*);
    virtual WebSize scrollOffset() const;
    virtual void setScrollOffset(const WebSize&);
    virtual WebSize minimumScrollOffset() const;
    virtual WebSize maximumScrollOffset() const;
    virtual WebSize contentsSize() const;
    virtual bool hasVisibleContent() const;
    virtual WebRect visibleContentRect() const;
    virtual bool hasHorizontalScrollbar() const;
    virtual bool hasVerticalScrollbar() const;
    virtual WebView* view() const;
    virtual WebFrame* opener() const;
    virtual void setOpener(const WebFrame*);
    virtual WebFrame* parent() const;
    virtual WebFrame* top() const;
    virtual WebFrame* firstChild() const;
    virtual WebFrame* lastChild() const;
    virtual WebFrame* nextSibling() const;
    virtual WebFrame* previousSibling() const;
    virtual WebFrame* traverseNext(bool wrap) const;
    virtual WebFrame* traversePrevious(bool wrap) const;
    virtual WebFrame* findChildByName(const WebString&) const;
    virtual WebFrame* findChildByExpression(const WebString&) const;
    virtual WebDocument document() const;
    virtual WebPerformance performance() const;
    virtual NPObject* windowObject() const;
    virtual void bindToWindowObject(const WebString& name, NPObject*);
    virtual void bindToWindowObject(const WebString& name, NPObject*, void*);
    virtual void executeScript(const WebScriptSource&);
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sources, unsigned numSources,
        int extensionGroup);
    virtual void setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin&);
    virtual void setIsolatedWorldContentSecurityPolicy(int worldID, const WebString&);
    virtual void addMessageToConsole(const WebConsoleMessage&);
    virtual void collectGarbage();
    virtual bool checkIfRunInsecureContent(const WebURL&) const;
    virtual v8::Handle<v8::Value> executeScriptAndReturnValue(
        const WebScriptSource&);
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sourcesIn, unsigned numSources,
        int extensionGroup, WebVector<v8::Local<v8::Value> >* results);
    virtual v8::Handle<v8::Value> callFunctionEvenIfScriptDisabled(
        v8::Handle<v8::Function>,
        v8::Handle<v8::Object>,
        int argc,
        v8::Handle<v8::Value> argv[]);
    virtual v8::Local<v8::Context> mainWorldScriptContext() const;
    virtual v8::Handle<v8::Value> createFileSystem(WebFileSystemType,
        const WebString& name,
        const WebString& path);
    virtual v8::Handle<v8::Value> createSerializableFileSystem(WebFileSystemType,
        const WebString& name,
        const WebString& path);
    virtual v8::Handle<v8::Value> createFileEntry(WebFileSystemType,
        const WebString& fileSystemName,
        const WebString& fileSystemPath,
        const WebString& filePath,
        bool isDirectory);
    virtual void reload(bool ignoreCache);
    virtual void reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache);
    virtual void loadRequest(const WebURLRequest&);
    virtual void loadHistoryItem(const WebHistoryItem&, WebURLRequest::CachePolicy);
    virtual void loadData(
        const WebData&, const WebString& mimeType, const WebString& textEncoding,
        const WebURL& baseURL, const WebURL& unreachableURL, bool replace);
    virtual void loadHTMLString(
        const WebData& html, const WebURL& baseURL, const WebURL& unreachableURL,
        bool replace);
    virtual bool isLoading() const;
    virtual void stopLoading();
    virtual WebDataSource* provisionalDataSource() const;
    virtual WebDataSource* dataSource() const;
    virtual WebHistoryItem previousHistoryItem() const;
    virtual WebHistoryItem currentHistoryItem() const;
    virtual void enableViewSourceMode(bool enable);
    virtual bool isViewSourceModeEnabled() const;
    virtual void setReferrerForRequest(WebURLRequest&, const WebURL& referrer);
    virtual void dispatchWillSendRequest(WebURLRequest&);
    virtual WebURLLoader* createAssociatedURLLoader(const WebURLLoaderOptions&);
    virtual unsigned unloadListenerCount() const;
    virtual void replaceSelection(const WebString&);
    virtual void insertText(const WebString&);
    virtual void setMarkedText(const WebString&, unsigned location, unsigned length);
    virtual void unmarkText();
    virtual bool hasMarkedText() const;
    virtual WebRange markedRange() const;
    virtual bool firstRectForCharacterRange(unsigned location, unsigned length, WebRect&) const;
    virtual size_t characterIndexForPoint(const WebPoint&) const;
    virtual bool executeCommand(const WebString&, const WebNode& = WebNode());
    virtual bool executeCommand(const WebString&, const WebString& value, const WebNode& = WebNode());
    virtual bool isCommandEnabled(const WebString&) const;
    virtual void enableContinuousSpellChecking(bool);
    virtual bool isContinuousSpellCheckingEnabled() const;
    virtual void requestTextChecking(const WebElement&);
    virtual void replaceMisspelledRange(const WebString&);
    virtual void removeSpellingMarkers();
    virtual bool hasSelection() const;
    virtual WebRange selectionRange() const;
    virtual WebString selectionAsText() const;
    virtual WebString selectionAsMarkup() const;
    virtual bool selectWordAroundCaret();
    virtual void selectRange(const WebPoint& base, const WebPoint& extent);
    virtual void selectRange(const WebRange&);
    virtual void moveRangeSelection(const WebPoint& base, const WebPoint& extent);
    virtual void moveCaretSelection(const WebPoint&);
    virtual void setCaretVisible(bool);
    virtual int printBegin(const WebPrintParams&, const WebNode& constrainToNode);
    virtual float printPage(int pageToPrint, WebCanvas*);
    virtual float getPrintPageShrink(int page);
    virtual void printEnd();
    virtual bool isPrintScalingDisabledForPlugin(const WebNode&);
    virtual bool hasCustomPageSizeStyle(int pageIndex);
    virtual bool isPageBoxVisible(int pageIndex);
    virtual void pageSizeAndMarginsInPixels(int pageIndex,
                                            WebSize& pageSize,
                                            int& marginTop,
                                            int& marginRight,
                                            int& marginBottom,
                                            int& marginLeft);
    virtual WebString pageProperty(const WebString& propertyName, int pageIndex);
    virtual void printPagesWithBoundaries(WebCanvas*, const WebSize&);
    virtual bool find(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool wrapWithinFrame, WebRect* selectionRect);
    virtual void stopFinding(bool clearSelection);
    virtual void scopeStringMatches(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool reset);
    virtual void cancelPendingScopingEffort();
    virtual void increaseMatchCount(int count, int identifier);
    virtual void resetMatchCount();
    virtual int findMatchMarkersVersion() const;
    virtual WebFloatRect activeFindMatchRect();
    virtual void findMatchRects(WebVector<WebFloatRect>&);
    virtual int selectNearestFindMatch(const WebFloatPoint&, WebRect* selectionRect);

    virtual void sendOrientationChangeEvent(int orientation);

    virtual void dispatchMessageEventWithOriginCheck(
        const WebSecurityOrigin& intendedTargetOrigin,
        const WebDOMEvent&);

    virtual WebString contentAsText(size_t maxChars) const;
    virtual WebString contentAsMarkup() const;
    virtual WebString renderTreeAsText(RenderAsTextControls toShow = RenderAsTextNormal) const;
    virtual WebString markerTextForListItem(const WebElement&) const;
    virtual WebRect selectionBoundsRect() const;

    virtual bool selectionStartHasSpellingMarkerFor(int from, int length) const;
    virtual WebString layerTreeAsText(bool showDebugInfo = false) const;

    void willDetachParent();

    static WebFrameImpl* create(WebFrameClient*);
    // FIXME: Move the embedderIdentifier concept fully to the embedder and
    // remove this factory method.
    static WebFrameImpl* create(WebFrameClient*, long long embedderIdentifier);
    virtual ~WebFrameImpl();

    // Called by the WebViewImpl to initialize the main frame for the page.
    void initializeAsMainFrame(WebCore::Page*);

    PassRefPtr<WebCore::Frame> createChildFrame(
        const WebCore::FrameLoadRequest&, WebCore::HTMLFrameOwnerElement*);

    void didChangeContentsSize(const WebCore::IntSize&);

    void createFrameView();

    static WebFrameImpl* fromFrame(WebCore::Frame* frame);
    static WebFrameImpl* fromFrameOwnerElement(WebCore::Element* element);

    // If the frame hosts a PluginDocument, this method returns the WebPluginContainerImpl
    // that hosts the plugin.
    static WebPluginContainerImpl* pluginContainerFromFrame(WebCore::Frame*);

    // If the frame hosts a PluginDocument, this method returns the WebPluginContainerImpl
    // that hosts the plugin. If the provided node is a plugin, then it runs its
    // WebPluginContainerImpl.
    static WebPluginContainerImpl* pluginContainerFromNode(WebCore::Frame*, const WebNode&);

    WebViewImpl* viewImpl() const;

    WebCore::FrameView* frameView() const { return frame() ? frame()->view() : 0; }

    // Getters for the impls corresponding to Get(Provisional)DataSource. They
    // may return 0 if there is no corresponding data source.
    WebDataSourceImpl* dataSourceImpl() const;
    WebDataSourceImpl* provisionalDataSourceImpl() const;

    // Returns which frame has an active match. This function should only be
    // called on the main frame, as it is the only frame keeping track. Returned
    // value can be 0 if no frame has an active match.
    WebFrameImpl* activeMatchFrame() const;

    // Returns the active match in the current frame. Could be a null range if
    // the local frame has no active match.
    WebCore::Range* activeMatch() const;

    // When a Find operation ends, we want to set the selection to what was active
    // and set focus to the first focusable node we find (starting with the first
    // node in the matched range and going up the inheritance chain). If we find
    // nothing to focus we focus the first focusable node in the range. This
    // allows us to set focus to a link (when we find text inside a link), which
    // allows us to navigate by pressing Enter after closing the Find box.
    void setFindEndstateFocusAndSelection();

    void didFail(const WebCore::ResourceError&, bool wasProvisional);

    // Sets whether the WebFrameImpl allows its document to be scrolled.
    // If the parameter is true, allow the document to be scrolled.
    // Otherwise, disallow scrolling.
    void setCanHaveScrollbars(bool);

    WebCore::Frame* frame() const { return m_frame.get(); }
    WebFrameClient* client() const { return m_client; }
    void setClient(WebFrameClient* client) { m_client = client; }

    WebPermissionClient* permissionClient() { return m_permissionClient; }
    SharedWorkerRepositoryClientImpl* sharedWorkerRepositoryClient() const { return m_sharedWorkerRepositoryClient.get(); }

    void setInputEventsTransformForEmulation(const WebCore::IntSize&, float);

    static void selectWordAroundPosition(WebCore::Frame*, WebCore::VisiblePosition);

    // Returns the text finder object if it already exists.
    // Otherwise creates it and then returns.
    TextFinder& getOrCreateTextFinder();

private:
    friend class FrameLoaderClientImpl;

    WebFrameImpl(WebFrameClient*, long long frame_identifier);

    // Sets the local WebCore frame and registers destruction observers.
    void setWebCoreFrame(PassRefPtr<WebCore::Frame>);

    void loadJavaScriptURL(const WebCore::KURL&);

    // Returns a hit-tested VisiblePosition for the given point
    WebCore::VisiblePosition visiblePositionForWindowPoint(const WebPoint&);

    class WebFrameInit : public WebCore::FrameInit {
    public:
        static PassRefPtr<WebFrameInit> create(WebFrameImpl* webFrameImpl, int64_t frameID)
        {
            return adoptRef(new WebFrameInit(webFrameImpl, frameID));
        }

    private:
        WebFrameInit(WebFrameImpl* webFrameImpl, int64_t frameID)
            : WebCore::FrameInit(frameID)
            , m_frameLoaderClientImpl(webFrameImpl)
        {
            setFrameLoaderClient(&m_frameLoaderClientImpl);
        }

        FrameLoaderClientImpl m_frameLoaderClientImpl;
    };
    RefPtr<WebFrameInit> m_frameInit;

    // The embedder retains a reference to the WebCore Frame while it is active in the DOM. This
    // reference is released when the frame is removed from the DOM or the entire page is closed.
    RefPtr<WebCore::Frame> m_frame;

    WebFrameClient* m_client;
    WebPermissionClient* m_permissionClient;
    OwnPtr<SharedWorkerRepositoryClientImpl> m_sharedWorkerRepositoryClient;

    // Will be initialized after first call to find() or scopeStringMatches().
    OwnPtr<TextFinder> m_textFinder;

    // Valid between calls to BeginPrint() and EndPrint(). Containts the print
    // information. Is used by PrintPage().
    OwnPtr<ChromePrintContext> m_printContext;

    // Stores the additional input events offset and scale when device metrics emulation is enabled.
    WebCore::IntSize m_inputEventsOffsetForEmulation;
    float m_inputEventsScaleFactorForEmulation;
};

DEFINE_TYPE_CASTS(WebFrameImpl, WebFrame, frame, true, true);

} // namespace blink

#endif
