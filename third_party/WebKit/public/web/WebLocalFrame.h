// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebLocalFrame_h
#define WebLocalFrame_h

#include <set>
#include "WebCompositionUnderline.h"
#include "WebFrame.h"
#include "WebFrameLoadType.h"
#include "WebHistoryItem.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebURLError.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/site_engagement.mojom-shared.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace blink {

class InterfaceProvider;
class InterfaceRegistry;
class WebAutofillClient;
class WebContentSettingsClient;
class WebDevToolsAgent;
class WebDevToolsAgentClient;
class WebDoubleSize;
class WebFrameClient;
class WebFrameWidget;
class WebInputMethodController;
class WebRange;
class WebScriptExecutionCallback;
class WebTextCheckClient;
class WebURLLoader;
enum class WebCachePolicy;
enum class WebSandboxFlags;
enum class WebTreeScopeType;
struct WebConsoleMessage;
struct WebContentSecurityPolicyViolation;
struct WebFindOptions;
struct WebFloatRect;
struct WebPrintPresetOptions;
struct WebSourceLocation;

// Interface for interacting with in process frames. This contains methods that
// require interacting with a frame's document.
// FIXME: Move lots of methods from WebFrame in here.
class WebLocalFrame : public WebFrame {
 public:
  // Creates a WebFrame. Delete this WebFrame by calling WebFrame::close().
  // WebFrameClient may not be null.
  BLINK_EXPORT static WebLocalFrame* Create(WebTreeScopeType,
                                            WebFrameClient*,
                                            blink::InterfaceProvider*,
                                            blink::InterfaceRegistry*,
                                            WebFrame* opener = nullptr);

  // Used to create a provisional local frame. Currently, it's possible for a
  // provisional navigation not to commit (i.e. it might turn into a download),
  // but this can only be determined by actually trying to load it. The loading
  // process depends on having a corresponding LocalFrame in Blink to hold all
  // the pending state.
  //
  // When a provisional frame is first created, it is only partially attached to
  // the frame tree. This means that though a provisional frame might have a
  // frame owner, the frame owner's content frame does not point back at the
  // provisional frame. Similarly, though a provisional frame may have a parent
  // frame pointer, the parent frame's children list will not contain the
  // provisional frame. Thus, a provisional frame is invisible to the rest of
  // Blink unless the navigation commits and the provisional frame is fully
  // attached to the frame tree by calling swap().
  //
  // Otherwise, if the load should not commit, call detach() to discard the
  // frame.
  BLINK_EXPORT static WebLocalFrame* CreateProvisional(
      WebFrameClient*,
      blink::InterfaceProvider*,
      blink::InterfaceRegistry*,
      WebRemoteFrame*,
      WebSandboxFlags);

  // TODO(dcheng): Add a CreateChild() method.

  // Returns the WebFrame associated with the current V8 context. This
  // function can return 0 if the context is associated with a Document that
  // is not currently being displayed in a Frame.
  BLINK_EXPORT static WebLocalFrame* FrameForCurrentContext();

  // Returns the frame corresponding to the given context. This can return 0
  // if the context is detached from the frame, or if the context doesn't
  // correspond to a frame (e.g., workers).
  BLINK_EXPORT static WebLocalFrame* FrameForContext(v8::Local<v8::Context>);

  // Returns the frame inside a given frame or iframe element. Returns 0 if
  // the given element is not a frame, iframe or if the frame is empty.
  BLINK_EXPORT static WebLocalFrame* FromFrameOwnerElement(const WebElement&);

  // Initialization ---------------------------------------------------------

  virtual void SetAutofillClient(WebAutofillClient*) = 0;
  virtual WebAutofillClient* AutofillClient() = 0;
  virtual void SetDevToolsAgentClient(WebDevToolsAgentClient*) = 0;
  virtual WebDevToolsAgent* DevToolsAgent() = 0;

  // Hierarchy ----------------------------------------------------------

  // Get the highest-level LocalFrame in this frame's in-process subtree.
  virtual WebLocalFrame* LocalRoot() = 0;

  // Navigation Ping --------------------------------------------------------

  virtual void SendPings(const WebURL& destination_url) = 0;

  // Navigation ----------------------------------------------------------

  // Runs beforeunload handlers for this frame and returns the value returned
  // by handlers.
  // Note: this may lead to the destruction of the frame.
  virtual bool DispatchBeforeUnloadEvent(bool is_reload) = 0;

  // Returns a WebURLRequest corresponding to the load of the WebHistoryItem.
  virtual WebURLRequest RequestFromHistoryItem(const WebHistoryItem&,
                                               WebCachePolicy) const = 0;

  // Returns a WebURLRequest corresponding to the reload of the current
  // HistoryItem.
  virtual WebURLRequest RequestForReload(
      WebFrameLoadType,
      const WebURL& override_url = WebURL()) const = 0;

  // Load the given URL. For history navigations, a valid WebHistoryItem
  // should be given, as well as a WebHistoryLoadType.
  virtual void Load(const WebURLRequest&,
                    WebFrameLoadType = WebFrameLoadType::kStandard,
                    const WebHistoryItem& = WebHistoryItem(),
                    WebHistoryLoadType = kWebHistoryDifferentDocumentLoad,
                    bool is_client_redirect = false) = 0;

  // Loads the given data with specific mime type and optional text
  // encoding.  For HTML data, baseURL indicates the security origin of
  // the document and is used to resolve links.  If specified,
  // unreachableURL is reported via WebDataSource::unreachableURL.  If
  // replace is false, then this data will be loaded as a normal
  // navigation.  Otherwise, the current history item will be replaced.
  virtual void LoadData(const WebData&,
                        const WebString& mime_type,
                        const WebString& text_encoding,
                        const WebURL& base_url,
                        const WebURL& unreachable_url = WebURL(),
                        bool replace = false,
                        WebFrameLoadType = WebFrameLoadType::kStandard,
                        const WebHistoryItem& = WebHistoryItem(),
                        WebHistoryLoadType = kWebHistoryDifferentDocumentLoad,
                        bool is_client_redirect = false) = 0;

  enum FallbackContentResult {
    // An error page should be shown instead of fallback.
    NoFallbackContent,
    // Something else committed, no fallback content or error page needed.
    NoLoadInProgress,
    // Fallback content rendered, no error page needed.
    FallbackRendered
  };
  // On load failure, attempts to make frame's parent render fallback content.
  virtual FallbackContentResult MaybeRenderFallbackContent(
      const WebURLError&) const = 0;

  // Called when a navigation is blocked because a Content Security Policy (CSP)
  // is infringed.
  virtual void ReportContentSecurityPolicyViolation(
      const blink::WebContentSecurityPolicyViolation&) = 0;

  // Navigation State -------------------------------------------------------

  // Returns true if the current frame's load event has not completed.
  virtual bool IsLoading() const = 0;

  // Returns true if there is a pending redirect or location change
  // within specified interval (in seconds). This could be caused by:
  // * an HTTP Refresh header
  // * an X-Frame-Options header
  // * the respective http-equiv meta tags
  // * window.location value being mutated
  // * CSP policy block
  // * reload
  // * form submission
  virtual bool IsNavigationScheduledWithin(
      double interval_in_seconds) const = 0;

  // Override the normal rules for whether a load has successfully committed
  // in this frame. Used to propagate state when this frame has navigated
  // cross process.
  virtual void SetCommittedFirstRealLoad() = 0;

  // Mark this frame's document as having received a user gesture, based on
  // one of its descendants having processed a user gesture.
  virtual void SetHasReceivedUserGesture() = 0;

  // Reports a list of unique blink::UseCounter::Feature values representing
  // Blink features used, performed or encountered by the browser during the
  // current page load happening on the frame.
  virtual void BlinkFeatureUsageReport(const std::set<int>& features) = 0;

  // Informs the renderer that mixed content was found externally regarding this
  // frame. Currently only the the browser process can do so. The included data
  // is used for instance to report to the CSP policy and to log to the frame
  // console.
  virtual void MixedContentFound(const WebURL& main_resource_url,
                                 const WebURL& mixed_content_url,
                                 WebURLRequest::RequestContext,
                                 bool was_allowed,
                                 bool had_redirect,
                                 const WebSourceLocation&) = 0;

  // Orientation Changes ----------------------------------------------------

  // Notify the frame that the screen orientation has changed.
  virtual void SendOrientationChangeEvent() = 0;

  // Printing ------------------------------------------------------------

  // Returns true on success and sets the out parameter to the print preset
  // options for the document.
  virtual bool GetPrintPresetOptionsForPlugin(const WebNode&,
                                              WebPrintPresetOptions*) = 0;

  // CSS3 Paged Media ----------------------------------------------------

  // Returns true if page box (margin boxes and page borders) is visible.
  virtual bool IsPageBoxVisible(int page_index) = 0;

  // Returns true if the page style has custom size information.
  virtual bool HasCustomPageSizeStyle(int page_index) = 0;

  // Returns the preferred page size and margins in pixels, assuming 96
  // pixels per inch. pageSize, marginTop, marginRight, marginBottom,
  // marginLeft must be initialized to the default values that are used if
  // auto is specified.
  virtual void PageSizeAndMarginsInPixels(int page_index,
                                          WebDoubleSize& page_size,
                                          int& margin_top,
                                          int& margin_right,
                                          int& margin_bottom,
                                          int& margin_left) = 0;

  // Returns the value for a page property that is only defined when printing.
  // printBegin must have been called before this method.
  virtual WebString PageProperty(const WebString& property_name,
                                 int page_index) = 0;

  // Scripting --------------------------------------------------------------
  // Executes script in the context of the current page and returns the value
  // that the script evaluated to with callback. Script execution can be
  // suspend.
  // DEPRECATED: Prefer requestExecuteScriptInIsolatedWorld().
  virtual void RequestExecuteScriptAndReturnValue(
      const WebScriptSource&,
      bool user_gesture,
      WebScriptExecutionCallback*) = 0;

  // Requests execution of the given function, but allowing for script
  // suspension and asynchronous execution.
  virtual void RequestExecuteV8Function(v8::Local<v8::Context>,
                                        v8::Local<v8::Function>,
                                        v8::Local<v8::Value> receiver,
                                        int argc,
                                        v8::Local<v8::Value> argv[],
                                        WebScriptExecutionCallback*) = 0;

  enum ScriptExecutionType {
    // Execute script synchronously, unless the page is suspended.
    kSynchronous,
    // Execute script asynchronously.
    kAsynchronous,
    // Execute script asynchronously, blocking the window.onload event.
    kAsynchronousBlockingOnload
  };

  // worldID must be > 0 (as 0 represents the main world).
  // worldID must be < EmbedderWorldIdLimit, high number used internally.
  virtual void RequestExecuteScriptInIsolatedWorld(
      int world_id,
      const WebScriptSource* source_in,
      unsigned num_sources,
      bool user_gesture,
      ScriptExecutionType,
      WebScriptExecutionCallback*) = 0;

  // Associates an isolated world with human-readable name which is useful for
  // extension debugging.
  virtual void SetIsolatedWorldHumanReadableName(int world_id,
                                                 const WebString&) = 0;

  // Logs to the console associated with this frame.
  virtual void AddMessageToConsole(const WebConsoleMessage&) = 0;

  // Editing -------------------------------------------------------------

  virtual void SetMarkedText(const WebString& text,
                             unsigned location,
                             unsigned length) = 0;
  virtual void UnmarkText() = 0;
  virtual bool HasMarkedText() const = 0;

  virtual WebRange MarkedRange() const = 0;

  // Returns the text range rectangle in the viepwort coordinate space.
  virtual bool FirstRectForCharacterRange(unsigned location,
                                          unsigned length,
                                          WebRect&) const = 0;

  // Returns the index of a character in the Frame's text stream at the given
  // point. The point is in the viewport coordinate space. Will return
  // WTF::notFound if the point is invalid.
  virtual size_t CharacterIndexForPoint(const WebPoint&) const = 0;

  // Supports commands like Undo, Redo, Cut, Copy, Paste, SelectAll,
  // Unselect, etc. See EditorCommand.cpp for the full list of supported
  // commands.
  virtual bool ExecuteCommand(const WebString&) = 0;
  virtual bool ExecuteCommand(const WebString&, const WebString& value) = 0;
  virtual bool IsCommandEnabled(const WebString&) const = 0;

  // Selection -----------------------------------------------------------

  virtual bool HasSelection() const = 0;

  virtual WebRange SelectionRange() const = 0;

  virtual WebString SelectionAsText() const = 0;
  virtual WebString SelectionAsMarkup() const = 0;

  // Expands the selection to a word around the caret and returns
  // true. Does nothing and returns false if there is no caret or
  // there is ranged selection.
  virtual bool SelectWordAroundCaret() = 0;

  // DEPRECATED: Use moveRangeSelection.
  virtual void SelectRange(const WebPoint& base, const WebPoint& extent) = 0;

  enum HandleVisibilityBehavior {
    // Hide handle(s) in the new selection.
    kHideSelectionHandle,
    // Show handle(s) in the new selection.
    kShowSelectionHandle,
    // Keep the current handle visibility.
    kPreserveHandleVisibility,
  };
  virtual void SelectRange(const WebRange&,
                           HandleVisibilityBehavior = kHideSelectionHandle) = 0;

  virtual WebString RangeAsText(const WebRange&) = 0;

  // Move the current selection to the provided viewport point/points. If the
  // current selection is editable, the new selection will be restricted to
  // the root editable element.
  // |TextGranularity| represents character wrapping granularity. If
  // WordGranularity is set, WebFrame extends selection to wrap word.
  virtual void MoveRangeSelection(
      const WebPoint& base,
      const WebPoint& extent,
      WebFrame::TextGranularity = kCharacterGranularity) = 0;
  virtual void MoveCaretSelection(const WebPoint&) = 0;

  virtual bool SetEditableSelectionOffsets(int start, int end) = 0;
  virtual bool SetCompositionFromExistingText(
      int composition_start,
      int composition_end,
      const WebVector<WebCompositionUnderline>& underlines) = 0;
  virtual void ExtendSelectionAndDelete(int before, int after) = 0;

  virtual void SetCaretVisible(bool) = 0;

  // Moves the selection extent point. This function does not allow the
  // selection to collapse. If the new extent is set to the same position as
  // the current base, this function will do nothing.
  virtual void MoveRangeSelectionExtent(const WebPoint&) = 0;
  // Replaces the selection with the input string.
  virtual void ReplaceSelection(const WebString&) = 0;
  // Deletes text before and after the current cursor position, excluding the
  // selection. The lengths are supplied in UTF-16 Code Unit, not in code points
  // or in glyphs.
  virtual void DeleteSurroundingText(int before, int after) = 0;
  // A variant of deleteSurroundingText(int, int). Major differences are:
  // 1. The lengths are supplied in code points, not in UTF-16 Code Unit or in
  // glyphs.
  // 2. This method does nothing if there are one or more invalid surrogate
  // pairs in the requested range.
  virtual void DeleteSurroundingTextInCodePoints(int before, int after) = 0;

  virtual void ExtractSmartClipData(WebRect rect_in_viewport,
                                    WebString& clip_text,
                                    WebString& clip_html) = 0;

  // Spell-checking support -------------------------------------------------
  virtual void SetTextCheckClient(WebTextCheckClient*) = 0;
  virtual void ReplaceMisspelledRange(const WebString&) = 0;
  virtual void EnableSpellChecking(bool) = 0;
  virtual bool IsSpellCheckingEnabled() const = 0;
  virtual void RemoveSpellingMarkers() = 0;
  virtual void RemoveSpellingMarkersUnderWords(
      const WebVector<WebString>& words) = 0;

  // Content Settings -------------------------------------------------------

  virtual void SetContentSettingsClient(WebContentSettingsClient*) = 0;

  // Image reload -----------------------------------------------------------

  // If the provided node is an image, reload the image disabling Lo-Fi.
  virtual void ReloadImage(const WebNode&) = 0;

  // Reloads all the Lo-Fi images in this WebLocalFrame. Ignores the cache and
  // reloads from the network.
  virtual void ReloadLoFiImages() = 0;

  // Feature usage logging --------------------------------------------------

  virtual void DidCallAddSearchProvider() = 0;
  virtual void DidCallIsSearchProviderInstalled() = 0;

  // Iframe sandbox ---------------------------------------------------------

  // Returns the effective sandbox flags which are inherited from their parent
  // frame.
  virtual WebSandboxFlags EffectiveSandboxFlags() const = 0;

  // Set sandbox flags that will always be forced on this frame.  This is
  // used to inherit sandbox flags from cross-process opener frames in popups.
  //
  // TODO(dcheng): Remove this once we have WebLocalFrame::createMainFrame.
  virtual void ForceSandboxFlags(WebSandboxFlags) = 0;

  // Find-in-page -----------------------------------------------------------

  // Specifies the action to be taken at the end of a find-in-page session.
  enum StopFindAction {
    // No selection will be left.
    kStopFindActionClearSelection,

    // The active match will remain selected.
    kStopFindActionKeepSelection,

    // The active match selection will be activated.
    kStopFindActionActivateSelection
  };

  // Begins a find request, which includes finding the next find match (using
  // find()) and scoping the frame for find matches if needed.
  virtual void RequestFind(int identifier,
                           const WebString& search_text,
                           const WebFindOptions&) = 0;

  // Searches a frame for a given string.
  //
  // If a match is found, this function will select it (scrolling down to
  // make it visible if needed) and fill in selectionRect with the
  // location of where the match was found (in window coordinates).
  //
  // If no match is found, this function clears all tickmarks and
  // highlighting.
  //
  // Returns true if the search string was found, false otherwise.
  virtual bool Find(int identifier,
                    const WebString& search_text,
                    const WebFindOptions&,
                    bool wrap_within_frame,
                    bool* active_now = nullptr) = 0;

  // Notifies the frame that we are no longer interested in searching.  This
  // will abort any asynchronous scoping effort already under way (see the
  // function TextFinder::scopeStringMatches for details) and erase all
  // tick-marks and highlighting from the previous search.  It will also
  // follow the specified StopFindAction.
  virtual void StopFinding(StopFindAction) = 0;

  // This function is called during the scoping effort to keep a running tally
  // of the accumulated total match-count in the frame.  After updating the
  // count it will notify the WebViewClient about the new count.
  virtual void IncreaseMatchCount(int count, int identifier) = 0;

  // Returns a counter that is incremented when the find-in-page markers are
  // changed on the frame. Switching the active marker doesn't change the
  // current version.
  virtual int FindMatchMarkersVersion() const = 0;

  // Returns the bounding box of the active find-in-page match marker or an
  // empty rect if no such marker exists. The rect is returned in find-in-page
  // coordinates.
  virtual WebFloatRect ActiveFindMatchRect() = 0;

  // Swaps the contents of the provided vector with the bounding boxes of the
  // find-in-page match markers from the frame. The bounding boxes are
  // returned in find-in-page coordinates.
  virtual void FindMatchRects(WebVector<WebFloatRect>&) = 0;

  // Selects the find-in-page match closest to the provided point in
  // find-in-page coordinates. Returns the ordinal of such match or -1 if none
  // could be found. If not null, selectionRect is set to the bounding box of
  // the selected match in window coordinates.
  virtual int SelectNearestFindMatch(const WebFloatPoint&,
                                     WebRect* selection_rect) = 0;

  // Returns the distance (squared) to the closest find-in-page match from the
  // provided point, in find-in-page coordinates.
  virtual float DistanceToNearestFindMatch(const WebFloatPoint&) = 0;

  // Set the tickmarks for the frame. This will override the default tickmarks
  // generated by find results. If this is called with an empty array, the
  // default behavior will be restored.
  virtual void SetTickmarks(const WebVector<WebRect>&) = 0;

  // Clears the active find match in the frame, if one exists.
  virtual void ClearActiveFindMatch() = 0;

  // Context menu -----------------------------------------------------------

  // Returns the node that the context menu opened over.
  virtual WebNode ContextMenuNode() const = 0;

  // Returns the WebFrameWidget associated with this frame if there is one or
  // nullptr otherwise.
  virtual WebFrameWidget* FrameWidget() const = 0;

  // Copy to the clipboard the image located at a particular point in visual
  // viewport coordinates.
  virtual void CopyImageAt(const WebPoint&) = 0;

  // Save as the image located at a particular point in visual viewport
  // coordinates.
  virtual void SaveImageAt(const WebPoint&) = 0;

  // Site engagement --------------------------------------------------------

  // Sets the site engagement level for this frame's document.
  virtual void SetEngagementLevel(mojom::EngagementLevel) = 0;

  // TEMP: Usage count for chrome.loadtimes deprecation.
  // This will be removed following the deprecation.
  virtual void UsageCountChromeLoadTimes(const WebString& metric) = 0;

  // Task queues --------------------------------------------------------------

  // Returns frame-specific task runner to run tasks of this type on.
  // They have the same lifetime as the frame.
  virtual base::SingleThreadTaskRunner* TimerTaskRunner() = 0;
  virtual base::SingleThreadTaskRunner* LoadingTaskRunner() = 0;
  virtual base::SingleThreadTaskRunner* UnthrottledTaskRunner() = 0;

  // Returns the WebInputMethodController associated with this local frame.
  virtual WebInputMethodController* GetInputMethodController() = 0;

  // Loading ------------------------------------------------------------------
  // Creates and returns a loader. This function can be called only when this
  // frame is attached to a document.
  virtual std::unique_ptr<WebURLLoader> CreateURLLoader() = 0;

 protected:
  explicit WebLocalFrame(WebTreeScopeType scope) : WebFrame(scope) {}

  // Inherited from WebFrame, but intentionally hidden: it never makes sense
  // to call these on a WebLocalFrame.
  bool IsWebLocalFrame() const override = 0;
  WebLocalFrame* ToWebLocalFrame() override = 0;
  bool IsWebRemoteFrame() const override = 0;
  WebRemoteFrame* ToWebRemoteFrame() override = 0;
};

}  // namespace blink

#endif  // WebLocalFrame_h
