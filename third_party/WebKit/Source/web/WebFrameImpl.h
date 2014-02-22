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
class WebFrameImpl FINAL
    : public WebFrame
    , public RefCounted<WebFrameImpl> {
public:
    // WebFrame methods:
    virtual void close() OVERRIDE;
    virtual WebString uniqueName() const OVERRIDE;
    virtual WebString assignedName() const OVERRIDE;
    virtual void setName(const WebString&) OVERRIDE;
    virtual WebVector<WebIconURL> iconURLs(int iconTypesMask) const OVERRIDE;
    virtual void setIsRemote(bool) OVERRIDE;
    virtual void setRemoteWebLayer(WebLayer*) OVERRIDE;
    virtual void setPermissionClient(WebPermissionClient*) OVERRIDE;
    virtual void setSharedWorkerRepositoryClient(WebSharedWorkerRepositoryClient*) OVERRIDE;
    virtual WebSize scrollOffset() const OVERRIDE;
    virtual void setScrollOffset(const WebSize&) OVERRIDE;
    virtual WebSize minimumScrollOffset() const OVERRIDE;
    virtual WebSize maximumScrollOffset() const OVERRIDE;
    virtual WebSize contentsSize() const OVERRIDE;
    virtual bool hasVisibleContent() const OVERRIDE;
    virtual WebRect visibleContentRect() const OVERRIDE;
    virtual bool hasHorizontalScrollbar() const OVERRIDE;
    virtual bool hasVerticalScrollbar() const OVERRIDE;
    virtual WebView* view() const OVERRIDE;
    virtual WebFrame* opener() const OVERRIDE;
    virtual void setOpener(const WebFrame*) OVERRIDE;
    virtual void appendChild(WebFrame*) OVERRIDE;
    virtual void removeChild(WebFrame*) OVERRIDE;
    virtual WebFrame* parent() const OVERRIDE;
    virtual WebFrame* top() const OVERRIDE;
    virtual WebFrame* previousSibling() const OVERRIDE;
    virtual WebFrame* nextSibling() const OVERRIDE;
    virtual WebFrame* firstChild() const OVERRIDE;
    virtual WebFrame* lastChild() const OVERRIDE;
    virtual WebFrame* traversePrevious(bool wrap) const OVERRIDE;
    virtual WebFrame* traverseNext(bool wrap) const OVERRIDE;
    virtual WebFrame* findChildByName(const WebString&) const OVERRIDE;
    virtual WebFrame* findChildByExpression(const WebString&) const OVERRIDE;
    virtual WebDocument document() const OVERRIDE;
    virtual WebPerformance performance() const OVERRIDE;
    virtual NPObject* windowObject() const OVERRIDE;
    virtual void bindToWindowObject(const WebString& name, NPObject*) OVERRIDE;
    virtual void bindToWindowObject(const WebString& name, NPObject*, void*) OVERRIDE;
    virtual void executeScript(const WebScriptSource&) OVERRIDE;
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sources, unsigned numSources,
        int extensionGroup) OVERRIDE;
    virtual void setIsolatedWorldSecurityOrigin(int worldID, const WebSecurityOrigin&) OVERRIDE;
    virtual void setIsolatedWorldContentSecurityPolicy(int worldID, const WebString&) OVERRIDE;
    virtual void addMessageToConsole(const WebConsoleMessage&) OVERRIDE;
    virtual void collectGarbage() OVERRIDE;
    virtual bool checkIfRunInsecureContent(const WebURL&) const OVERRIDE;
    virtual v8::Handle<v8::Value> executeScriptAndReturnValue(
        const WebScriptSource&) OVERRIDE;
    virtual void executeScriptInIsolatedWorld(
        int worldID, const WebScriptSource* sourcesIn, unsigned numSources,
        int extensionGroup, WebVector<v8::Local<v8::Value> >* results) OVERRIDE;
    virtual v8::Handle<v8::Value> callFunctionEvenIfScriptDisabled(
        v8::Handle<v8::Function>,
        v8::Handle<v8::Object>,
        int argc,
        v8::Handle<v8::Value> argv[]) OVERRIDE;
    virtual v8::Local<v8::Context> mainWorldScriptContext() const OVERRIDE;
    virtual v8::Handle<v8::Value> createFileSystem(WebFileSystemType,
        const WebString& name,
        const WebString& path) OVERRIDE;
    virtual v8::Handle<v8::Value> createSerializableFileSystem(WebFileSystemType,
        const WebString& name,
        const WebString& path) OVERRIDE;
    virtual v8::Handle<v8::Value> createFileEntry(WebFileSystemType,
        const WebString& fileSystemName,
        const WebString& fileSystemPath,
        const WebString& filePath,
        bool isDirectory) OVERRIDE;
    virtual void reload(bool ignoreCache) OVERRIDE;
    virtual void reloadWithOverrideURL(const WebURL& overrideUrl, bool ignoreCache) OVERRIDE;
    virtual void loadRequest(const WebURLRequest&) OVERRIDE;
    virtual void loadHistoryItem(const WebHistoryItem&, WebURLRequest::CachePolicy) OVERRIDE;
    virtual void loadData(
        const WebData&, const WebString& mimeType, const WebString& textEncoding,
        const WebURL& baseURL, const WebURL& unreachableURL, bool replace) OVERRIDE;
    virtual void loadHTMLString(
        const WebData& html, const WebURL& baseURL, const WebURL& unreachableURL,
        bool replace) OVERRIDE;
    virtual bool isLoading() const OVERRIDE;
    virtual void stopLoading() OVERRIDE;
    virtual WebDataSource* provisionalDataSource() const OVERRIDE;
    virtual WebDataSource* dataSource() const OVERRIDE;
    virtual WebHistoryItem previousHistoryItem() const OVERRIDE;
    virtual WebHistoryItem currentHistoryItem() const OVERRIDE;
    virtual void enableViewSourceMode(bool enable) OVERRIDE;
    virtual bool isViewSourceModeEnabled() const OVERRIDE;
    virtual void setReferrerForRequest(WebURLRequest&, const WebURL& referrer) OVERRIDE;
    virtual void dispatchWillSendRequest(WebURLRequest&) OVERRIDE;
    virtual WebURLLoader* createAssociatedURLLoader(const WebURLLoaderOptions&) OVERRIDE;
    virtual unsigned unloadListenerCount() const OVERRIDE;
    virtual void replaceSelection(const WebString&) OVERRIDE;
    virtual void insertText(const WebString&) OVERRIDE;
    virtual void setMarkedText(const WebString&, unsigned location, unsigned length) OVERRIDE;
    virtual void unmarkText() OVERRIDE;
    virtual bool hasMarkedText() const OVERRIDE;
    virtual WebRange markedRange() const OVERRIDE;
    virtual bool firstRectForCharacterRange(unsigned location, unsigned length, WebRect&) const OVERRIDE;
    virtual size_t characterIndexForPoint(const WebPoint&) const OVERRIDE;
    virtual bool executeCommand(const WebString&, const WebNode& = WebNode()) OVERRIDE;
    virtual bool executeCommand(const WebString&, const WebString& value, const WebNode& = WebNode()) OVERRIDE;
    virtual bool isCommandEnabled(const WebString&) const OVERRIDE;
    virtual void enableContinuousSpellChecking(bool) OVERRIDE;
    virtual bool isContinuousSpellCheckingEnabled() const OVERRIDE;
    virtual void requestTextChecking(const WebElement&) OVERRIDE;
    virtual void replaceMisspelledRange(const WebString&) OVERRIDE;
    virtual void removeSpellingMarkers() OVERRIDE;
    virtual bool hasSelection() const OVERRIDE;
    virtual WebRange selectionRange() const OVERRIDE;
    virtual WebString selectionAsText() const OVERRIDE;
    virtual WebString selectionAsMarkup() const OVERRIDE;
    virtual bool selectWordAroundCaret() OVERRIDE;
    virtual void selectRange(const WebPoint& base, const WebPoint& extent) OVERRIDE;
    virtual void selectRange(const WebRange&) OVERRIDE;
    virtual void moveRangeSelection(const WebPoint& base, const WebPoint& extent) OVERRIDE;
    virtual void moveCaretSelection(const WebPoint&) OVERRIDE;
    virtual void setCaretVisible(bool) OVERRIDE;
    virtual int printBegin(const WebPrintParams&, const WebNode& constrainToNode) OVERRIDE;
    virtual float printPage(int pageToPrint, WebCanvas*) OVERRIDE;
    virtual float getPrintPageShrink(int page) OVERRIDE;
    virtual void printEnd() OVERRIDE;
    virtual bool isPrintScalingDisabledForPlugin(const WebNode&) OVERRIDE;
    virtual bool hasCustomPageSizeStyle(int pageIndex) OVERRIDE;
    virtual bool isPageBoxVisible(int pageIndex) OVERRIDE;
    virtual void pageSizeAndMarginsInPixels(int pageIndex,
                                            WebSize& pageSize,
                                            int& marginTop,
                                            int& marginRight,
                                            int& marginBottom,
                                            int& marginLeft) OVERRIDE;
    virtual WebString pageProperty(const WebString& propertyName, int pageIndex) OVERRIDE;
    virtual void printPagesWithBoundaries(WebCanvas*, const WebSize&) OVERRIDE;
    virtual bool find(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool wrapWithinFrame, WebRect* selectionRect) OVERRIDE;
    virtual void stopFinding(bool clearSelection) OVERRIDE;
    virtual void scopeStringMatches(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool reset) OVERRIDE;
    virtual void cancelPendingScopingEffort() OVERRIDE;
    virtual void increaseMatchCount(int count, int identifier) OVERRIDE;
    virtual void resetMatchCount() OVERRIDE;
    virtual int findMatchMarkersVersion() const OVERRIDE;
    virtual WebFloatRect activeFindMatchRect() OVERRIDE;
    virtual void findMatchRects(WebVector<WebFloatRect>&) OVERRIDE;
    virtual int selectNearestFindMatch(const WebFloatPoint&, WebRect* selectionRect) OVERRIDE;

    virtual void sendOrientationChangeEvent(int orientation) OVERRIDE;

    virtual void dispatchMessageEventWithOriginCheck(
        const WebSecurityOrigin& intendedTargetOrigin,
        const WebDOMEvent&) OVERRIDE;

    virtual WebString contentAsText(size_t maxChars) const OVERRIDE;
    virtual WebString contentAsMarkup() const OVERRIDE;
    virtual WebString renderTreeAsText(RenderAsTextControls toShow = RenderAsTextNormal) const OVERRIDE;
    virtual WebString markerTextForListItem(const WebElement&) const OVERRIDE;
    virtual WebRect selectionBoundsRect() const OVERRIDE;

    virtual bool selectionStartHasSpellingMarkerFor(int from, int length) const OVERRIDE;
    virtual WebString layerTreeAsText(bool showDebugInfo = false) const OVERRIDE;

    void willDetachParent();

    static WebFrameImpl* create(WebFrameClient*);
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
    WebFrameImpl* activeMatchFrame() const { return m_currentActiveMatchFrame; }

    // Returns the active match in the current frame. Could be a null range if
    // the local frame has no active match.
    WebCore::Range* activeMatch() const { return m_activeMatch.get(); }

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
    virtual void setCanHaveScrollbars(bool) OVERRIDE;

    WebCore::Frame* frame() const { return m_frame.get(); }
    WebFrameClient* client() const { return m_client; }
    void setClient(WebFrameClient* client) { m_client = client; }

    WebPermissionClient* permissionClient() { return m_permissionClient; }
    SharedWorkerRepositoryClientImpl* sharedWorkerRepositoryClient() const { return m_sharedWorkerRepositoryClient.get(); }

    void setInputEventsTransformForEmulation(const WebCore::IntSize&, float);

    static void selectWordAroundPosition(WebCore::Frame*, WebCore::VisiblePosition);

private:
    class DeferredScopeStringMatches;
    friend class DeferredScopeStringMatches;
    friend class FrameLoaderClientImpl;

    struct FindMatch {
        RefPtr<WebCore::Range> m_range;

        // 1-based index within this frame.
        int m_ordinal;

        // In find-in-page coordinates.
        // Lazily calculated by updateFindMatchRects.
        WebCore::FloatRect m_rect;

        FindMatch(PassRefPtr<WebCore::Range>, int ordinal);
    };

    // A bit mask specifying area of the frame to invalidate.
    enum AreaToInvalidate {
      InvalidateNothing,
      InvalidateContentArea,
      InvalidateScrollbar,   // Vertical scrollbar only.
      InvalidateAll          // Both content area and the scrollbar.
    };

    explicit WebFrameImpl(WebFrameClient*);

    // Sets the local WebCore frame and registers destruction observers.
    void setWebCoreFrame(PassRefPtr<WebCore::Frame>);

    // Notifies the delegate about a new selection rect.
    void reportFindInPageSelection(
        const WebRect& selectionRect, int activeMatchOrdinal, int identifier);

    // Clear the find-in-page matches cache forcing rects to be fully
    // calculated again next time updateFindMatchRects is called.
    void clearFindMatchesCache();

    // Check if the activeMatchFrame still exists in the frame tree.
    bool isActiveMatchFrameValid() const;

    // Return the index in the find-in-page cache of the match closest to the
    // provided point in find-in-page coordinates, or -1 in case of error.
    // The squared distance to the closest match is returned in the distanceSquared parameter.
    int nearestFindMatch(const WebCore::FloatPoint&, float& distanceSquared);

    // Select a find-in-page match marker in the current frame using a cache
    // match index returned by nearestFindMatch. Returns the ordinal of the new
    // selected match or -1 in case of error. Also provides the bounding box of
    // the marker in window coordinates if selectionRect is not null.
    int selectFindMatch(unsigned index, WebRect* selectionRect);

    // Compute and cache the rects for FindMatches if required.
    // Rects are automatically invalidated in case of content size changes,
    // propagating the invalidation to child frames.
    void updateFindMatchRects();

    // Append the find-in-page match rects of the current frame to the provided vector.
    void appendFindMatchRects(Vector<WebFloatRect>& frameRects);

    // Invalidates a certain area within the frame.
    void invalidateArea(AreaToInvalidate);

    // Add a WebKit TextMatch-highlight marker to nodes in a range.
    void addMarker(WebCore::Range*, bool activeMatch);

    // Sets the markers within a range as active or inactive.
    void setMarkerActive(WebCore::Range*, bool active);

    // Returns the ordinal of the first match in the frame specified. This
    // function enumerates the frames, starting with the main frame and up to (but
    // not including) the frame passed in as a parameter and counts how many
    // matches have been found.
    int ordinalOfFirstMatchForFrame(WebFrameImpl*) const;

    // Determines whether the scoping effort is required for a particular frame.
    // It is not necessary if the frame is invisible, for example, or if this
    // is a repeat search that already returned nothing last time the same prefix
    // was searched.
    bool shouldScopeMatches(const WTF::String& searchText);

    // Removes the current frame from the global scoping effort and triggers any
    // updates if appropriate. This method does not mark the scoping operation
    // as finished.
    void flushCurrentScopingEffort(int identifier);

    // Finishes the current scoping effort and triggers any updates if appropriate.
    void finishCurrentScopingEffort(int identifier);

    // Queue up a deferred call to scopeStringMatches.
    void scopeStringMatchesSoon(
        int identifier, const WebString& searchText, const WebFindOptions&,
        bool reset);

    // Called by a DeferredScopeStringMatches instance.
    void callScopeStringMatches(
        DeferredScopeStringMatches*, int identifier, const WebString& searchText,
        const WebFindOptions&, bool reset);

    // Determines whether to invalidate the content area and scrollbar.
    void invalidateIfNecessary();

    void loadJavaScriptURL(const WebCore::KURL&);

    // Returns a hit-tested VisiblePosition for the given point
    WebCore::VisiblePosition visiblePositionForWindowPoint(const WebPoint&);

    class WebFrameInit : public WebCore::FrameInit {
    public:
        static PassRefPtr<WebFrameInit> create(WebFrameImpl* webFrameImpl)
        {
            return adoptRef(new WebFrameInit(webFrameImpl));
        }

    private:
        explicit WebFrameInit(WebFrameImpl* webFrameImpl)
            : m_frameLoaderClientImpl(webFrameImpl)
        {
            setFrameLoaderClient(&m_frameLoaderClientImpl);
        }

        FrameLoaderClientImpl m_frameLoaderClientImpl;
    };
    RefPtr<WebFrameInit> m_frameInit;

    // The embedder retains a reference to the WebCore Frame while it is active in the DOM. This
    // reference is released when the frame is removed from the DOM or the entire page is closed.
    RefPtr<WebCore::Frame> m_frame;
    WebFrameImpl* m_parent;
    WebFrameImpl* m_previousSibling;
    WebFrameImpl* m_nextSibling;
    WebFrameImpl* m_firstChild;
    WebFrameImpl* m_lastChild;

    // Indicate whether the current Frame is local or remote. Remote frames are
    // rendered in a different process from their parent frames.
    bool m_isRemote;

    WebFrameClient* m_client;
    WebPermissionClient* m_permissionClient;
    OwnPtr<SharedWorkerRepositoryClientImpl> m_sharedWorkerRepositoryClient;

    // A way for the main frame to keep track of which frame has an active
    // match. Should be 0 for all other frames.
    WebFrameImpl* m_currentActiveMatchFrame;

    // The range of the active match for the current frame.
    RefPtr<WebCore::Range> m_activeMatch;

    // The index of the active match for the current frame.
    int m_activeMatchIndexInCurrentFrame;

    // This flag is used by the scoping effort to determine if we need to figure
    // out which rectangle is the active match. Once we find the active
    // rectangle we clear this flag.
    bool m_locatingActiveRect;

    // The scoping effort can time out and we need to keep track of where we
    // ended our last search so we can continue from where we left of.
    RefPtr<WebCore::Range> m_resumeScopingFromRange;

    // Keeps track of the last string this frame searched for. This is used for
    // short-circuiting searches in the following scenarios: When a frame has
    // been searched and returned 0 results, we don't need to search that frame
    // again if the user is just adding to the search (making it more specific).
    WTF::String m_lastSearchString;

    // Keeps track of how many matches this frame has found so far, so that we
    // don't loose count between scoping efforts, and is also used (in conjunction
    // with m_lastSearchString) to figure out if we need to search the frame again.
    int m_lastMatchCount;

    // This variable keeps a cumulative total of matches found so far for ALL the
    // frames on the page, and is only incremented by calling IncreaseMatchCount
    // (on the main frame only). It should be -1 for all other frames.
    int m_totalMatchCount;

    // This variable keeps a cumulative total of how many frames are currently
    // scoping, and is incremented/decremented on the main frame only.
    // It should be -1 for all other frames.
    int m_framesScopingCount;

    // Identifier of the latest find-in-page request. Required to be stored in
    // the frame in order to reply if required in case the frame is detached.
    int m_findRequestIdentifier;

    // Keeps track of whether there is an scoping effort ongoing in the frame.
    bool m_scopingInProgress;

    // Keeps track of whether the last find request completed its scoping effort
    // without finding any matches in this frame.
    bool m_lastFindRequestCompletedWithNoMatches;

    // Keeps track of when the scoping effort should next invalidate the scrollbar
    // and the frame area.
    int m_nextInvalidateAfter;

    // A list of all of the pending calls to scopeStringMatches.
    Vector<DeferredScopeStringMatches*> m_deferredScopingWork;

    // Version number incremented on the main frame only whenever the document
    // find-in-page match markers change. It should be 0 for all other frames.
    int m_findMatchMarkersVersion;

    // Local cache of the find match markers currently displayed for this frame.
    Vector<FindMatch> m_findMatchesCache;

    // Determines if the rects in the find-in-page matches cache of this frame
    // are invalid and should be recomputed.
    bool m_findMatchRectsAreValid;

    // Contents size when find-in-page match rects were last computed for this
    // frame's cache.
    WebCore::IntSize m_contentsSizeForCurrentFindMatchRects;

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
