// Copyright (c) 2012 The Chromium Authors. All rights reserved.
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
#include "media/base/filter_collection.h"
#include "media/base/media_log.h"
#include "media/base/message_loop_factory.h"
#include "net/base/net_errors.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAccessibilityObject.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebConsoleMessage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebContextMenuData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDeviceOrientationClientMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDataSource.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebHistoryItem.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebImage.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFileSystemCallbacks.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebGeolocationClientMock.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebKit.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebKitPlatformSupport.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNotificationPresenter.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPluginParams.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebPoint.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupMenu.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRange.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebScreenInfo.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebStorageNamespace.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURL.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLError.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLRequest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebURLResponse.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebVector.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWindowFeatures.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/point.h"
#include "webkit/appcache/web_application_cache_host_impl.h"
#include "webkit/glue/glue_serialize.h"
#include "webkit/glue/webdropdata.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/weburlrequest_extradata_impl.h"
#include "webkit/glue/window_open_disposition.h"
#include "webkit/gpu/webgraphicscontext3d_in_process_impl.h"
#include "webkit/media/webmediaplayer_impl.h"
#include "webkit/plugins/npapi/webplugin_impl.h"
#include "webkit/plugins/npapi/plugin_list.h"
#include "webkit/plugins/npapi/webplugin_delegate_impl.h"
#include "webkit/tools/test_shell/mock_spellcheck.h"
#include "webkit/tools/test_shell/notification_presenter.h"
#include "webkit/tools/test_shell/simple_appcache_system.h"
#include "webkit/tools/test_shell/simple_dom_storage_system.h"
#include "webkit/tools/test_shell/simple_file_system.h"
#include "webkit/tools/test_shell/test_navigation_controller.h"
#include "webkit/tools/test_shell/test_shell.h"

#if defined(OS_WIN)
// TODO(port): make these files work everywhere.
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
using WebKit::WebGraphicsContext3D;
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
using WebKit::WebVector;
using WebKit::WebView;
using webkit_glue::WebPreferences;

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

// Adds a file called "DRTFakeFile" to |data_object|.  Use to fake dragging a
// file.
void AddDRTFakeFileToDataObject(WebDragData* drag_data) {
  WebDragData::Item item;
  item.storageType = WebDragData::Item::StorageTypeFilename;
  item.filenameData = WebString::fromUTF8("DRTFakeFile");
  drag_data->addItem(item);
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
    const WebString& frame_name,
    WebNavigationPolicy policy) {
  return shell_->CreateWebView();
}

WebWidget* TestWebViewDelegate::createPopupMenu(WebPopupType popup_type) {
  // TODO(darin): Should we take into account |popup_type| (for activation
  //              purpose)?
  return shell_->CreatePopupWidget();
}

WebStorageNamespace* TestWebViewDelegate::createSessionStorageNamespace(
    unsigned quota) {
  return SimpleDomStorageSystem::instance().CreateSessionStorageNamespace();
}

void TestWebViewDelegate::didAddMessageToConsole(
    const WebConsoleMessage& message, const WebString& source_name,
    unsigned source_line) {
  logging::LogMessage("CONSOLE", 0).stream() << "\""
                                             << message.text.utf8().data()
                                             << ",\" source: "
                                             << source_name.utf8().data()
                                             << "("
                                             << source_line
                                             << ")";
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
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldEndEditing(const WebRange& range) {
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldInsertNode(const WebNode& node,
                                           const WebRange& range,
                                           WebEditingAction action) {
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldInsertText(const WebString& text,
                                           const WebRange& range,
                                           WebEditingAction action) {
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldChangeSelectedRange(const WebRange& from_range,
                                                    const WebRange& to_range,
                                                    WebTextAffinity affinity,
                                                    bool still_selecting) {
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldDeleteRange(const WebRange& range) {
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::shouldApplyStyle(const WebString& style,
                                           const WebRange& range) {
  return shell_->AcceptsEditing();
}

bool TestWebViewDelegate::isSmartInsertDeleteEnabled() {
  return smart_insert_delete_enabled_;
}

bool TestWebViewDelegate::isSelectTrailingWhitespaceEnabled() {
  return select_trailing_whitespace_enabled_;
}

void TestWebViewDelegate::didBeginEditing() {
}

void TestWebViewDelegate::didChangeSelection(bool is_empty_selection) {
  UpdateSelectionClipboard(is_empty_selection);
}

void TestWebViewDelegate::didChangeContents() {
}

void TestWebViewDelegate::didEndEditing() {
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
    ShowJavaScriptAlert(message);
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




void TestWebViewDelegate::setStatusText(const WebString& text) {
}

void TestWebViewDelegate::startDragging(
    WebFrame* frame,
    const WebDragData& data,
    WebDragOperationsMask mask,
    const WebImage& image,
    const WebPoint& image_offset) {
  shell_->webView()->dragSourceSystemDragEnded();
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
  return 0;
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
  std::vector<webkit::WebPluginInfo> plugins;
  std::vector<std::string> mime_types;
  webkit::npapi::PluginList::Singleton()->GetPluginInfoArray(
      params.url, params.mimeType.utf8(), allow_wildcard,
      NULL, &plugins, &mime_types);
  if (plugins.empty())
    return NULL;

  WebPluginParams params_copy = params;
  params_copy.mimeType = WebString::fromUTF8(mime_types.front());

#if defined(OS_MACOSX)
  if (!shell_->layout_test_mode()) {
    bool flash = LowerCaseEqualsASCII(params_copy.mimeType.utf8(),
                                      "application/x-shockwave-flash");
    if (flash) {
      // Mac does not support windowed plugins. Force Flash plugins to use
      // windowless mode by setting the wmode="opaque" attribute.
      DCHECK(params_copy.attributeNames.size() ==
             params_copy.attributeValues.size());
      size_t size = params_copy.attributeNames.size();

      WebVector<WebString> new_names(size+1),  new_values(size+1);

      for (size_t i = 0; i < size; ++i) {
        new_names[i] = params_copy.attributeNames[i];
        new_values[i] = params_copy.attributeValues[i];
      }

      new_names[size] = "wmode";
      new_values[size] = "opaque";

      params_copy.attributeNames.swap(new_names);
      params_copy.attributeValues.swap(new_values);

      return new webkit::npapi::WebPluginImpl(
          frame, params_copy, plugins.front().path, AsWeakPtr());
    }
  }
#endif  // defined (OS_MACOSX)

  return new webkit::npapi::WebPluginImpl(
      frame, params, plugins.front().path, AsWeakPtr());
}

WebMediaPlayer* TestWebViewDelegate::createMediaPlayer(
    WebFrame* frame, const WebKit::WebURL& url, WebMediaPlayerClient* client) {
  scoped_ptr<media::MessageLoopFactory> message_loop_factory(
      new media::MessageLoopFactory());

  scoped_ptr<media::FilterCollection> collection(
      new media::FilterCollection());

  return new webkit_media::WebMediaPlayerImpl(
      frame,
      client,
      base::WeakPtr<webkit_media::WebMediaPlayerDelegate>(),
      collection.release(),
      NULL,
      NULL,
      message_loop_factory.release(),
      NULL,
      new media::MediaLog());
}

WebApplicationCacheHost* TestWebViewDelegate::createApplicationCacheHost(
    WebFrame* frame, WebApplicationCacheHostClient* client) {
  return SimpleAppCacheSystem::CreateApplicationCacheHost(client);
}

bool TestWebViewDelegate::allowPlugins(WebFrame* frame,
                                       bool enabled_per_settings) {
  return enabled_per_settings && shell_->allow_plugins();
}

bool TestWebViewDelegate::allowImage(WebFrame* frame,
                                     bool enabled_per_settings,
                                     const WebURL& image_url) {
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
      domain.data(), error.reason, frame->uniqueName().utf8().data());
}

void TestWebViewDelegate::willPerformClientRedirect(
    WebFrame* frame, const WebURL& from, const WebURL& to,
    double interval, double fire_time) {
}

void TestWebViewDelegate::didCancelClientRedirect(WebFrame* frame) {
}

void TestWebViewDelegate::didCreateDataSource(
    WebFrame* frame, WebDataSource* ds) {
  ds->setExtraData(pending_extra_data_.release());
}

void TestWebViewDelegate::didStartProvisionalLoad(WebFrame* frame) {
  if (!top_loading_frame_) {
    top_loading_frame_ = frame;
  }

  UpdateAddressBar(frame->view());
}

void TestWebViewDelegate::didReceiveServerRedirectForProvisionalLoad(
    WebFrame* frame) {
  UpdateAddressBar(frame->view());
}

void TestWebViewDelegate::didFailProvisionalLoad(
    WebFrame* frame, const WebURLError& error) {
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
  UpdateForCommittedLoad(frame, is_new_navigation);
}

void TestWebViewDelegate::didReceiveTitle(
    WebFrame* frame, const WebString& title, WebTextDirection direction) {
  SetPageTitle(title);
}

void TestWebViewDelegate::didFinishDocumentLoad(WebFrame* frame) {
  unsigned pending_unload_events = frame->unloadListenerCount();
  if (pending_unload_events) {
    printf("%s - has %u onunload handler(s)\n",
        UTF16ToUTF8(GetFrameDescription(frame)).c_str(), pending_unload_events);
  }
}

void TestWebViewDelegate::didHandleOnloadEvents(WebFrame* frame) {
}

void TestWebViewDelegate::didFailLoad(
    WebFrame* frame, const WebURLError& error) {
  LocationChangeDone(frame);
}

void TestWebViewDelegate::didFinishLoad(WebFrame* frame) {
  TRACE_EVENT_END_ETW("frame.load", this, frame->document().url().spec());
  UpdateAddressBar(frame->view());
  LocationChangeDone(frame);
}

void TestWebViewDelegate::didNavigateWithinPage(
    WebFrame* frame, bool is_new_navigation) {
  frame->dataSource()->setExtraData(pending_extra_data_.release());

  UpdateForCommittedLoad(frame, is_new_navigation);
}

void TestWebViewDelegate::didChangeLocationWithinPage(WebFrame* frame) {
}

void TestWebViewDelegate::assignIdentifierToRequest(
    WebFrame* frame, unsigned identifier, const WebURLRequest& request) {
}

void TestWebViewDelegate::willSendRequest(
    WebFrame* frame, unsigned identifier, WebURLRequest& request,
    const WebURLResponse& redirect_response) {
  GURL url = request.url();
  std::string request_url = url.possibly_invalid_spec();

  request.setExtraData(
      new webkit_glue::WebURLRequestExtraDataImpl(
          frame->document().referrerPolicy(), WebString()));

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

  TRACE_EVENT_BEGIN_ETW("url.load", identifier, request_url);
  // Set the new substituted URL.
  request.setURL(GURL(TestShell::RewriteLocalUrl(request_url)));
}

void TestWebViewDelegate::didReceiveResponse(
    WebFrame* frame, unsigned identifier, const WebURLResponse& response) {
}

void TestWebViewDelegate::didFinishResourceLoad(
    WebFrame* frame, unsigned identifier) {
  TRACE_EVENT_END_ETW("url.load", identifier, "");
  resource_identifier_map_.erase(identifier);
}

void TestWebViewDelegate::didFailResourceLoad(
    WebFrame* frame, unsigned identifier, const WebURLError& error) {
  resource_identifier_map_.erase(identifier);
}

void TestWebViewDelegate::didDisplayInsecureContent(WebFrame* frame) {
}

void TestWebViewDelegate::didRunInsecureContent(
    WebFrame* frame, const WebSecurityOrigin& origin, const WebURL& target) {
}

bool TestWebViewDelegate::allowScript(WebFrame* frame,
                                      bool enabled_per_settings) {
  return enabled_per_settings && shell_->allow_scripts();
}

void TestWebViewDelegate::openFileSystem(
    WebFrame* frame, WebFileSystem::Type type, long long size, bool create,
    WebFileSystemCallbacks* callbacks) {
  SimpleFileSystem* fileSystem = static_cast<SimpleFileSystem*>(
      WebKit::webKitPlatformSupport()->fileSystem());
  fileSystem->OpenFileSystem(frame, type, size, create, callbacks);
}

// WebPluginPageDelegate -----------------------------------------------------

WebKit::WebPlugin* TestWebViewDelegate::CreatePluginReplacement(
    const FilePath& file_path) {
  return NULL;
}

WebCookieJar* TestWebViewDelegate::GetCookieJar() {
  return WebKit::webKitPlatformSupport()->cookieJar();
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
#if defined(TOOLKIT_GTK)
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
    shell_->TestFinished();
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

string16 TestWebViewDelegate::GetFrameDescription(WebFrame* webframe) {
  std::string name = UTF16ToUTF8(webframe->uniqueName());

  if (webframe == shell_->webView()->mainFrame()) {
    if (name.length())
      name = "main frame \"" + name + "\"";
    else
      name = "main frame";
  } else {
    if (name.length())
      name = "frame \"" + name + "\"";
    else
      name = "frame (anonymous)";
  }
  return UTF8ToUTF16(name);
}

void TestWebViewDelegate::set_fake_window_rect(const WebRect& rect) {
  fake_rect_ = rect;
  using_fake_rect_ = true;
}

WebRect TestWebViewDelegate::fake_window_rect() {
  return fake_rect_;
}
