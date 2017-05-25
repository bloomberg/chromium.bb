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

#ifndef WebFrame_h
#define WebFrame_h

#include <memory>
#include "WebIconURL.h"
#include "WebNode.h"
#include "public/platform/WebCanvas.h"
#include "public/platform/WebFeaturePolicy.h"
#include "public/platform/WebInsecureRequestPolicy.h"
#include "public/web/WebFrameLoadType.h"
#include "public/web/WebTreeScopeType.h"

namespace v8 {
class Context;
class Function;
class Value;
template <class T>
class Local;
}

namespace blink {

class Frame;
class OpenedFrameTracker;
class Page;
class Visitor;
class WebAssociatedURLLoader;
struct WebAssociatedURLLoaderOptions;
class WebDOMEvent;
class WebData;
class WebDataSource;
class WebDocument;
class WebElement;
class WebLocalFrame;
class WebPerformance;
class WebRemoteFrame;
class WebSecurityOrigin;
class WebSharedWorkerRepositoryClient;
class WebString;
class WebURL;
class WebURLRequest;
class WebView;
enum class WebSandboxFlags;
struct WebFrameOwnerProperties;
struct WebPrintParams;
struct WebRect;
struct WebScriptSource;
struct WebSize;

template <typename T>
class WebVector;

// Frames may be rendered in process ('local') or out of process ('remote').
// A remote frame is always cross-site; a local frame may be either same-site or
// cross-site.
// WebFrame is the base class for both WebLocalFrame and WebRemoteFrame and
// contains methods that are valid on both local and remote frames, such as
// getting a frame's parent or its opener.
class WebFrame {
 public:
  // FIXME: We already have blink::TextGranularity. For now we support only
  // a part of blink::TextGranularity.
  // Ideally it seems blink::TextGranularity should be broken up into
  // blink::TextGranularity and perhaps blink::TextBoundary and then
  // TextGranularity enum could be moved somewhere to public/, and we could
  // just use it here directly without introducing a new enum.
  enum TextGranularity {
    kCharacterGranularity = 0,
    kWordGranularity,
    kTextGranularityLast = kWordGranularity,
  };

  // Returns the number of live WebFrame objects, used for leak checking.
  BLINK_EXPORT static int InstanceCount();

  virtual bool IsWebLocalFrame() const = 0;
  virtual WebLocalFrame* ToWebLocalFrame() = 0;
  virtual bool IsWebRemoteFrame() const = 0;
  virtual WebRemoteFrame* ToWebRemoteFrame() = 0;

  BLINK_EXPORT bool Swap(WebFrame*);

  // This method closes and deletes the WebFrame. This is typically called by
  // the embedder in response to a frame detached callback to the WebFrame
  // client.
  virtual void Close();

  // Called by the embedder when it needs to detach the subtree rooted at this
  // frame.
  BLINK_EXPORT void Detach();

  // Basic properties ---------------------------------------------------

  // The name of this frame. If no name is given, empty string is returned.
  virtual WebString AssignedName() const = 0;

  // Sets the name of this frame. For child frames (frames that are not a
  // top-most frame) the actual name may have a suffix appended to make the
  // frame name unique within the hierarchy.
  virtual void SetName(const WebString&) = 0;

  // The urls of the given combination types of favicon (if any) specified by
  // the document loaded in this frame. The iconTypesMask is a bit-mask of
  // WebIconURL::Type values, used to select from the available set of icon
  // URLs
  virtual WebVector<WebIconURL> IconURLs(int icon_types_mask) const = 0;

  // Initializes the various client interfaces.
  virtual void SetSharedWorkerRepositoryClient(
      WebSharedWorkerRepositoryClient*) = 0;

  // The security origin of this frame.
  BLINK_EXPORT WebSecurityOrigin GetSecurityOrigin() const;

  // Updates the snapshotted policy attributes (sandbox flags and feature policy
  // container policy) in the frame's FrameOwner. This is used when this frame's
  // parent is in another process and it dynamically updates this frame's
  // sandbox flags or container policy. The new policy won't take effect until
  // the next navigation.
  BLINK_EXPORT void SetFrameOwnerPolicy(WebSandboxFlags,
                                        const blink::WebParsedFeaturePolicy&);

  // The frame's insecure request policy.
  BLINK_EXPORT WebInsecureRequestPolicy GetInsecureRequestPolicy() const;

  // Updates this frame's FrameOwner properties, such as scrolling, margin,
  // or allowfullscreen.  This is used when this frame's parent is in
  // another process and it dynamically updates these properties.
  // TODO(dcheng): Currently, the update only takes effect on next frame
  // navigation.  This matches the in-process frame behavior.
  BLINK_EXPORT void SetFrameOwnerProperties(const WebFrameOwnerProperties&);

  // Geometry -----------------------------------------------------------

  // NOTE: These routines do not force page layout so their results may
  // not be accurate if the page layout is out-of-date.

  // If set to false, do not draw scrollbars on this frame's view.
  virtual void SetCanHaveScrollbars(bool) = 0;

  // The scroll offset from the top-left corner of the frame in pixels.
  virtual WebSize GetScrollOffset() const = 0;
  virtual void SetScrollOffset(const WebSize&) = 0;

  // The size of the contents area.
  virtual WebSize ContentsSize() const = 0;

  // Returns true if the contents (minus scrollbars) has non-zero area.
  virtual bool HasVisibleContent() const = 0;

  // Returns the visible content rect (minus scrollbars, in absolute coordinate)
  virtual WebRect VisibleContentRect() const = 0;

  virtual bool HasHorizontalScrollbar() const = 0;
  virtual bool HasVerticalScrollbar() const = 0;

  // Whether to collapse the frame's owner element in the embedder document,
  // that is, to remove it from the layout as if it did not exist. Only works
  // for <iframe> owner elements.
  BLINK_EXPORT void Collapse(bool);

  // Hierarchy ----------------------------------------------------------

  // Returns the containing view.
  virtual WebView* View() const = 0;

  // Returns the frame that opened this frame or 0 if there is none.
  BLINK_EXPORT WebFrame* Opener() const;

  // Sets the frame that opened this one or 0 if there is none.
  BLINK_EXPORT void SetOpener(WebFrame*);

  // Reset the frame that opened this frame to 0.
  // This is executed between layout tests runs
  void ClearOpener() { SetOpener(0); }

  // Inserts the given frame as a child of this frame, so that it is the next
  // child after |previousSibling|, or first child if |previousSibling| is null.
  BLINK_EXPORT void InsertAfter(WebFrame* child, WebFrame* previous_sibling);

  // Adds the given frame as a child of this frame.
  BLINK_EXPORT void AppendChild(WebFrame*);

  // Removes the given child from this frame.
  BLINK_EXPORT void RemoveChild(WebFrame*);

  // Returns the parent frame or 0 if this is a top-most frame.
  BLINK_EXPORT WebFrame* Parent() const;

  // Returns the top-most frame in the hierarchy containing this frame.
  BLINK_EXPORT WebFrame* Top() const;

  // Returns the first child frame.
  BLINK_EXPORT WebFrame* FirstChild() const;

  // Returns the next sibling frame.
  BLINK_EXPORT WebFrame* NextSibling() const;

  // Returns the next frame in "frame traversal order".
  BLINK_EXPORT WebFrame* TraverseNext() const;

  // Content ------------------------------------------------------------

  virtual WebDocument GetDocument() const = 0;

  virtual WebPerformance Performance() const = 0;

  // Closing -------------------------------------------------------------

  // Runs unload handlers for this frame.
  virtual void DispatchUnloadEvent() = 0;

  // Scripting ----------------------------------------------------------

  // Executes script in the context of the current page.
  virtual void ExecuteScript(const WebScriptSource&) = 0;

  // Executes JavaScript in a new world associated with the web frame.
  // The script gets its own global scope and its own prototypes for
  // intrinsic JavaScript objects (String, Array, and so-on). It also
  // gets its own wrappers for all DOM nodes and DOM constructors.
  //
  // worldID must be > 0 (as 0 represents the main world).
  // worldID must be < EmbedderWorldIdLimit, high number used internally.
  virtual void ExecuteScriptInIsolatedWorld(int world_id,
                                            const WebScriptSource* sources,
                                            unsigned num_sources) = 0;

  // Associates an isolated world (see above for description) with a security
  // origin. XMLHttpRequest instances used in that world will be considered
  // to come from that origin, not the frame's.
  virtual void SetIsolatedWorldSecurityOrigin(int world_id,
                                              const WebSecurityOrigin&) = 0;

  // Associates a content security policy with an isolated world. This policy
  // should be used when evaluating script in the isolated world, and should
  // also replace a protected resource's CSP when evaluating resources
  // injected into the DOM.
  //
  // FIXME: Setting this simply bypasses the protected resource's CSP. It
  //     doesn't yet restrict the isolated world to the provided policy.
  virtual void SetIsolatedWorldContentSecurityPolicy(int world_id,
                                                     const WebString&) = 0;

  // Calls window.gc() if it is defined.
  virtual void CollectGarbage() = 0;

  // Executes script in the context of the current page and returns the value
  // that the script evaluated to.
  // DEPRECATED: Use WebLocalFrame::requestExecuteScriptAndReturnValue.
  virtual v8::Local<v8::Value> ExecuteScriptAndReturnValue(
      const WebScriptSource&) = 0;

  // worldID must be > 0 (as 0 represents the main world).
  // worldID must be < EmbedderWorldIdLimit, high number used internally.
  // DEPRECATED: Use WebLocalFrame::requestExecuteScriptInIsolatedWorld.
  virtual void ExecuteScriptInIsolatedWorld(
      int world_id,
      const WebScriptSource* sources_in,
      unsigned num_sources,
      WebVector<v8::Local<v8::Value>>* results) = 0;

  // Call the function with the given receiver and arguments, bypassing
  // canExecute().
  virtual v8::Local<v8::Value> CallFunctionEvenIfScriptDisabled(
      v8::Local<v8::Function>,
      v8::Local<v8::Value>,
      int argc,
      v8::Local<v8::Value> argv[]) = 0;

  // Returns the V8 context for associated with the main world and this
  // frame. There can be many V8 contexts associated with this frame, one for
  // each isolated world and one for the main world. If you don't know what
  // the "main world" or an "isolated world" is, then you probably shouldn't
  // be calling this API.
  virtual v8::Local<v8::Context> MainWorldScriptContext() const = 0;

  // Returns true if the WebFrame currently executing JavaScript has access
  // to the given WebFrame, or false otherwise.
  BLINK_EXPORT static bool ScriptCanAccess(WebFrame*);

  // Navigation ----------------------------------------------------------
  // TODO(clamy): Remove the reload, reloadWithOverrideURL, and loadRequest
  // functions once RenderFrame only calls WebLoadFrame::load.

  // Reload the current document.
  // Note: reload() and reloadWithOverrideURL() will be deprecated.
  // Do not use these APIs any more, but use loadRequest() instead.
  virtual void Reload(WebFrameLoadType) = 0;

  // This is used for situations where we want to reload a different URL because
  // of a redirect.
  virtual void ReloadWithOverrideURL(const WebURL& override_url,
                                     WebFrameLoadType) = 0;

  // Load the given URL.
  virtual void LoadRequest(const WebURLRequest&) = 0;

  // This method is short-hand for calling LoadData, where mime_type is
  // "text/html" and text_encoding is "UTF-8".
  virtual void LoadHTMLString(const WebData& html,
                              const WebURL& base_url,
                              const WebURL& unreachable_url = WebURL(),
                              bool replace = false) = 0;

  // Stops any pending loads on the frame and its children.
  virtual void StopLoading() = 0;

  // Returns the data source that is currently loading.  May be null.
  virtual WebDataSource* ProvisionalDataSource() const = 0;

  // Returns the data source that is currently loaded.
  virtual WebDataSource* DataSource() const = 0;

  // View-source rendering mode.  Set this before loading an URL to cause
  // it to be rendered in view-source mode.
  virtual void EnableViewSourceMode(bool) = 0;
  virtual bool IsViewSourceModeEnabled() const = 0;

  // Sets the referrer for the given request to be the specified URL or
  // if that is null, then it sets the referrer to the referrer that the
  // frame would use for subresources.  NOTE: This method also filters
  // out invalid referrers (e.g., it is invalid to send a HTTPS URL as
  // the referrer for a HTTP request).
  virtual void SetReferrerForRequest(WebURLRequest&, const WebURL&) = 0;

  // Returns an AssociatedURLLoader that is associated with this frame.  The
  // loader will, for example, be cancelled when WebFrame::stopLoading is
  // called.
  //
  // FIXME: stopLoading does not yet cancel an associated loader!!
  virtual WebAssociatedURLLoader* CreateAssociatedURLLoader(
      const WebAssociatedURLLoaderOptions&) = 0;

  // Returns the number of registered unload listeners.
  virtual unsigned UnloadListenerCount() const = 0;

  // Will return true if between didStartLoading and didStopLoading
  // notifications.
  virtual bool IsLoading() const;

  // Printing ------------------------------------------------------------

  // Reformats the WebFrame for printing. WebPrintParams specifies the printable
  // content size, paper size, printable area size, printer DPI and print
  // scaling option. If constrainToNode node is specified, then only the given
  // node is printed (for now only plugins are supported), instead of the entire
  // frame.
  // Returns the number of pages that can be printed at the given
  // page size.
  virtual int PrintBegin(const WebPrintParams&,
                         const WebNode& constrain_to_node = WebNode()) = 0;

  // Returns the page shrinking factor calculated by webkit (usually
  // between 1/1.33 and 1/2). Returns 0 if the page number is invalid or
  // not in printing mode.
  virtual float GetPrintPageShrink(int page) = 0;

  // Prints one page, and returns the calculated page shrinking factor
  // (usually between 1/1.33 and 1/2).  Returns 0 if the page number is
  // invalid or not in printing mode.
  virtual float PrintPage(int page_to_print, WebCanvas*) = 0;

  // Reformats the WebFrame for screen display.
  virtual void PrintEnd() = 0;

  // If the frame contains a full-frame plugin or the given node refers to a
  // plugin whose content indicates that printed output should not be scaled,
  // return true, otherwise return false.
  virtual bool IsPrintScalingDisabledForPlugin(const WebNode& = WebNode()) = 0;

  // Events --------------------------------------------------------------

  // Dispatches a message event on the current DOMWindow in this WebFrame.
  virtual void DispatchMessageEventWithOriginCheck(
      const WebSecurityOrigin& intended_target_origin,
      const WebDOMEvent&) = 0;

  // Utility -------------------------------------------------------------

  // Prints all of the pages into the canvas, with page boundaries drawn as
  // one pixel wide blue lines. This method exists to support layout tests.
  virtual void PrintPagesWithBoundaries(WebCanvas*, const WebSize&) = 0;

  // Returns the bounds rect for current selection. If selection is performed
  // on transformed text, the rect will still bound the selection but will
  // not be transformed itself. If no selection is present, the rect will be
  // empty ((0,0), (0,0)).
  virtual WebRect SelectionBoundsRect() const = 0;

  // Dumps the layer tree, used by the accelerated compositor, in
  // text form. This is used only by layout tests.
  virtual WebString LayerTreeAsText(bool show_debug_info = false) const = 0;

  // Returns the frame inside a given frame or iframe element. Returns 0 if
  // the given element is not a frame, iframe or if the frame is empty.
  BLINK_EXPORT static WebFrame* FromFrameOwnerElement(const WebElement&);

#if BLINK_IMPLEMENTATION
  // TODO(mustaq): Should be named FromCoreFrame instead.
  static WebFrame* FromFrame(Frame*);
  BLINK_EXPORT static Frame* ToCoreFrame(const WebFrame&);

  bool InShadowTree() const { return scope_ == WebTreeScopeType::kShadow; }

  static void InitializeCoreFrame(WebFrame&, Page&);
  static void TraceFrames(Visitor*, WebFrame*);
#endif

 protected:
  explicit WebFrame(WebTreeScopeType);
  virtual ~WebFrame();

  // Sets the parent WITHOUT fulling adding it to the frame tree.
  // Used to lie to a local frame that is replacing a remote frame,
  // so it can properly start a navigation but wait to swap until
  // commit-time.
  void SetParent(WebFrame*);

 private:
#if BLINK_IMPLEMENTATION
  friend class OpenedFrameTracker;
  friend class WebFrameTest;

  static void TraceFrame(Visitor*, WebFrame*);
#endif

  const WebTreeScopeType scope_;

  WebFrame* parent_;
  WebFrame* previous_sibling_;
  WebFrame* next_sibling_;
  WebFrame* first_child_;
  WebFrame* last_child_;

  WebFrame* opener_;
  std::unique_ptr<OpenedFrameTracker> opened_frame_tracker_;
};

}  // namespace blink

#endif
