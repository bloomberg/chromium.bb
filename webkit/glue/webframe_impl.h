/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef WEBKIT_GLUE_WEBFRAME_IMPL_H_
#define WEBKIT_GLUE_WEBFRAME_IMPL_H_

#include "Frame.h"
#include "PlatformString.h"
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>

#include "webkit/api/public/WebFrame.h"
#include "webkit/glue/webframeloaderclient_impl.h"

class ChromePrintContext;
class WebViewImpl;

namespace gfx {
class BitmapPlatformDevice;
}

namespace WebCore {
class HistoryItem;
class KURL;
class Node;
class Range;
class SubstituteData;
struct WindowFeatures;
}

namespace WebKit {
class PasswordAutocompleteListener;
class WebDataSourceImpl;
class WebFrameClient;
class WebView;
}

// Implementation of WebFrame, note that this is a reference counted object.
class WebFrameImpl : public WebKit::WebFrame, public RefCounted<WebFrameImpl> {
 public:
  // WebFrame methods:
  virtual WebKit::WebString name() const;
  virtual WebKit::WebURL url() const;
  virtual WebKit::WebURL favIconURL() const;
  virtual WebKit::WebURL openSearchDescriptionURL() const;
  virtual WebKit::WebSize scrollOffset() const;
  virtual WebKit::WebSize contentsSize() const;
  virtual int contentsPreferredWidth() const;
  virtual bool hasVisibleContent() const;
  virtual WebKit::WebView* view() const;
  virtual WebKit::WebFrame* opener() const;
  virtual WebKit::WebFrame* parent() const;
  virtual WebKit::WebFrame* top() const;
  virtual WebKit::WebFrame* firstChild() const;
  virtual WebKit::WebFrame* lastChild() const;
  virtual WebKit::WebFrame* nextSibling() const;
  virtual WebKit::WebFrame* previousSibling() const;
  virtual WebKit::WebFrame* traverseNext(bool wrap) const;
  virtual WebKit::WebFrame* traversePrevious(bool wrap) const;
  virtual WebKit::WebFrame* findChildByName(const WebKit::WebString& name) const;
  virtual WebKit::WebFrame* findChildByExpression(
      const WebKit::WebString& xpath) const;
  virtual void forms(WebKit::WebVector<WebKit::WebForm>&) const;
  virtual WebKit::WebSecurityOrigin securityOrigin() const;
  virtual void grantUniversalAccess();
  virtual NPObject* windowObject() const;
  virtual void bindToWindowObject(
      const WebKit::WebString& name, NPObject* object);
  virtual void executeScript(const WebKit::WebScriptSource&);
  virtual void executeScriptInNewContext(
      const WebKit::WebScriptSource* sources, unsigned num_sources,
      int extension_group);
  virtual void executeScriptInIsolatedWorld(
      int world_id,  const WebKit::WebScriptSource* sources,
      unsigned num_sources, int extension_group);
  virtual void addMessageToConsole(const WebKit::WebConsoleMessage&);
  virtual void collectGarbage();
#if WEBKIT_USING_V8
  virtual v8::Local<v8::Context> mainWorldScriptContext() const;
#endif
  virtual bool insertStyleText(
      const WebKit::WebString& css, const WebKit::WebString& id);
  virtual void reload();
  virtual void loadRequest(const WebKit::WebURLRequest& request);
  virtual void loadHistoryItem(const WebKit::WebHistoryItem& history_item);
  virtual void loadData(
      const WebKit::WebData& data, const WebKit::WebString& mime_type,
      const WebKit::WebString& text_encoding, const WebKit::WebURL& base_url,
      const WebKit::WebURL& unreachable_url, bool replace);
  virtual void loadHTMLString(
      const WebKit::WebData& html, const WebKit::WebURL& base_url,
      const WebKit::WebURL& unreachable_url, bool replace);
  virtual bool isLoading() const;
  virtual void stopLoading();
  virtual WebKit::WebDataSource* provisionalDataSource() const;
  virtual WebKit::WebDataSource* dataSource() const;
  virtual WebKit::WebHistoryItem previousHistoryItem() const;
  virtual WebKit::WebHistoryItem currentHistoryItem() const;
  virtual void enableViewSourceMode(bool enable);
  virtual bool isViewSourceModeEnabled() const;
  virtual void setReferrerForRequest(
      WebKit::WebURLRequest& request, const WebKit::WebURL& referrer);
  virtual void dispatchWillSendRequest(WebKit::WebURLRequest& request);
  virtual void commitDocumentData(const char* data, size_t length);
  virtual unsigned unloadListenerCount() const;
  virtual bool isProcessingUserGesture() const;
  virtual bool willSuppressOpenerInNewFrame() const;
  virtual void replaceSelection(const WebKit::WebString& text);
  virtual void insertText(const WebKit::WebString& text);
  virtual void setMarkedText(
      const WebKit::WebString& text, unsigned location, unsigned length);
  virtual void unmarkText();
  virtual bool hasMarkedText() const;
  virtual WebKit::WebRange markedRange() const;
  virtual bool executeCommand(const WebKit::WebString& command);
  virtual bool executeCommand(
      const WebKit::WebString& command, const WebKit::WebString& value);
  virtual bool isCommandEnabled(const WebKit::WebString& command) const;
  virtual void enableContinuousSpellChecking(bool enable);
  virtual bool isContinuousSpellCheckingEnabled() const;
  virtual bool hasSelection() const;
  virtual WebKit::WebRange selectionRange() const;
  virtual WebKit::WebString selectionAsText() const;
  virtual WebKit::WebString selectionAsMarkup() const;
  virtual int printBegin(const WebKit::WebSize& page_size);
  virtual float printPage(int page_to_print, WebKit::WebCanvas* canvas);
  virtual float getPrintPageShrink(int page);
  virtual void printEnd();
  virtual bool find(
      int identifier, const WebKit::WebString& search_text,
      const WebKit::WebFindOptions& options, bool wrap_within_frame,
      WebKit::WebRect* selection_rect);
  virtual void stopFinding(bool clear_selection);
  virtual void scopeStringMatches(
      int identifier, const WebKit::WebString& search_text,
      const WebKit::WebFindOptions& options, bool reset);
  virtual void cancelPendingScopingEffort();
  virtual void increaseMatchCount(int count, int identifier);
  virtual void resetMatchCount();
  virtual WebKit::WebURL completeURL(const WebKit::WebString& url) const;
  virtual WebKit::WebString contentAsText(size_t max_chars) const;
  virtual WebKit::WebString contentAsMarkup() const;

  static PassRefPtr<WebFrameImpl> create(WebKit::WebFrameClient* client);
  ~WebFrameImpl();

  static int live_object_count() {
    return live_object_count_;
  }

  // Called by the WebViewImpl to initialize its main frame:
  void InitMainFrame(WebViewImpl* webview_impl);

  PassRefPtr<WebCore::Frame> CreateChildFrame(
      const WebCore::FrameLoadRequest&,
      WebCore::HTMLFrameOwnerElement* owner_element);

  void Layout();
  void Paint(WebKit::WebCanvas* canvas, const WebKit::WebRect& rect);

  void CreateFrameView();

  WebCore::Frame* frame() const {
    return frame_;
  }

  static WebFrameImpl* FromFrame(WebCore::Frame* frame);

  WebViewImpl* GetWebViewImpl() const;

  WebCore::FrameView* frameview() const {
    return frame_ ? frame_->view() : NULL;
  }

  // Getters for the impls corresponding to Get(Provisional)DataSource. They
  // may return NULL if there is no corresponding data source.
  WebKit::WebDataSourceImpl* GetDataSourceImpl() const;
  WebKit::WebDataSourceImpl* GetProvisionalDataSourceImpl() const;

  // Returns which frame has an active match. This function should only be
  // called on the main frame, as it is the only frame keeping track. Returned
  // value can be NULL if no frame has an active match.
  const WebFrameImpl* active_match_frame() const {
    return active_match_frame_;
  }

  // When a Find operation ends, we want to set the selection to what was active
  // and set focus to the first focusable node we find (starting with the first
  // node in the matched range and going up the inheritance chain). If we find
  // nothing to focus we focus the first focusable node in the range. This
  // allows us to set focus to a link (when we find text inside a link), which
  // allows us to navigate by pressing Enter after closing the Find box.
  void SetFindEndstateFocusAndSelection();

  void DidFail(const WebCore::ResourceError& error, bool was_provisional);

  // Sets whether the WebFrameImpl allows its document to be scrolled.
  // If the parameter is true, allow the document to be scrolled.
  // Otherwise, disallow scrolling.
  void SetAllowsScrolling(bool flag);

  // Registers a listener for the specified user name input element.  The
  // listener will receive notifications for blur and when autocomplete should
  // be triggered.
  // The WebFrameImpl becomes the owner of the passed listener.
  void RegisterPasswordListener(
      PassRefPtr<WebCore::HTMLInputElement> user_name_input_element,
      WebKit::PasswordAutocompleteListener* listener);

  // Returns the password autocomplete listener associated with the passed
  // user name input element, or NULL if none available.
  // Note that the returned listener is owner by the WebFrameImpl and should not
  // be kept around as it is deleted when the page goes away.
  WebKit::PasswordAutocompleteListener* GetPasswordListener(
      WebCore::HTMLInputElement* user_name_input_element);

  WebKit::WebFrameClient* client() const { return client_handle_->client(); }
  void drop_client() { client_handle_->drop_client(); }

 protected:
  friend class WebFrameLoaderClient;

  // A weak reference to the WebFrameClient.  Each WebFrame in the hierarchy
  // owns a reference to a ClientHandle.  When the main frame is destroyed, it
  // clears the WebFrameClient.
  class ClientHandle : public RefCounted<ClientHandle> {
   public:
    static PassRefPtr<ClientHandle> create(WebKit::WebFrameClient* client) {
      return adoptRef(new ClientHandle(client));
    }
    WebKit::WebFrameClient* client() { return client_; }
    void drop_client() { client_ = NULL; }
   private:
    ClientHandle(WebKit::WebFrameClient* client) : client_(client) {}
    WebKit::WebFrameClient* client_;
  };

  WebFrameImpl(PassRefPtr<ClientHandle> client_handle);

  // Informs the WebFrame that the Frame is being closed, called by the
  // WebFrameLoaderClient
  void Closing();

  // Used to check for leaks of this object.
  static int live_object_count_;

  WebFrameLoaderClient frame_loader_client_;

  RefPtr<ClientHandle> client_handle_;

  // This is a weak pointer to our corresponding WebCore frame.  A reference to
  // ourselves is held while frame_ is valid.  See our Closing method.
  WebCore::Frame* frame_;

  // A way for the main frame to keep track of which frame has an active
  // match. Should be NULL for all other frames.
  WebFrameImpl* active_match_frame_;

  // The range of the active match for the current frame.
  RefPtr<WebCore::Range> active_match_;

  // The index of the active match.
  int active_match_index_;

  // This flag is used by the scoping effort to determine if we need to figure
  // out which rectangle is the active match. Once we find the active
  // rectangle we clear this flag.
  bool locating_active_rect_;

  // The scoping effort can time out and we need to keep track of where we
  // ended our last search so we can continue from where we left of.
  RefPtr<WebCore::Range> resume_scoping_from_range_;

  // Keeps track of the last string this frame searched for. This is used for
  // short-circuiting searches in the following scenarios: When a frame has
  // been searched and returned 0 results, we don't need to search that frame
  // again if the user is just adding to the search (making it more specific).
  string16 last_search_string_;

  // Keeps track of how many matches this frame has found so far, so that we
  // don't loose count between scoping efforts, and is also used (in conjunction
  // with last_search_string_ and scoping_complete_) to figure out if we need to
  // search the frame again.
  int last_match_count_;

  // This variable keeps a cumulative total of matches found so far for ALL the
  // frames on the page, and is only incremented by calling IncreaseMatchCount
  // (on the main frame only). It should be -1 for all other frames.
  size_t total_matchcount_;

  // This variable keeps a cumulative total of how many frames are currently
  // scoping, and is incremented/decremented on the main frame only.
  // It should be -1 for all other frames.
  int frames_scoping_count_;

  // Keeps track of whether the scoping effort was completed (the user may
  // interrupt it before it completes by submitting a new search).
  bool scoping_complete_;

  // Keeps track of when the scoping effort should next invalidate the scrollbar
  // and the frame area.
  int next_invalidate_after_;

 private:
  class DeferredScopeStringMatches;
  friend class DeferredScopeStringMatches;

  // A bit mask specifying area of the frame to invalidate.
  enum AreaToInvalidate {
    INVALIDATE_NOTHING      = 0,
    INVALIDATE_CONTENT_AREA = 1,
    INVALIDATE_SCROLLBAR    = 2,  // vertical scrollbar only.
    INVALIDATE_ALL          = 3   // both content area and the scrollbar.
  };

  // Notifies the delegate about a new selection rect.
  void ReportFindInPageSelection(
      const WebKit::WebRect& selection_rect, int active_match_ordinal,
      int identifier);

  // Invalidates a certain area within the frame.
  void InvalidateArea(AreaToInvalidate area);

  // Add a WebKit TextMatch-highlight marker to nodes in a range.
  void AddMarker(WebCore::Range* range, bool active_match);

  // Sets the markers within a range as active or inactive.
  void SetMarkerActive(WebCore::Range* range, bool active);

  // Returns the ordinal of the first match in the frame specified. This
  // function enumerates the frames, starting with the main frame and up to (but
  // not including) the frame passed in as a parameter and counts how many
  // matches have been found.
  int OrdinalOfFirstMatchForFrame(WebFrameImpl* frame) const;

  // Determines whether the scoping effort is required for a particular frame.
  // It is not necessary if the frame is invisible, for example, or if this
  // is a repeat search that already returned nothing last time the same prefix
  // was searched.
  bool ShouldScopeMatches(const string16& search_text);

  // Queue up a deferred call to scopeStringMatches.
  void ScopeStringMatchesSoon(
      int identifier, const WebKit::WebString& search_text,
      const WebKit::WebFindOptions& options, bool reset);

  // Called by a DeferredScopeStringMatches instance.
  void CallScopeStringMatches(
      DeferredScopeStringMatches* deferred,
      int identifier, const WebKit::WebString& search_text,
      const WebKit::WebFindOptions& options, bool reset);

  // Determines whether to invalidate the content area and scrollbar.
  void InvalidateIfNecessary();

  // Clears the map of password listeners.
  void ClearPasswordListeners();

  void LoadJavaScriptURL(const WebCore::KURL& url);

  // A list of all of the pending calls to scopeStringMatches.
  Vector<DeferredScopeStringMatches*> deferred_scoping_work_;

  // Valid between calls to BeginPrint() and EndPrint(). Containts the print
  // information. Is used by PrintPage().
  OwnPtr<ChromePrintContext> print_context_;

  // The input fields that are interested in edit events and their associated
  // listeners.
  typedef HashMap<RefPtr<WebCore::HTMLInputElement>,
      WebKit::PasswordAutocompleteListener*> PasswordListenerMap;
  PasswordListenerMap password_listeners_;
};

#endif  // WEBKIT_GLUE_WEBFRAME_IMPL_H_
