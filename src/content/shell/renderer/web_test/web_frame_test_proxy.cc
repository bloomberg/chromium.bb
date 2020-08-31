// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/renderer/web_test/web_frame_test_proxy.h"

#include "components/plugins/renderer/plugin_placeholder.h"
#include "content/common/unique_name_helper.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/shell/common/web_test/web_test_string_util.h"
#include "content/shell/renderer/web_test/blink_test_helpers.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/event_sender.h"
#include "content/shell/renderer/web_test/gc_controller.h"
#include "content/shell/renderer/web_test/spell_check_client.h"
#include "content/shell/renderer/web_test/test_interfaces.h"
#include "content/shell/renderer/web_test/test_plugin.h"
#include "content/shell/renderer/web_test/test_runner.h"
#include "content/shell/renderer/web_test/web_view_test_proxy.h"
#include "content/shell/renderer/web_test/web_widget_test_proxy.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/blink/public/web/web_testing_support.h"
#include "third_party/blink/public/web/web_view.h"

namespace content {

namespace {

// Used to write a platform neutral file:/// URL by taking the
// filename and its directory. (e.g., converts
// "file:///tmp/foo/bar.txt" to just "bar.txt").
std::string DescriptionSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  size_t pos = url.rfind('/');
  if (pos == std::string::npos || !pos)
    return "ERROR:" + url;
  pos = url.rfind('/', pos - 1);
  if (pos == std::string::npos)
    return "ERROR:" + url;

  return url.substr(pos + 1);
}

// Used to write a platform neutral file:/// URL by only taking the filename
// (e.g., converts "file:///tmp/foo.txt" to just "foo.txt").
// TODO(danakj): Can we just use DescriptionSuitableForTestResult() and delete
// this version?
std::string URLSuitableForTestResult(const std::string& url) {
  if (url.empty() || std::string::npos == url.find("file://"))
    return url;

  size_t pos = url.rfind('/');
  if (pos == std::string::npos) {
#ifdef WIN32
    pos = url.rfind('\\');
    if (pos == std::string::npos)
      pos = 0;
#else
    pos = 0;
#endif
  }
  std::string filename = url.substr(pos + 1);
  if (filename.empty())
    return "file:";  // A WebKit test has this in its expected output.
  return filename;
}

void BlockRequest(blink::WebURLRequest& request) {
  request.SetUrl(GURL("255.255.255.255"));
}

bool IsLocalHost(const std::string& host) {
  return host == "127.0.0.1" || host == "localhost" || host == "[::1]";
}

bool IsTestHost(const std::string& host) {
  return base::EndsWith(host, ".test", base::CompareCase::INSENSITIVE_ASCII) ||
         base::EndsWith(host, ".test.", base::CompareCase::INSENSITIVE_ASCII);
}

bool HostIsUsedBySomeTestsToGenerateError(const std::string& host) {
  return host == "255.255.255.255";
}

// WebNavigationType debugging strings taken from PolicyDelegate.mm.
const char kLinkClickedString[] = "link clicked";
const char kFormSubmittedString[] = "form submitted";
const char kBackForwardString[] = "back/forward";
const char kReloadString[] = "reload";
const char kFormResubmittedString[] = "form resubmitted";
const char kOtherString[] = "other";

// Get a debugging string from a WebNavigationType.
const char* WebNavigationTypeToString(blink::WebNavigationType type) {
  switch (type) {
    case blink::kWebNavigationTypeLinkClicked:
      return kLinkClickedString;
    case blink::kWebNavigationTypeFormSubmitted:
      return kFormSubmittedString;
    case blink::kWebNavigationTypeBackForward:
      return kBackForwardString;
    case blink::kWebNavigationTypeReload:
      return kReloadString;
    case blink::kWebNavigationTypeFormResubmitted:
      return kFormResubmittedString;
    case blink::kWebNavigationTypeOther:
      return kOtherString;
  }
  return web_test_string_util::kIllegalString;
}

void PrintFrameUserGestureStatus(BlinkTestRunner* blink_test_runner,
                                 blink::WebLocalFrame* frame,
                                 const char* msg) {
  bool is_user_gesture = frame->HasTransientUserActivation();
  blink_test_runner->PrintMessage(std::string("Frame with user gesture \"") +
                                  (is_user_gesture ? "true" : "false") + "\"" +
                                  msg);
}

class TestRenderFrameObserver : public RenderFrameObserver {
 public:
  TestRenderFrameObserver(RenderFrame* frame, WebViewTestProxy* proxy)
      : RenderFrameObserver(frame), web_view_test_proxy_(proxy) {}

  ~TestRenderFrameObserver() override {}

 private:
  WebFrameTestProxy* frame_proxy() {
    return static_cast<WebFrameTestProxy*>(render_frame());
  }

  TestRunner* test_runner() {
    return web_view_test_proxy_->test_interfaces()->GetTestRunner();
  }

  BlinkTestRunner* blink_test_runner() {
    return web_view_test_proxy_->blink_test_runner();
  }

  // RenderFrameObserver overrides.
  void OnDestruct() override { delete this; }

  void DidStartNavigation(
      const GURL& url,
      base::Optional<blink::WebNavigationType> navigation_type) override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      blink_test_runner()->PrintMessage(description +
                                        " - DidStartNavigation\n");
    }

    if (test_runner()->ShouldDumpUserGestureInFrameLoadCallbacks()) {
      PrintFrameUserGestureStatus(blink_test_runner(),
                                  render_frame()->GetWebFrame(),
                                  " - in DidStartNavigation\n");
    }
  }

  void ReadyToCommitNavigation(
      blink::WebDocumentLoader* document_loader) override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      blink_test_runner()->PrintMessage(description +
                                        " - ReadyToCommitNavigation\n");
    }
  }

  void DidCommitProvisionalLoad(bool is_same_document_navigation,
                                ui::PageTransition transition) override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      blink_test_runner()->PrintMessage(description +
                                        " - didCommitLoadForFrame\n");
    }

    // Looking for navigations to about:blank after a test completes.
    if (render_frame()->IsMainFrame() && !is_same_document_navigation) {
      blink_test_runner()->DidCommitNavigationInMainFrame();
    }
  }

  void DidFailProvisionalLoad() override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      blink_test_runner()->PrintMessage(description +
                                        " - didFailProvisionalLoadWithError\n");
    }
  }

  void DidFinishDocumentLoad() override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      blink_test_runner()->PrintMessage(description +
                                        " - didFinishDocumentLoadForFrame\n");
    }
  }

  void DidFinishLoad() override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      blink_test_runner()->PrintMessage(description +
                                        " - didFinishLoadForFrame\n");
    }
  }

  void DidHandleOnloadEvents() override {
    if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
      std::string description = frame_proxy()->GetFrameDescriptionForWebTests();
      blink_test_runner()->PrintMessage(description +
                                        " - didHandleOnloadEventsForFrame\n");
    }
  }

  WebViewTestProxy* web_view_test_proxy_;
  DISALLOW_COPY_AND_ASSIGN(TestRenderFrameObserver);
};

}  // namespace

WebFrameTestProxy::WebFrameTestProxy(RenderFrameImpl::CreateParams params)
    : RenderFrameImpl(std::move(params)),
      web_view_test_proxy_(static_cast<WebViewTestProxy*>(render_view())) {}

WebFrameTestProxy::~WebFrameTestProxy() = default;

void WebFrameTestProxy::Initialize() {
  RenderFrameImpl::Initialize();

  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  TestRunner* test_runner = interfaces->GetTestRunner();

  GetWebFrame()->SetContentSettingsClient(test_runner->GetWebContentSettings());

  spell_check_ = std::make_unique<SpellCheckClient>(GetWebFrame());
  GetWebFrame()->SetTextCheckClient(spell_check_.get());

  GetAssociatedInterfaceRegistry()->AddInterface(
      base::BindRepeating(&WebFrameTestProxy::BindReceiver,
                          // The registry goes away and stops using this
                          // callback when RenderFrameImpl (which is this class)
                          // is destroyed.
                          base::Unretained(this)));

  new TestRenderFrameObserver(this, web_view_test_proxy_);  // deletes itself.
}

void WebFrameTestProxy::Reset() {
  // TODO(crbug.com/936696): The RenderDocument project will cause us to replace
  // the main frame on each navigation, including to about:blank and then to the
  // next test. So resetting the frame or RenderWidget won't be meaningful then.

  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();
  TestRunner* test_runner = interfaces->GetTestRunner();
  test_runner->ResetWebFrame(this);

  if (IsMainFrame()) {
    GetWebFrame()->SetName(blink::WebString());
    GetWebFrame()->ClearOpener();

    blink::WebTestingSupport::ResetInternalsObject(GetWebFrame());
    // Resetting the internals object also overrides the WebPreferences, so we
    // have to sync them to WebKit again.
    render_view()->SetWebkitPreferences(render_view()->GetWebkitPreferences());
  }
  if (IsLocalRoot()) {
    GetLocalRootWebWidgetTestProxy()->Reset();
    GetLocalRootWebWidgetTestProxy()->EndSyntheticGestures();
  }

  spell_check_->Reset();
}

std::string WebFrameTestProxy::GetFrameNameForWebTests() {
  return UniqueNameHelper::ExtractStableNameForTesting(unique_name());
}

std::string WebFrameTestProxy::GetFrameDescriptionForWebTests() {
  std::string name = GetFrameNameForWebTests();
  if (IsMainFrame()) {
    DCHECK(name.empty());
    return "main frame";
  }
  if (name.empty()) {
    return "frame (anonymous)";
  }
  return std::string("frame \"") + name + "\"";
}

void WebFrameTestProxy::UpdateAllLifecyclePhasesAndCompositeForTesting() {
  if (!IsLocalRoot())
    return;
  auto* widget = static_cast<WebWidgetTestProxy*>(GetLocalRootRenderWidget());
  widget->SynchronouslyComposite(/*do_raster=*/true);
}

blink::WebPlugin* WebFrameTestProxy::CreatePlugin(
    const blink::WebPluginParams& params) {
  if (TestPlugin::IsSupportedMimeType(params.mime_type))
    return TestPlugin::Create(params, blink_test_runner(), GetWebFrame());

  if (params.mime_type == "application/x-plugin-placeholder-test") {
    auto* placeholder =
        new plugins::PluginPlaceholder(this, params, "<div>Test content</div>");
    return placeholder->plugin();
  }

  return RenderFrameImpl::CreatePlugin(params);
}

void WebFrameTestProxy::DidAddMessageToConsole(
    const blink::WebConsoleMessage& message,
    const blink::WebString& source_name,
    unsigned source_line,
    const blink::WebString& stack_trace) {
  RenderFrameImpl::DidAddMessageToConsole(message, source_name, source_line,
                                          stack_trace);

  if (!test_runner()->ShouldDumpConsoleMessages())
    return;

  std::string level;
  switch (message.level) {
    case blink::mojom::ConsoleMessageLevel::kVerbose:
      level = "DEBUG";
      break;
    case blink::mojom::ConsoleMessageLevel::kInfo:
      level = "MESSAGE";
      break;
    case blink::mojom::ConsoleMessageLevel::kWarning:
      level = "WARNING";
      break;
    case blink::mojom::ConsoleMessageLevel::kError:
      level = "ERROR";
      break;
    default:
      level = "MESSAGE";
  }
  std::string console_message(std::string("CONSOLE ") + level + ": ");
  // Do not print line numbers if there is no associated source file name.
  // TODO(crbug.com/896194): Figure out why the source line is flaky for empty
  // source names.
  if (!source_name.IsEmpty() && source_line) {
    console_message += base::StringPrintf("line %d: ", source_line);
  }
  // Console messages shouldn't be included in the expected output for
  // web-platform-tests because they may create non-determinism not
  // intended by the test author. They are still included in the stderr
  // output for debug purposes.
  bool dump_to_stderr = test_runner()->IsWebPlatformTestsMode();
  if (!message.text.IsEmpty()) {
    std::string new_message;
    new_message = message.text.Utf8();
    size_t file_protocol = new_message.find("file://");
    if (file_protocol != std::string::npos) {
      new_message = new_message.substr(0, file_protocol) +
                    URLSuitableForTestResult(new_message.substr(file_protocol));
    }
    console_message += new_message;
  }
  console_message += "\n";

  if (dump_to_stderr) {
    blink_test_runner()->PrintMessageToStderr(console_message);
  } else {
    blink_test_runner()->PrintMessage(console_message);
  }
}

void WebFrameTestProxy::DidStartLoading() {
  test_runner()->AddLoadingFrame(GetWebFrame());

  RenderFrameImpl::DidStartLoading();
}

void WebFrameTestProxy::DidStopLoading() {
  RenderFrameImpl::DidStopLoading();

  test_runner()->RemoveLoadingFrame(GetWebFrame());
}

void WebFrameTestProxy::DidChangeSelection(bool is_selection_empty) {
  if (test_runner()->ShouldDumpEditingCallbacks()) {
    blink_test_runner()->PrintMessage(
        "EDITING DELEGATE: "
        "webViewDidChangeSelection:WebViewDidChangeSelectionNotification\n");
  }
  RenderFrameImpl::DidChangeSelection(is_selection_empty);
}

void WebFrameTestProxy::DidChangeContents() {
  if (test_runner()->ShouldDumpEditingCallbacks()) {
    blink_test_runner()->PrintMessage(
        "EDITING DELEGATE: webViewDidChange:WebViewDidChangeNotification\n");
  }
  RenderFrameImpl::DidChangeContents();
}

blink::WebEffectiveConnectionType
WebFrameTestProxy::GetEffectiveConnectionType() {
  blink::WebEffectiveConnectionType connection_type =
      test_runner()->effective_connection_type();
  if (connection_type != blink::WebEffectiveConnectionType::kTypeUnknown)
    return connection_type;
  return RenderFrameImpl::GetEffectiveConnectionType();
}

void WebFrameTestProxy::ShowContextMenu(
    const blink::WebContextMenuData& context_menu_data) {
  WebWidgetTestProxy* widget_proxy = GetLocalRootWebWidgetTestProxy();
  widget_proxy->event_sender()->SetContextMenuData(context_menu_data);

  RenderFrameImpl::ShowContextMenu(context_menu_data);
}

void WebFrameTestProxy::DidDispatchPingLoader(const blink::WebURL& url) {
  if (test_runner()->ShouldDumpPingLoaderCallbacks()) {
    blink_test_runner()->PrintMessage(
        std::string("PingLoader dispatched to '") +
        web_test_string_util::URLDescription(url).c_str() + "'.\n");
  }

  RenderFrameImpl::DidDispatchPingLoader(url);
}

void WebFrameTestProxy::WillSendRequest(blink::WebURLRequest& request) {
  RenderFrameImpl::WillSendRequest(request);

  // Need to use GURL for host() and SchemeIs()
  GURL url = request.Url();

  // Warning: this may be null in some cross-site cases.
  net::SiteForCookies site_for_cookies = request.SiteForCookies();

  if (test_runner()->HttpHeadersToClear()) {
    for (const std::string& header : *test_runner()->HttpHeadersToClear()) {
      DCHECK(!base::EqualsCaseInsensitiveASCII(header, "referer"));
      request.ClearHttpHeaderField(blink::WebString::FromUTF8(header));
    }
  }

  if (test_runner()->ClearReferrer()) {
    request.SetReferrerString(blink::WebString());
    request.SetReferrerPolicy(
        content::Referrer::NetReferrerPolicyToBlinkReferrerPolicy(
            content::Referrer::GetDefaultReferrerPolicy()));
  }

  std::string host = url.host();
  if (!host.empty() &&
      (url.SchemeIs(url::kHttpScheme) || url.SchemeIs(url::kHttpsScheme))) {
    if (!IsLocalHost(host) && !IsTestHost(host) &&
        !HostIsUsedBySomeTestsToGenerateError(host) &&
        ((site_for_cookies.scheme() != url::kHttpScheme &&
          site_for_cookies.scheme() != url::kHttpsScheme) ||
         IsLocalHost(site_for_cookies.registrable_domain())) &&
        !blink_test_runner()->AllowExternalPages()) {
      blink_test_runner()->PrintMessage(
          std::string("Blocked access to external URL ") +
          url.possibly_invalid_spec() + "\n");
      BlockRequest(request);
      return;
    }
  }

  // Set the new substituted URL.
  request.SetUrl(RewriteWebTestsURL(request.Url().GetString().Utf8(),
                                    test_runner()->IsWebPlatformTestsMode()));
}

void WebFrameTestProxy::BeginNavigation(
    std::unique_ptr<blink::WebNavigationInfo> info) {
  if (test_runner()->ShouldDumpNavigationPolicy()) {
    blink_test_runner()->PrintMessage(
        "Default policy for navigation to '" +
        web_test_string_util::URLDescription(info->url_request.Url()) +
        "' is '" +
        web_test_string_util::WebNavigationPolicyToString(
            info->navigation_policy) +
        "'\n");
  }

  if (test_runner()->ShouldDumpFrameLoadCallbacks()) {
    GURL url = info->url_request.Url();
    std::string description = GetFrameDescriptionForWebTests();
    blink_test_runner()->PrintMessage(description +
                                      " - BeginNavigation request to '");
    blink_test_runner()->PrintMessage(
        DescriptionSuitableForTestResult(url.possibly_invalid_spec()));
    blink_test_runner()->PrintMessage("', http method ");
    blink_test_runner()->PrintMessage(
        info->url_request.HttpMethod().Utf8().data());
    blink_test_runner()->PrintMessage("\n");
  }

  bool should_continue = true;
  if (test_runner()->PolicyDelegateEnabled()) {
    blink_test_runner()->PrintMessage(
        std::string("Policy delegate: attempt to load ") +
        web_test_string_util::URLDescription(info->url_request.Url()) +
        " with navigation type '" +
        WebNavigationTypeToString(info->navigation_type) + "'\n");
    should_continue = test_runner()->PolicyDelegateIsPermissive();
    if (test_runner()->PolicyDelegateShouldNotifyDone()) {
      test_runner()->PolicyDelegateDone();
      should_continue = false;
    }
  }

  if (test_runner()->HttpHeadersToClear()) {
    for (const std::string& header : *test_runner()->HttpHeadersToClear()) {
      DCHECK(!base::EqualsCaseInsensitiveASCII(header, "referer"));
      info->url_request.ClearHttpHeaderField(
          blink::WebString::FromUTF8(header));
    }
  }

  if (test_runner()->ClearReferrer()) {
    info->url_request.SetReferrerString(blink::WebString());
    info->url_request.SetReferrerPolicy(
        network::mojom::ReferrerPolicy::kDefault);
  }

  info->url_request.SetUrl(
      RewriteWebTestsURL(info->url_request.Url().GetString().Utf8(),
                         test_runner()->IsWebPlatformTestsMode()));

  if (!should_continue)
    return;

  RenderFrameImpl::BeginNavigation(std::move(info));
}

void WebFrameTestProxy::PostAccessibilityEvent(const ui::AXEvent& event) {
  const char* event_name = nullptr;
  switch (event.event_type) {
    case ax::mojom::Event::kActiveDescendantChanged:
      event_name = "ActiveDescendantChanged";
      break;
    case ax::mojom::Event::kAriaAttributeChanged:
      event_name = "AriaAttributeChanged";
      break;
    case ax::mojom::Event::kAutocorrectionOccured:
      event_name = "AutocorrectionOccured";
      break;
    case ax::mojom::Event::kBlur:
      event_name = "Blur";
      break;
    case ax::mojom::Event::kCheckedStateChanged:
      event_name = "CheckedStateChanged";
      break;
    case ax::mojom::Event::kChildrenChanged:
      event_name = "ChildrenChanged";
      break;
    case ax::mojom::Event::kClicked:
      event_name = "Clicked";
      break;
    case ax::mojom::Event::kDocumentSelectionChanged:
      event_name = "DocumentSelectionChanged";
      break;
    case ax::mojom::Event::kDocumentTitleChanged:
      event_name = "DocumentTitleChanged";
      break;
    case ax::mojom::Event::kFocus:
      event_name = "Focus";
      break;
    case ax::mojom::Event::kHover:
      event_name = "Hover";
      break;
    case ax::mojom::Event::kInvalidStatusChanged:
      event_name = "InvalidStatusChanged";
      break;
    case ax::mojom::Event::kLayoutComplete:
      event_name = "LayoutComplete";
      break;
    case ax::mojom::Event::kLiveRegionChanged:
      event_name = "LiveRegionChanged";
      break;
    case ax::mojom::Event::kLoadComplete:
      event_name = "LoadComplete";
      break;
    case ax::mojom::Event::kLocationChanged:
      event_name = "LocationChanged";
      break;
    case ax::mojom::Event::kMenuListItemSelected:
      event_name = "MenuListItemSelected";
      break;
    case ax::mojom::Event::kMenuListValueChanged:
      event_name = "MenuListValueChanged";
      break;
    case ax::mojom::Event::kRowCollapsed:
      event_name = "RowCollapsed";
      break;
    case ax::mojom::Event::kRowCountChanged:
      event_name = "RowCountChanged";
      break;
    case ax::mojom::Event::kRowExpanded:
      event_name = "RowExpanded";
      break;
    case ax::mojom::Event::kScrollPositionChanged:
      event_name = "ScrollPositionChanged";
      break;
    case ax::mojom::Event::kScrolledToAnchor:
      event_name = "ScrolledToAnchor";
      break;
    case ax::mojom::Event::kSelectedChildrenChanged:
      event_name = "SelectedChildrenChanged";
      break;
    case ax::mojom::Event::kTextSelectionChanged:
      event_name = "SelectedTextChanged";
      break;
    case ax::mojom::Event::kTextChanged:
      event_name = "TextChanged";
      break;
    case ax::mojom::Event::kValueChanged:
      event_name = "ValueChanged";
      break;
    default:
      event_name = "Unknown";
      break;
  }

  blink::WebDocument document = GetWebFrame()->GetDocument();
  auto object = blink::WebAXObject::FromWebDocumentByID(document, event.id);
  HandleWebAccessibilityEvent(std::move(object), event_name,
                              event.event_intents);

  RenderFrameImpl::PostAccessibilityEvent(event);
}

void WebFrameTestProxy::MarkWebAXObjectDirty(const blink::WebAXObject& object,
                                             bool subtree) {
  HandleWebAccessibilityEvent(object, "MarkDirty",
                              std::vector<ui::AXEventIntent>());

  // Guard against the case where |this| was deleted as a result of an
  // accessibility listener detaching a frame. If that occurs, the
  // WebAXObject will be detached.
  if (object.IsDetached())
    return;  // |this| is invalid.

  RenderFrameImpl::MarkWebAXObjectDirty(object, subtree);
}

void WebFrameTestProxy::HandleWebAccessibilityEvent(
    const blink::WebAXObject& object,
    const char* event_name,
    const std::vector<ui::AXEventIntent>& event_intents) {
  // Only hook the accessibility events that occurred during the test run.
  // This check prevents false positives in BlinkLeakDetector.
  // The pending tasks in browser/renderer message queue may trigger
  // accessibility events,
  // and AccessibilityController will hold on to their target nodes if we don't
  // ignore them here.
  if (!test_runner()->TestIsRunning())
    return;

  AccessibilityController* accessibility_controller =
      web_view_test_proxy_->accessibility_controller();

  accessibility_controller->NotificationReceived(GetWebFrame(), object,
                                                 event_name, event_intents);

  if (accessibility_controller->ShouldLogAccessibilityEvents()) {
    std::string message("AccessibilityNotification - ");
    message += event_name;

    blink::WebNode node = object.GetNode();
    if (!node.IsNull() && node.IsElementNode()) {
      blink::WebElement element = node.To<blink::WebElement>();
      if (element.HasAttribute("id")) {
        message += " - id:";
        message += element.GetAttribute("id").Utf8().data();
      }
    }

    blink_test_runner()->PrintMessage(message + "\n");
  }
}

void WebFrameTestProxy::CheckIfAudioSinkExistsAndIsAuthorized(
    const blink::WebString& sink_id,
    blink::WebSetSinkIdCompleteCallback completion_callback) {
  std::string device_id = sink_id.Utf8();
  if (device_id == "valid" || device_id.empty())
    std::move(completion_callback).Run(/*error =*/base::nullopt);
  else if (device_id == "unauthorized")
    std::move(completion_callback)
        .Run(blink::WebSetSinkIdError::kNotAuthorized);
  else
    std::move(completion_callback).Run(blink::WebSetSinkIdError::kNotFound);

  // Intentionally does not call RenderFrameImpl.
}

void WebFrameTestProxy::DidClearWindowObject() {
  TestInterfaces* interfaces = web_view_test_proxy_->test_interfaces();

  // These calls will install the various JS bindings for web tests into the
  // frame before JS has a chance to run.
  GCController::Install(GetWebFrame());
  interfaces->Install(GetWebFrame());
  test_runner()->Install(static_cast<RenderFrame*>(this), spell_check_.get(),
                         web_view_test_proxy_->view_test_runner());
  web_view_test_proxy_->Install(GetWebFrame());
  GetLocalRootWebWidgetTestProxy()->Install(GetWebFrame());
  blink::WebTestingSupport::InjectInternalsObject(GetWebFrame());

  RenderFrameImpl::DidClearWindowObject();
}

WebWidgetTestProxy* WebFrameTestProxy::GetLocalRootWebWidgetTestProxy() {
  return static_cast<WebWidgetTestProxy*>(GetLocalRootRenderWidget());
}

void WebFrameTestProxy::CaptureDump(CaptureDumpCallback callback) {
  blink_test_runner()->CaptureDump(std::move(callback));
}

void WebFrameTestProxy::CompositeWithRaster(
    CompositeWithRasterCallback callback) {
  // When the TestFinished() occurred, if the browser is capturing pixels, it
  // asks each composited RenderFrame to submit a new frame via here.
  UpdateAllLifecyclePhasesAndCompositeForTesting();
  std::move(callback).Run();
}

void WebFrameTestProxy::DumpFrameLayout(DumpFrameLayoutCallback callback) {
  std::string dump = test_runner()->DumpLayout(GetWebFrame());
  std::move(callback).Run(std::move(dump));
}

void WebFrameTestProxy::ReplicateTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr config) {
  blink_test_runner()->OnReplicateTestConfiguration(std::move(config));
}

void WebFrameTestProxy::SetTestConfiguration(
    mojom::WebTestRunTestConfigurationPtr config) {
  blink_test_runner()->OnSetTestConfiguration(std::move(config));
}

void WebFrameTestProxy::SetupRendererProcessForNonTestWindow() {
  blink_test_runner()->OnSetupRendererProcessForNonTestWindow();
}

void WebFrameTestProxy::ResetRendererAfterWebTest() {
  blink_test_runner()->OnResetRendererAfterWebTest();
}

void WebFrameTestProxy::FinishTestInMainWindow() {
  blink_test_runner()->OnFinishTestInMainWindow();
}

void WebFrameTestProxy::LayoutDumpCompleted(
    const std::string& completed_layout_dump) {
  blink_test_runner()->OnLayoutDumpCompleted(completed_layout_dump);
}

void WebFrameTestProxy::ReplyBluetoothManualChooserEvents(
    const std::vector<std::string>& events) {
  blink_test_runner()->OnReplyBluetoothManualChooserEvents(events);
}

void WebFrameTestProxy::BindReceiver(
    mojo::PendingAssociatedReceiver<mojom::WebTestRenderFrame> receiver) {
  web_test_render_frame_receiver_.Bind(
      std::move(receiver),
      GetWebFrame()->GetTaskRunner(blink::TaskType::kInternalTest));
}

TestRunner* WebFrameTestProxy::test_runner() {
  return web_view_test_proxy_->test_interfaces()->GetTestRunner();
}

BlinkTestRunner* WebFrameTestProxy::blink_test_runner() {
  return web_view_test_proxy_->blink_test_runner();
}

}  // namespace content
