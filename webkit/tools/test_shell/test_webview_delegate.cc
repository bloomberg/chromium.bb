// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the implementation of TestWebViewDelegate, which serves
// as the WebViewDelegate for the TestShellWebHost.  The host is expected to
// have initialized a MessageLoop before these methods are called.

#include "webkit/tools/test_shell/test_webview_delegate.h"

#include "base/debug/trace_event.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "gfx/native_widget_types.h"
#include "gfx/point.h"
#include "media/base/filter_collection.h"
#include "media/base/message_loop_factory_impl.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientationClientMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationClientMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKitClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechInputController.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechInputControllerMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSpeechInputListener.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWindowFeatures.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/media/video_renderer_impl.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webmediaplayer_impl.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/plugins/npapi/webplugin_impl.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/tools/test_shell/mock_spellcheck.h"
#include "webkit/tools/test_shell/notification_presenter.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"
#include "webkit/tools/test_shell/test_web_worker.h"

#if defined(OS_WIN)
// TODO(port): make these files work everywhere.
#include "webkit/tools/test_shell/drag_delegate.h"
#include "webkit/tools/test_shell/drop_delegate.h"
#endif

using appcache::WebApplicationCacheHostImpl;
using WebKit::WebAccessibilityObject;
using WebKit::WebApplicationCacheHost;
using WebKit::WebApplicationCacheHostClient;
using WebKit::WebConsoleMessage;
using WebKit::WebContextMenuData;
using WebKit::WebCookieJar;
using WebKit::WebData;
using WebKit::WebDataSource;
using WebKit::WebDragData;
using WebKit::WebDragOperationsMask;
using WebKit::WebEditingAction;
using WebKit::WebFileSystem;
using WebKit::WebFileSystemCallbacks;
using WebKit::WebFormElement;
using WebKit::WebFrame;
using WebKit::WebHistoryItem;
using WebKit::WebImage;
using WebKit::WebMediaPlayer;
using WebKit::WebMediaPlayerClient;
using WebKit::WebNavigationType;
using WebKit::WebNavigationPolicy;
using WebKit::WebNode;
using WebKit::WebNotificationPresenter;
using WebKit::WebPlugin;
using WebKit::WebPluginParams;
using WebKit::WebPoint;
using WebKit::WebPopupMenu;
using WebKit::WebPopupType;
using WebKit::WebRange;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebSecurityOrigin;
using WebKit::WebSize;
using WebKit::WebStorageNamespace;
using WebKit::WebString;
using WebKit::WebTextAffinity;
using WebKit::WebTextDirection;
using WebKit::WebURL;
using WebKit::WebURLError;
using WebKit::WebURLRequest;
using WebKit::WebURLResponse;
using WebKit::WebWidget;
using WebKit::WebWindowFeatures;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;
using WebKit::WebView;

namespace {

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char* kLinkClickedString = "link clicked";
const char* kFormSubmittedString = "form submitted";
const char* kBackForwardString = "back/forward";
const char* kReloadString = "reload";
const char* kFormResubmittedString = "form resubmitted";
const char* kOtherString = "other";
const char* kIllegalString = "illegal value";

int next_page_id_ = 1;

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
std::string UrlSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  // TODO: elsewhere in the codebase we use net::FormatUrl() for this.
  std::string filename =
      WideToUTF8(FilePath::FromWStringHack(UTF8ToWide(url))
                     .BaseName().ToWStringHack());
  if (filename.empty())
    return "file:";  // A WebKit test has this in its expected output.
  return filename;
}

// Used to write a platform neutral file:/// URL by taking the
// filename and its directory. (e.g., converts
// "file:///tmp/foo/bar.txt" to just "bar.txt").
std::string DescriptionSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  size_t pos = url.rfind('/');
  if (pos == std::string::npos || pos == 0)
    return "ERROR:" + url;
  pos = url.rfind('/', pos - 1);
  if (pos == std::string::npos)
    return "ERROR:" + url;

  return url.substr(pos + 1);
}

// Adds a file called "DRTFakeFile" to |data_object| (CF_HDROP).  Use to fake
// dragging a file.
void AddDRTFakeFileToDataObject(WebDragData* drag_data) {
  drag_data->appendToFilenames(WebString::fromUTF8("DRTFakeFile"));
}

// Get a debugging string from a WebNavigationType.
const char* WebNavigationTypeToString(WebNavigationType type) {
  switch (type) {
    case WebKit::WebNavigationTypeLinkClicked:
      return kLinkClickedString;
    case WebKit::WebNavigationTypeFormSubmitted:
      return kFormSubmittedString;
    case WebKit::WebNavigationTypeBackForward:
      return kBackForwardString;
    case WebKit::WebNavigationTypeReload:
      return kReloadString;
    case WebKit::WebNavigationTypeFormResubmitted:
      return kFormResubmittedString;
    case WebKit::WebNavigationTypeOther:
      return kOtherString;
  }
  return kIllegalString;
}

std::string GetURLDescription(const GURL& url) {
  if (url.SchemeIs("file"))
    return url.ExtractFileName();

  return url.possibly_invalid_spec();
}

std::string GetResponseDescription(const WebURLResponse& response) {
  if (response.isNull())
    return "(null)";

  const std::string url = GURL(response.url()).possibly_invalid_spec();
  return base::StringPrintf("<NSURLResponse %s, http status code %d>",
                            DescriptionSuitableForTestResult(url).c_str(),
                            response.httpStatusCode());
}

std::string GetErrorDescription(const WebURLError& error) {
  std::string domain = UTF16ToASCII(error.domain);
  int code = error.reason;

  if (domain == net::kErrorDomain) {
    domain = "NSURLErrorDomain";
    switch (error.reason) {
      case net::ERR_ABORTED:
        code = -999;  // NSURLErrorCancelled
        break;
      case net::ERR_UNSAFE_PORT:
        // Our unsafe port checking happens at the network stack level, but we
        // make this translation here to match the behavior of stock WebKit.
        domain = "WebKitErrorDomain";
        code = 103;
        break;
      case net::ERR_ADDRESS_INVALID:
      case net::ERR_ADDRESS_UNREACHABLE:
      case net::ERR_NETWORK_ACCESS_DENIED:
        code = -1004;  // NSURLErrorCannotConnectToHost
        break;
    }
  } else {
    DLOG(WARNING) << "Unknown error domain";
  }

  return base::StringPrintf("<NSError domain %s, code %d, failing URL \"%s\">",
      domain.c_str(), code, error.unreachableURL.spec().data());
}

std::string GetNodeDescription(const WebNode& node, int exception) {
  if (exception)
    return "ERROR";
  if (node.isNull())
    return "(null)";
  std::string str = node.nodeName().utf8();
  const WebNode& parent = node.parentNode();
  if (!parent.isNull()) {
    str.append(" > ");
    str.append(GetNodeDescription(parent, 0));
  }
  return str;
}

std::string GetRangeDescription(const WebRange& range) {
  if (range.isNull())
    return "(null)";
  int exception = 0;
  std::string str = "range from ";
  int offset = range.startOffset();
  str.append(base::IntToString(offset));
  str.append(" of ");
  WebNode container = range.startContainer(exception);
  str.append(GetNodeDescription(container, exception));
  str.append(" to ");
  offset = range.endOffset();
  str.append(base::IntToString(offset));
  str.append(" of ");
  container = range.endContainer(exception);
  str.append(GetNodeDescription(container, exception));
  return str;
}

std::string GetEditingActionDescription(WebEditingAction action) {
  switch (action) {
    case WebKit::WebEditingActionTyped:
      return "WebViewInsertActionTyped";
    case WebKit::WebEditingActionPasted:
      return "WebViewInsertActionPasted";
    case WebKit::WebEditingActionDropped:
      return "WebViewInsertActionDropped";
  }
  return "(UNKNOWN ACTION)";
}

std::string GetTextAffinityDescription(WebTextAffinity affinity) {
  switch (affinity) {
    case WebKit::WebTextAffinityUpstream:
      return "NSSelectionAffinityUpstream";
    case WebKit::WebTextAffinityDownstream:
      return "NSSelectionAffinityDownstream";
  }
  return "(UNKNOWN AFFINITY)";
}

}  // namespace

// WebViewDelegate -----------------------------------------------------------

std::string TestWebViewDelegate::GetResourceDescription(uint32 identifier) {
  ResourceMap::iterator it = resource_identifier_map_.find(identifier);
  return it != resource_identifier_map_.end() ? it->second : "<unknown>";
}

void TestWebViewDelegate::SetUserStyleSheetEnabled(bool is_enabled) {
  WebPreferences* prefs = shell_->GetWebPreferences();
  prefs->user_style_sheet_enabled = is_enabled;
  prefs->Apply(shell_->webView());
}

void TestWebViewDelegate::SetUserStyleSheetLocation(const GURL& location) {
  WebPreferences* prefs = shell_->GetWebPreferences();
  prefs->user_style_sheet_enabled = true;
  prefs->user_style_sheet_location = location;
  prefs->Apply(shell_->webView());
}

void TestWebViewDelegate::SetAuthorAndUserStylesEnabled(bool is_enabled) {
  WebPreferences* prefs = shell_->GetWebPreferences();
  prefs->author_and_user_styles_enabled = is_enabled;
  prefs->Apply(shell_->webView());
}

// WebViewClient -------------------------------------------------------------
WebView* TestWebViewDelegate::createView(
    WebFrame* creator,
    const WebURLRequest& request,
    const WebWindowFeatures& window_features,
    const WebString& frame_name) {
  return shell_->CreateWebView();
}

WebWidget* TestWebViewDelegate::createPopupMenu(WebPopupType popup_type) {
  // TODO(darin): Should we take into account |popup_type| (for activation
  //              purpose)?
  return shell_->CreatePopupWidget();
}

WebStorageNamespace* TestWebViewDelegate::createSessionStorageNamespace(
    unsigned quota) {
  // Enforce quota, ignoring the parameter from WebCore as in Chrome.
  return WebKit::WebStorageNamespace::createSessionStorageNamespace(
      WebStorageNamespace::m_sessionStorageQuota);
}

void TestWebViewDelegate::didAddMessageToConsole(
    const WebConsoleMessage& message, const WebString& source_name,
    unsigned source_line) {
  if (!shell_->layout_test_mode()) {
    logging::LogMessage("CONSOLE", 0).stream() << "\""
                                               << message.text.utf8().data()
                                               << ",\" source: "
                                               << source_name.utf8().data()
                                               << "("
                                               << source_line
                                               << ")";
  } else {
    // This matches win DumpRenderTree's UIDelegate.cpp.
    std::string new_message;
    if (!message.text.isEmpty()) {
      new_message = message.text.utf8();
      size_t file_protocol = new_message.find("file://");
      if (file_protocol != std::string::npos) {
        new_message = new_message.substr(0, file_protocol) +
            UrlSuitableForTestResult(new_message.substr(file_protocol));
      }
    }

    printf("CONSOLE MESSAGE: line %d: %s\n", source_line, new_message.data());
  }
}

void TestWebViewDelegate::didStartLoading() {
  shell_->set_is_loading(true);
  shell_->UpdateNavigationControls();
}

void TestWebViewDelegate::didStopLoading() {
  shell_->set_is_loading(false);
  shell_->UpdateNavigationControls();
}

// The output from these methods in layout test mode should match that
// expected by the layout tests.  See EditingDelegate.m in DumpRenderTree.

bool TestWebViewDelegate::shouldBeginEditing(const WebRange& range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldBeginEditingInDOMRange:%s\n",
           GetRangeDescription(range).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldEndEditing(const WebRange& range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldEndEditingInDOMRange:%s\n",
           GetRangeDescription(range).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldInsertNode(const WebNode& node,
                                           const WebRange& range,
                                           WebEditingAction action) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldInsertNode:%s "
           "replacingDOMRange:%s givenAction:%s\n",
           GetNodeDescription(node, 0).c_str(),
           GetRangeDescription(range).c_str(),
           GetEditingActionDescription(action).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldInsertText(const WebString& text,
                                           const WebRange& range,
                                           WebEditingAction action) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldInsertText:%s "
           "replacingDOMRange:%s givenAction:%s\n",
           text.utf8().data(),
           GetRangeDescription(range).c_str(),
           GetEditingActionDescription(action).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldChangeSelectedRange(const WebRange& from_range,
                                                    const WebRange& to_range,
                                                    WebTextAffinity affinity,
                                                    bool still_selecting) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldChangeSelectedDOMRange:%s "
           "toDOMRange:%s affinity:%s stillSelecting:%s\n",
           GetRangeDescription(from_range).c_str(),
           GetRangeDescription(to_range).c_str(),
           GetTextAffinityDescription(affinity).c_str(),
           (still_selecting ? "TRUE" : "FALSE"));
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldDeleteRange(const WebRange& range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldDeleteDOMRange:%s\n",
           GetRangeDescription(range).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldApplyStyle(const WebString& style,
                                           const WebRange& range) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: shouldApplyStyle:%s toElementsInDOMRange:%s\n",
           style.utf8().data(),
           GetRangeDescription(range).c_str());
  }
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::isSmartInsertDeleteEnabled() {
  return smart_insert_delete_enabled_;
}

bool TestWebViewDelegate::isSelectTrailingWhitespaceEnabled() {
  return select_trailing_whitespace_enabled_;
}

void TestWebViewDelegate::didBeginEditing() {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
           "webViewDidBeginEditing:WebViewDidBeginEditingNotification\n");
  }
}

void TestWebViewDelegate::didChangeSelection(bool is_empty_selection) {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
    "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
  }
  UpdateSelectionClipboard(is_empty_selection);
}

void TestWebViewDelegate::didChangeContents() {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
           "webViewDidChange:WebViewDidChangeNotification\n");
  }
}

void TestWebViewDelegate::didEndEditing() {
  if (shell_->ShouldDumpEditingCallbacks()) {
    printf("EDITING DELEGATE: "
           "webViewDidEndEditing:WebViewDidEndEditingNotification\n");
  }
}

bool TestWebViewDelegate::handleCurrentKeyboardEvent() {
  if (edit_command_name_.empty())
    return false;

  WebFrame* frame = shell_->webView()->focusedFrame();
  if (!frame)
    return false;

  return frame->executeCommand(WebString::fromUTF8(edit_command_name_),
                               WebString::fromUTF8(edit_command_value_));
}

void TestWebViewDelegate::spellCheck(const WebString& text,
                                     int& misspelledOffset,
                                     int& misspelledLength) {
#if defined(OS_MACOSX)
  // Check the spelling of the given text.
  // TODO(hbono): rebaseline layout-test results of Windows and Linux so we
  // can enable this mock spellchecker on them.
  string16 word(text);
  mock_spellcheck_.SpellCheckWord(word, &misspelledOffset, &misspelledLength);
#endif
}

WebString TestWebViewDelegate::autoCorrectWord(const WebString& word) {
  // Returns an empty string as Mac WebKit ('WebKitSupport/WebEditorClient.mm')
  // does. (If this function returns a non-empty string, WebKit replaces the
  // given misspelled string with the result one. This process executes some
  // editor commands and causes layout-test failures.)
  return WebString();
}

void TestWebViewDelegate::runModalAlertDialog(
    WebFrame* frame, const WebString& message) {
  if (!shell_->layout_test_mode()) {
    ShowJavaScriptAlert(UTF16ToWideHack(message));
  } else {
    printf("ALERT: %s\n", message.utf8().data());
  }
}

bool TestWebViewDelegate::runModalConfirmDialog(
    WebFrame* frame, const WebString& message) {
  if (shell_->layout_test_mode()) {
    // When running tests, write to stdout.
    printf("CONFIRM: %s\n", message.utf8().data());
    return true;
  }
  return false;
}

bool TestWebViewDelegate::runModalPromptDialog(
    WebFrame* frame, const WebString& message, const WebString& default_value,
    WebString* actual_value) {
  if (shell_->layout_test_mode()) {
    // When running tests, write to stdout.
    printf("PROMPT: %s, default text: %s\n",
           message.utf8().data(),
           default_value.utf8().data());
    return true;
  }
  return false;
}

bool TestWebViewDelegate::runModalBeforeUnloadDialog(
    WebFrame* frame, const WebString& message) {
  return true;  // Allow window closure.
}

void TestWebViewDelegate::showContextMenu(
    WebFrame* frame, const WebContextMenuData& data) {
  CapturedContextMenuEvent context(data.mediaType,
                                   data.mousePosition.x,
                                   data.mousePosition.y);
  captured_context_menu_events_.push_back(context);
  last_context_menu_data_.reset(new WebContextMenuData(data));
}

void TestWebViewDelegate::ClearContextMenuData() {
  last_context_menu_data_.reset();
}




void TestWebViewDelegate::setStatusText(const WebString& text) {
  if (WebKit::layoutTestMode() &&
      shell_->layout_test_controller()->ShouldDumpStatusCallbacks()) {
    // When running tests, write to stdout.
    printf("UI DELEGATE STATUS CALLBACK: setStatusText:%s\n",
           text.utf8().data());
  }
}

void TestWebViewDelegate::startDragging(
    const WebDragData& data,
    WebDragOperationsMask mask,
    const WebImage& image,
    const WebPoint& image_offset) {
  // TODO(tc): Drag and drop is disabled in the test shell because we need
  // to be able to convert from WebDragData to an IDataObject.
  //if (!drag_delegate_)
  //  drag_delegate_ = new TestDragDelegate(shell_->webViewWnd(),
  //                                        shell_->webView());
  //const DWORD ok_effect = DROPEFFECT_COPY | DROPEFFECT_LINK |
  //                        DROPEFFECT_MOVE;
  //DWORD effect;
  //HRESULT res = DoDragDrop(drop_data.data_object, drag_delegate_.get(),
  //                         ok_effect, &effect);
  //DCHECK(DRAGDROP_S_DROP == res || DRAGDROP_S_CANCEL == res);
}

void TestWebViewDelegate::navigateBackForwardSoon(int offset) {
  shell_->navigation_controller()->GoToOffset(offset);
}

int TestWebViewDelegate::historyBackListCount() {
  int current_index =
      shell_->navigation_controller()->GetLastCommittedEntryIndex();
  return current_index;
}

int TestWebViewDelegate::historyForwardListCount() {
  int current_index =
      shell_->navigation_controller()->GetLastCommittedEntryIndex();
  return shell_->navigation_controller()->GetEntryCount() - current_index - 1;
}

WebNotificationPresenter* TestWebViewDelegate::notificationPresenter() {
  return shell_->notification_presenter();
}

WebKit::WebGeolocationClient* TestWebViewDelegate::geolocationClient() {
  return shell_->geolocation_client_mock();
}

WebKit::WebDeviceOrientationClient*
TestWebViewDelegate::deviceOrientationClient() {
  return shell_->device_orientation_client_mock();
}

WebKit::WebSpeechInputController* TestWebViewDelegate::speechInputController(
    WebKit::WebSpeechInputListener* listener) {
  return shell_->CreateSpeechInputControllerMock(listener);
}

// WebWidgetClient -----------------------------------------------------------

void TestWebViewDelegate::didInvalidateRect(const WebRect& rect) {
  if (WebWidgetHost* host = GetWidgetHost())
    host->DidInvalidateRect(rect);
}

void TestWebViewDelegate::didScrollRect(int dx, int dy,
                                        const WebRect& clip_rect) {
  if (WebWidgetHost* host = GetWidgetHost())
    host->DidScrollRect(dx, dy, clip_rect);
}

void TestWebViewDelegate::scheduleComposite() {
  if (WebWidgetHost* host = GetWidgetHost())
    host->ScheduleComposite();
}

void TestWebViewDelegate::scheduleAnimation() {
  if (WebWidgetHost* host = GetWidgetHost())
    host->ScheduleAnimation();
}

void TestWebViewDelegate::didFocus() {
  if (WebWidgetHost* host = GetWidgetHost())
    shell_->SetFocus(host, true);
}

void TestWebViewDelegate::didBlur() {
  if (WebWidgetHost* host = GetWidgetHost())
    shell_->SetFocus(host, false);
}

WebScreenInfo TestWebViewDelegate::screenInfo() {
  if (WebWidgetHost* host = GetWidgetHost())
    return host->GetScreenInfo();

  return WebScreenInfo();
}

// WebFrameClient ------------------------------------------------------------

WebPlugin* TestWebViewDelegate::createPlugin(WebFrame* frame,
                                             const WebPluginParams& params) {
  bool allow_wildcard = true;
  webkit::npapi::WebPluginInfo info;
  std::string actual_mime_type;
  if (!webkit::npapi::PluginList::Singleton()->GetPluginInfo(
          params.url, params.mimeType.utf8(), allow_wildcard, &info,
          &actual_mime_type) || !webkit::npapi::IsPluginEnabled(info))
    return NULL;

  return new webkit::npapi::WebPluginImpl(
      frame, params, info.path, actual_mime_type, AsWeakPtr());
}

WebWorker* TestWebViewDelegate::createWorker(WebFrame* frame,
                                             WebWorkerClient* client) {
  return new TestWebWorker();
}

WebMediaPlayer* TestWebViewDelegate::createMediaPlayer(
    WebFrame* frame, WebMediaPlayerClient* client) {
  scoped_ptr<media::MessageLoopFactory> message_loop_factory(
      new media::MessageLoopFactoryImpl());

  scoped_ptr<media::FilterCollection> collection(
      new media::FilterCollection());

  scoped_refptr<webkit_glue::VideoRendererImpl> video_renderer(
      new webkit_glue::VideoRendererImpl(false));
  collection->AddVideoRenderer(video_renderer);

  scoped_ptr<webkit_glue::WebMediaPlayerImpl> result(
      new webkit_glue::WebMediaPlayerImpl(client,
                                          collection.release(),
                                          message_loop_factory.release()));
  if (!result->Initialize(frame, false, video_renderer)) {
    return NULL;
  }
  return result.release();
}

WebApplicationCacheHost* TestWebViewDelegate::createApplicationCacheHost(
    WebFrame* frame, WebApplicationCacheHostClient* client) {
  return SimpleAppCacheSystem::CreateApplicationCacheHost(client);
}

bool TestWebViewDelegate::allowPlugins(WebFrame* frame,
                                       bool enabled_per_settings) {
  return enabled_per_settings && shell_->allow_plugins();
}

bool TestWebViewDelegate::allowImages(WebFrame* frame,
                                      bool enabled_per_settings) {
  return enabled_per_settings && shell_->allow_images();
}

void TestWebViewDelegate::loadURLExternally(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationPolicy policy) {
  DCHECK_NE(policy, WebKit::WebNavigationPolicyCurrentTab);
  TestShell* shell = NULL;
  if (TestShell::CreateNewWindow(request.url(), &shell))
    shell->Show(policy);
}

WebNavigationPolicy TestWebViewDelegate::decidePolicyForNavigation(
    WebFrame* frame, const WebURLRequest& request,
    WebNavigationType type, const WebNode& originating_node,
    WebNavigationPolicy default_policy, bool is_redirect) {
  WebNavigationPolicy result;
  if (policy_delegate_enabled_) {
    printf("Policy delegate: attempt to load %s with navigation type '%s'",
           GetURLDescription(request.url()).c_str(),
           WebNavigationTypeToString(type));
    if (!originating_node.isNull()) {
      printf(" originating from %s",
          GetNodeDescription(originating_node, 0).c_str());
    }
    printf("\n");
    if (policy_delegate_is_permissive_) {
      result = WebKit::WebNavigationPolicyCurrentTab;
    } else {
      result = WebKit::WebNavigationPolicyIgnore;
    }
    if (policy_delegate_should_notify_done_)
      shell_->layout_test_controller()->PolicyDelegateDone();
  } else {
    result = default_policy;
  }
  return result;
}

bool TestWebViewDelegate::canHandleRequest(
    WebFrame* frame, const WebURLRequest& request) {
  GURL url = request.url();
  // Just reject the scheme used in
  // LayoutTests/http/tests/misc/redirect-to-external-url.html
  return !url.SchemeIs("spaceballs");
}

WebURLError TestWebViewDelegate::cannotHandleRequestError(
    WebFrame* frame, const WebURLRequest& request) {
  WebURLError error;
  // A WebKit layout test expects the following values.
  // unableToImplementPolicyWithError() below prints them.
  error.domain = WebString::fromUTF8("WebKitErrorDomain");
  error.reason = 101;
  error.unreachableURL = request.url();
  return error;
}

WebURLError TestWebViewDelegate::cancelledError(
    WebFrame* frame, const WebURLRequest& request) {
  WebURLError error;
  error.domain = WebString::fromUTF8(net::kErrorDomain);
  error.reason = net::ERR_ABORTED;
  error.unreachableURL = request.url();
  return error;
}

void TestWebViewDelegate::unableToImplementPolicyWithError(
    WebFrame* frame, const WebURLError& error) {
  std::string domain = error.domain.utf8();
  printf("Policy delegate: unable to implement policy with error domain '%s', "
      "error code %d, in frame '%s'\n",
      domain.data(), error.reason, frame->name().utf8().data());
}

void TestWebViewDelegate::willPerformClientRedirect(
    WebFrame* frame, const WebURL& from, const WebURL& to,
    double interval, double fire_time) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - willPerformClientRedirectToURL: %s \n",
           GetFrameDescription(frame).c_str(),
           to.spec().data());
  }
}

void TestWebViewDelegate::didCancelClientRedirect(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didCancelClientRedirectForFrame\n",
           GetFrameDescription(frame).c_str());
  }
}

void TestWebViewDelegate::didCreateDataSource(
    WebFrame* frame, WebDataSource* ds) {
  ds->setExtraData(pending_extra_data_.release());
}

void TestWebViewDelegate::didStartProvisionalLoad(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didStartProvisionalLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }

  if (!top_loading_frame_) {
    top_loading_frame_ = frame;
  }

  if (shell_->layout_test_controller()->StopProvisionalFrameLoads()) {
    printf("%S - stopping load in didStartProvisionalLoadForFrame callback\n",
           GetFrameDescription(frame).c_str());
    frame->stopLoading();
  }
  UpdateAddressBar(frame->view());
}

void TestWebViewDelegate::didReceiveServerRedirectForProvisionalLoad(
    WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didReceiveServerRedirectForProvisionalLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }
  UpdateAddressBar(frame->view());
}

void TestWebViewDelegate::didFailProvisionalLoad(
    WebFrame* frame, const WebURLError& error) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFailProvisionalLoadWithError\n",
           GetFrameDescription(frame).c_str());
  }

  LocationChangeDone(frame);

  // Don't display an error page if we're running layout tests, because
  // DumpRenderTree doesn't.
  if (shell_->layout_test_mode())
    return;

  // Don't display an error page if this is simply a cancelled load.  Aside
  // from being dumb, WebCore doesn't expect it and it will cause a crash.
  if (error.reason == net::ERR_ABORTED)
    return;

  const WebDataSource* failed_ds = frame->provisionalDataSource();

  TestShellExtraData* extra_data =
      static_cast<TestShellExtraData*>(failed_ds->extraData());
  bool replace = extra_data && extra_data->pending_page_id != -1;

  // Ensure the error page ends up with the same page ID if we are replacing.
  if (replace)
    set_pending_extra_data(new TestShellExtraData(extra_data->pending_page_id));

  const std::string& error_text =
      base::StringPrintf("Error %d when loading url %s", error.reason,
      failed_ds->request().url().spec().data());

  // Make sure we never show errors in view source mode.
  frame->enableViewSourceMode(false);

  frame->loadHTMLString(
      error_text, GURL("testshell-error:"), error.unreachableURL, replace);

  // In case the load failed before DidCreateDataSource was called.
  if (replace)
    set_pending_extra_data(NULL);
}

void TestWebViewDelegate::didCommitProvisionalLoad(
    WebFrame* frame, bool is_new_navigation) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didCommitLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }
  UpdateForCommittedLoad(frame, is_new_navigation);
}

void TestWebViewDelegate::didClearWindowObject(WebFrame* frame) {
  shell_->BindJSObjectsToWindow(frame);
}

void TestWebViewDelegate::didReceiveTitle(
    WebFrame* frame, const WebString& title) {
  std::wstring wtitle = UTF16ToWideHack(title);

  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didReceiveTitle: %S\n",
           GetFrameDescription(frame).c_str(), wtitle.c_str());
  }

  if (shell_->ShouldDumpTitleChanges()) {
    printf("TITLE CHANGED: %S\n", wtitle.c_str());
  }

  SetPageTitle(wtitle);
}

void TestWebViewDelegate::didFinishDocumentLoad(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFinishDocumentLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  } else {
    unsigned pending_unload_events = frame->unloadListenerCount();
    if (pending_unload_events) {
      printf("%S - has %u onunload handler(s)\n",
          GetFrameDescription(frame).c_str(), pending_unload_events);
    }
  }
}

void TestWebViewDelegate::didHandleOnloadEvents(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didHandleOnloadEventsForFrame\n",
           GetFrameDescription(frame).c_str());
  }
}

void TestWebViewDelegate::didFailLoad(
    WebFrame* frame, const WebURLError& error) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFailLoadWithError\n",
           GetFrameDescription(frame).c_str());
  }
  LocationChangeDone(frame);
}

void TestWebViewDelegate::didFinishLoad(WebFrame* frame) {
  TRACE_EVENT_END("frame.load", this, frame->url().spec());
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didFinishLoadForFrame\n",
           GetFrameDescription(frame).c_str());
  }
  UpdateAddressBar(frame->view());
  LocationChangeDone(frame);
}

void TestWebViewDelegate::didNavigateWithinPage(
    WebFrame* frame, bool is_new_navigation) {
  frame->dataSource()->setExtraData(pending_extra_data_.release());

  UpdateForCommittedLoad(frame, is_new_navigation);
}

void TestWebViewDelegate::didChangeLocationWithinPage(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks()) {
    printf("%S - didChangeLocationWithinPageForFrame\n",
           GetFrameDescription(frame).c_str());
  }
}

void TestWebViewDelegate::assignIdentifierToRequest(
    WebFrame* frame, unsigned identifier, const WebURLRequest& request) {
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    resource_identifier_map_[identifier] =
        DescriptionSuitableForTestResult(request.url().spec());
  }
}

void TestWebViewDelegate::willSendRequest(
    WebFrame* frame, unsigned identifier, WebURLRequest& request,
    const WebURLResponse& redirect_response) {
  GURL url = request.url();
  std::string request_url = url.possibly_invalid_spec();

  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    GURL main_document_url = request.firstPartyForCookies();
    printf("%s - willSendRequest <NSURLRequest URL %s, main document URL %s,"
           " http method %s> redirectResponse %s\n",
           GetResourceDescription(identifier).c_str(),
           DescriptionSuitableForTestResult(request_url).c_str(),
           GetURLDescription(main_document_url).c_str(),
           request.httpMethod().utf8().data(),
           GetResponseDescription(redirect_response).c_str());
  }

  if (!redirect_response.isNull() && block_redirects_) {
    printf("Returning null for this redirect\n");

    // To block the request, we set its URL to an empty one.
    request.setURL(WebURL());
    return;
  }

  if (request_return_null_) {
    // To block the request, we set its URL to an empty one.
    request.setURL(WebURL());
    return;
  }

  std::string host = url.host();
  if (TestShell::layout_test_mode() && !host.empty() &&
      (url.SchemeIs("http") || url.SchemeIs("https")) &&
       host != "127.0.0.1" &&
       host != "255.255.255.255" &&  // Used in some tests that expect to get
                                     // back an error.
       host != "localhost" &&
      !TestShell::allow_external_pages()) {
    printf("Blocked access to external URL %s\n", request_url.c_str());

    // To block the request, we set its URL to an empty one.
    request.setURL(WebURL());
    return;
  }

  for (std::set<std::string>::const_iterator header = clear_headers_.begin();
       header != clear_headers_.end(); ++header)
    request.clearHTTPHeaderField(WebString::fromUTF8(*header));

  TRACE_EVENT_BEGIN("url.load", identifier, request_url);
  // Set the new substituted URL.
  request.setURL(GURL(TestShell::RewriteLocalUrl(request_url)));
}

void TestWebViewDelegate::didReceiveResponse(
    WebFrame* frame, unsigned identifier, const WebURLResponse& response) {
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    printf("%s - didReceiveResponse %s\n",
           GetResourceDescription(identifier).c_str(),
           GetResponseDescription(response).c_str());
  }
  if (shell_->ShouldDumpResourceResponseMIMETypes()) {
    GURL url = response.url();
    WebString mimeType = response.mimeType();
    printf("%s has MIME type %s\n",
        url.ExtractFileName().c_str(),
        // Simulate NSURLResponse's mapping of empty/unknown MIME types to
        // application/octet-stream.
        mimeType.isEmpty() ?
            "application/octet-stream" : mimeType.utf8().data());
  }
}

void TestWebViewDelegate::didFinishResourceLoad(
    WebFrame* frame, unsigned identifier) {
  TRACE_EVENT_END("url.load", identifier, "");
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    printf("%s - didFinishLoading\n",
           GetResourceDescription(identifier).c_str());
  }
  resource_identifier_map_.erase(identifier);
}

void TestWebViewDelegate::didFailResourceLoad(
    WebFrame* frame, unsigned identifier, const WebURLError& error) {
  if (shell_->ShouldDumpResourceLoadCallbacks()) {
    printf("%s - didFailLoadingWithError: %s\n",
           GetResourceDescription(identifier).c_str(),
           GetErrorDescription(error).c_str());
  }
  resource_identifier_map_.erase(identifier);
}

void TestWebViewDelegate::didDisplayInsecureContent(WebFrame* frame) {
  if (shell_->ShouldDumpFrameLoadCallbacks())
    printf("didDisplayInsecureContent\n");
}

// We have two didRunInsecureContent's with the same name. That's because
// we're in the process of adding an argument and one of them will be correct.
// Once the WebKit change is in, the first should be removed.
void TestWebViewDelegate::didRunInsecureContent(
    WebFrame* frame, const WebSecurityOrigin& origin) {
  if (shell_->ShouldDumpFrameLoadCallbacks())
    printf("didRunInsecureContent\n");
}

void TestWebViewDelegate::didRunInsecureContent(
    WebFrame* frame, const WebSecurityOrigin& origin, const WebURL& target) {
  if (shell_->ShouldDumpFrameLoadCallbacks())
    printf("didRunInsecureContent\n");
}

bool TestWebViewDelegate::allowScript(WebFrame* frame,
                                      bool enabled_per_settings) {
  return enabled_per_settings && shell_->allow_scripts();
}

void TestWebViewDelegate::openFileSystem(
    WebFrame* frame, WebFileSystem::Type type, long long size, bool create,
    WebFileSystemCallbacks* callbacks) {
  SimpleFileSystem* fileSystem = static_cast<SimpleFileSystem*>(
      WebKit::webKitClient()->fileSystem());
  fileSystem->OpenFileSystem(frame, type, size, create, callbacks);
}

// WebPluginPageDelegate -----------------------------------------------------

WebCookieJar* TestWebViewDelegate::GetCookieJar() {
  return WebKit::webKitClient()->cookieJar();
}

// Public methods ------------------------------------------------------------

TestWebViewDelegate::TestWebViewDelegate(TestShell* shell)
    : policy_delegate_enabled_(false),
      policy_delegate_is_permissive_(false),
      policy_delegate_should_notify_done_(false),
      shell_(shell),
      top_loading_frame_(NULL),
      page_id_(-1),
      last_page_id_updated_(-1),
      using_fake_rect_(false),
#if defined(TOOLKIT_USES_GTK)
      cursor_type_(GDK_X_CURSOR),
#endif
      smart_insert_delete_enabled_(true),
#if defined(OS_WIN)
      select_trailing_whitespace_enabled_(true),
#else
      select_trailing_whitespace_enabled_(false),
#endif
      block_redirects_(false),
      request_return_null_(false) {
}

TestWebViewDelegate::~TestWebViewDelegate() {
}

void TestWebViewDelegate::Reset() {
  // Do a little placement new dance...
  TestShell* shell = shell_;
  this->~TestWebViewDelegate();
  new (this) TestWebViewDelegate(shell);
  if (shell->speech_input_controller_mock())
    shell->speech_input_controller_mock()->clearResults();
}

void TestWebViewDelegate::SetSmartInsertDeleteEnabled(bool enabled) {
  smart_insert_delete_enabled_ = enabled;
  // In upstream WebKit, smart insert/delete is mutually exclusive with select
  // trailing whitespace, however, we allow both because Chromium on Windows
  // allows both.
}

void TestWebViewDelegate::SetSelectTrailingWhitespaceEnabled(bool enabled) {
  select_trailing_whitespace_enabled_ = enabled;
  // In upstream WebKit, smart insert/delete is mutually exclusive with select
  // trailing whitespace, however, we allow both because Chromium on Windows
  // allows both.
}

void TestWebViewDelegate::RegisterDragDrop() {
#if defined(OS_WIN)
  // TODO(port): add me once drag and drop works.
  DCHECK(!drop_delegate_);
  drop_delegate_ = new TestDropDelegate(shell_->webViewWnd(),
                                        shell_->webView());
#endif
}

void TestWebViewDelegate::RevokeDragDrop() {
#if defined(OS_WIN)
  ::RevokeDragDrop(shell_->webViewWnd());
#endif
}

void TestWebViewDelegate::SetCustomPolicyDelegate(bool is_custom,
                                                  bool is_permissive) {
  policy_delegate_enabled_ = is_custom;
  policy_delegate_is_permissive_ = is_permissive;
}

void TestWebViewDelegate::WaitForPolicyDelegate() {
  policy_delegate_enabled_ = true;
  policy_delegate_should_notify_done_ = true;
}

// Private methods -----------------------------------------------------------

void TestWebViewDelegate::UpdateAddressBar(WebView* webView) {
  WebFrame* main_frame = webView->mainFrame();

  WebDataSource* data_source = main_frame->dataSource();
  if (!data_source)
    data_source = main_frame->provisionalDataSource();
  if (!data_source)
    return;

  SetAddressBarURL(data_source->request().url());
}

void TestWebViewDelegate::LocationChangeDone(WebFrame* frame) {
  if (frame == top_loading_frame_) {
    top_loading_frame_ = NULL;

    if (shell_->layout_test_mode())
      shell_->layout_test_controller()->LocationChangeDone();
  }
}

WebWidgetHost* TestWebViewDelegate::GetWidgetHost() {
  if (this == shell_->delegate())
    return shell_->webViewHost();
  if (this == shell_->popup_delegate())
    return shell_->popupHost();
  return NULL;
}

void TestWebViewDelegate::UpdateForCommittedLoad(WebFrame* frame,
                                                 bool is_new_navigation) {
  // Code duplicated from RenderView::DidCommitLoadForFrame.
  TestShellExtraData* extra_data = static_cast<TestShellExtraData*>(
      frame->dataSource()->extraData());

  if (is_new_navigation) {
    // New navigation.
    UpdateSessionHistory(frame);
    page_id_ = next_page_id_++;
  } else if (extra_data && extra_data->pending_page_id != -1 &&
             !extra_data->request_committed) {
    // This is a successful session history navigation!
    UpdateSessionHistory(frame);
    page_id_ = extra_data->pending_page_id;
  }

  // Don't update session history multiple times.
  if (extra_data)
    extra_data->request_committed = true;

  UpdateURL(frame);
}

void TestWebViewDelegate::UpdateURL(WebFrame* frame) {
  WebDataSource* ds = frame->dataSource();
  DCHECK(ds);

  const WebURLRequest& request = ds->request();

  // Type is unused.
  scoped_ptr<TestNavigationEntry> entry(new TestNavigationEntry);

  // Bug 654101: the referrer will be empty on https->http transitions. It
  // would be nice if we could get the real referrer from somewhere.
  entry->SetPageID(page_id_);
  if (ds->hasUnreachableURL()) {
    entry->SetURL(ds->unreachableURL());
  } else {
    entry->SetURL(request.url());
  }

  const WebHistoryItem& history_item = frame->currentHistoryItem();
  if (!history_item.isNull())
    entry->SetContentState(webkit_glue::HistoryItemToString(history_item));

  shell_->navigation_controller()->DidNavigateToEntry(entry.release());
  shell_->UpdateNavigationControls();
  UpdateAddressBar(frame->view());

  last_page_id_updated_ = std::max(last_page_id_updated_, page_id_);
}

void TestWebViewDelegate::UpdateSessionHistory(WebFrame* frame) {
  // If we have a valid page ID at this point, then it corresponds to the page
  // we are navigating away from.  Otherwise, this is the first navigation, so
  // there is no past session history to record.
  if (page_id_ == -1)
    return;

  TestNavigationEntry* entry = static_cast<TestNavigationEntry*>(
      shell_->navigation_controller()->GetEntryWithPageID(page_id_));
  if (!entry)
    return;

  const WebHistoryItem& history_item =
      shell_->webView()->mainFrame()->previousHistoryItem();
  if (history_item.isNull())
    return;

  entry->SetContentState(webkit_glue::HistoryItemToString(history_item));
}

std::wstring TestWebViewDelegate::GetFrameDescription(WebFrame* webframe) {
  std::wstring name = UTF16ToWideHack(webframe->name());

  if (webframe == shell_->webView()->mainFrame()) {
    if (name.length())
      return L"main frame \"" + name + L"\"";
    else
      return L"main frame";
  } else {
    if (name.length())
      return L"frame \"" + name + L"\"";
    else
      return L"frame (anonymous)";
  }
}

void TestWebViewDelegate::set_fake_window_rect(const WebRect& rect) {
  fake_rect_ = rect;
  using_fake_rect_ = true;
}

WebRect TestWebViewDelegate::fake_window_rect() {
  return fake_rect_;
}
