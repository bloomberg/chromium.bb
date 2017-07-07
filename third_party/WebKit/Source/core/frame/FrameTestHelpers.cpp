/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "core/frame/FrameTestHelpers.h"

#include <utility>

#include "core/exported/WebRemoteFrameImpl.h"
#include "core/frame/WebLocalFrameBase.h"
#include "platform/testing/URLTestHelpers.h"
#include "platform/testing/UnitTestHelpers.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "platform/wtf/Functional.h"
#include "platform/wtf/PtrUtil.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/StringBuilder.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "public/platform/WebString.h"
#include "public/platform/WebThread.h"
#include "public/platform/WebURLLoaderMockFactory.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebURLResponse.h"
#include "public/web/WebFrameWidget.h"
#include "public/web/WebSettings.h"
#include "public/web/WebTreeScopeType.h"
#include "public/web/WebViewClient.h"

namespace blink {
namespace FrameTestHelpers {

namespace {

// The frame test helpers coordinate frame loads in a carefully choreographed
// dance. Since the parser is threaded, simply spinning the run loop once is not
// enough to ensure completion of a load. Instead, the following pattern is
// used to ensure that tests see the final state:
// 1. Starts a load.
// 2. Enter the run loop.
// 3. Posted task triggers the load, and starts pumping pending resource
//    requests using runServeAsyncRequestsTask().
// 4. TestWebFrameClient watches for didStartLoading/didStopLoading calls,
//    keeping track of how many loads it thinks are in flight.
// 5. While runServeAsyncRequestsTask() observes TestWebFrameClient to still
//    have loads in progress, it posts itself back to the run loop.
// 6. When runServeAsyncRequestsTask() notices there are no more loads in
//    progress, it exits the run loop.
// 7. At this point, all parsing, resource loads, and layout should be finished.

void RunServeAsyncRequestsTask() {
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  if (TestWebFrameClient::IsLoading()) {
    Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE, WTF::Bind(&RunServeAsyncRequestsTask));
  } else {
    testing::ExitRunLoop();
  }
}

// Helper to create a default test client if the supplied client pointer is
// null.
template <typename T>
std::unique_ptr<T> CreateDefaultClientIfNeeded(T*& client) {
  if (client)
    return nullptr;
  auto owned_client = WTF::MakeUnique<T>();
  client = owned_client.get();
  return owned_client;
}

}  // namespace

void LoadFrame(WebLocalFrame* frame, const std::string& url) {
  WebURLRequest url_request(URLTestHelpers::ToKURL(url));
  frame->LoadRequest(url_request);
  PumpPendingRequestsForFrameToLoad(frame);
}

void LoadHTMLString(WebLocalFrame* frame,
                    const std::string& html,
                    const WebURL& base_url) {
  frame->LoadHTMLString(WebData(html.data(), html.size()), base_url);
  PumpPendingRequestsForFrameToLoad(frame);
}

void LoadHistoryItem(WebLocalFrame* frame,
                     const WebHistoryItem& item,
                     WebHistoryLoadType load_type,
                     WebCachePolicy cache_policy) {
  WebURLRequest request = frame->RequestFromHistoryItem(item, cache_policy);
  frame->Load(request, WebFrameLoadType::kBackForward, item);
  PumpPendingRequestsForFrameToLoad(frame);
}

void ReloadFrame(WebLocalFrame* frame) {
  frame->Reload(WebFrameLoadType::kReload);
  PumpPendingRequestsForFrameToLoad(frame);
}

void ReloadFrameBypassingCache(WebLocalFrame* frame) {
  frame->Reload(WebFrameLoadType::kReloadBypassingCache);
  PumpPendingRequestsForFrameToLoad(frame);
}

void PumpPendingRequestsForFrameToLoad(WebFrame* frame) {
  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&RunServeAsyncRequestsTask));
  testing::EnterRunLoop();
}

WebMouseEvent CreateMouseEvent(WebInputEvent::Type type,
                               WebMouseEvent::Button button,
                               const IntPoint& point,
                               int modifiers) {
  WebMouseEvent result(type, modifiers, WebInputEvent::kTimeStampForTesting);
  result.SetPositionInWidget(point.X(), point.Y());
  result.SetPositionInScreen(point.X(), point.Y());
  result.button = button;
  result.click_count = 1;
  return result;
}

WebLocalFrameBase* CreateLocalChild(WebLocalFrame& parent,
                                    WebTreeScopeType scope,
                                    TestWebFrameClient* client) {
  auto owned_client = CreateDefaultClientIfNeeded(client);
  WebLocalFrameBase* frame =
      ToWebLocalFrameBase(parent.CreateLocalChild(scope, client, nullptr));
  client->Bind(frame, std::move(owned_client));
  return frame;
}

WebLocalFrameBase* CreateLocalChild(
    WebLocalFrame& parent,
    WebTreeScopeType scope,
    std::unique_ptr<TestWebFrameClient> self_owned) {
  DCHECK(self_owned);
  TestWebFrameClient* client = self_owned.get();
  WebLocalFrameBase* frame =
      ToWebLocalFrameBase(parent.CreateLocalChild(scope, client, nullptr));
  client->Bind(frame, std::move(self_owned));
  return frame;
}

WebLocalFrameBase* CreateProvisional(WebRemoteFrame& old_frame,
                                     TestWebFrameClient* client) {
  auto owned_client = CreateDefaultClientIfNeeded(client);
  WebLocalFrameBase* frame =
      ToWebLocalFrameBase(WebLocalFrame::CreateProvisional(
          client, nullptr, &old_frame, WebSandboxFlags::kNone,
          WebParsedFeaturePolicy()));
  client->Bind(frame, std::move(owned_client));
  // Create a local root, if necessary.
  std::unique_ptr<TestWebWidgetClient> owned_widget_client;
  if (!frame->Parent()) {
    // TODO(dcheng): The main frame widget currently has a special case.
    // Eliminate this once WebView is no longer a WebWidget.
    owned_widget_client = WTF::MakeUnique<TestWebViewWidgetClient>(
        *static_cast<TestWebViewClient*>(frame->ViewImpl()->Client()));
  } else if (frame->Parent()->IsWebRemoteFrame()) {
    owned_widget_client = WTF::MakeUnique<TestWebWidgetClient>();
  }
  if (owned_widget_client) {
    WebFrameWidget::Create(owned_widget_client.get(), frame);
    client->BindWidgetClient(std::move(owned_widget_client));
  }
  return frame;
}

WebRemoteFrameImpl* CreateRemote(TestWebRemoteFrameClient* client) {
  auto owned_client = CreateDefaultClientIfNeeded(client);
  auto* frame = WebRemoteFrameImpl::Create(WebTreeScopeType::kDocument, client);
  client->Bind(frame, std::move(owned_client));
  return frame;
}

WebLocalFrameBase* CreateLocalChild(WebRemoteFrame& parent,
                                    const WebString& name,
                                    const WebFrameOwnerProperties& properties,
                                    WebFrame* previous_sibling,
                                    TestWebFrameClient* client,
                                    TestWebWidgetClient* widget_client) {
  auto owned_client = CreateDefaultClientIfNeeded(client);
  auto* frame = ToWebLocalFrameBase(parent.CreateLocalChild(
      WebTreeScopeType::kDocument, name, WebSandboxFlags::kNone, client,
      nullptr, previous_sibling, WebParsedFeaturePolicy(), properties,
      nullptr));
  client->Bind(frame, std::move(owned_client));

  auto owned_widget_client = CreateDefaultClientIfNeeded(widget_client);
  WebFrameWidget::Create(widget_client, frame);
  client->BindWidgetClient(std::move(owned_widget_client));
  return frame;
}

WebRemoteFrameImpl* CreateRemoteChild(WebRemoteFrame& parent,
                                      const WebString& name,
                                      RefPtr<SecurityOrigin> security_origin,
                                      TestWebRemoteFrameClient* client) {
  auto owned_client = CreateDefaultClientIfNeeded(client);
  auto* frame = ToWebRemoteFrameImpl(parent.CreateRemoteChild(
      WebTreeScopeType::kDocument, name, WebSandboxFlags::kNone,
      WebParsedFeaturePolicy(), client, nullptr));
  client->Bind(frame, std::move(owned_client));
  if (!security_origin)
    security_origin = SecurityOrigin::CreateUnique();
  frame->GetFrame()->GetSecurityContext()->SetReplicatedOrigin(
      std::move(security_origin));
  return frame;
}

WebViewHelper::WebViewHelper() : web_view_(nullptr) {}

WebViewHelper::~WebViewHelper() {
  Reset();
}

WebViewBase* WebViewHelper::InitializeWithOpener(
    WebFrame* opener,
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client,
    void (*update_settings_func)(WebSettings*)) {
  Reset();

  InitializeWebView(web_view_client);
  if (update_settings_func)
    update_settings_func(web_view_->GetSettings());

  auto owned_web_frame_client = CreateDefaultClientIfNeeded(web_frame_client);
  WebLocalFrame* frame = WebLocalFrame::CreateMainFrame(
      web_view_, web_frame_client, nullptr, opener);
  web_frame_client->Bind(frame, std::move(owned_web_frame_client));

  // TODO(dcheng): The main frame widget currently has a special case.
  // Eliminate this once WebView is no longer a WebWidget.
  std::unique_ptr<TestWebWidgetClient> owned_web_widget_client;
  if (!web_widget_client) {
    owned_web_widget_client =
        WTF::MakeUnique<TestWebViewWidgetClient>(*test_web_view_client_);
    web_widget_client = owned_web_widget_client.get();
  }
  blink::WebFrameWidget::Create(web_widget_client, frame);
  web_frame_client->BindWidgetClient(std::move(owned_web_widget_client));

  return web_view_;
}

WebViewBase* WebViewHelper::Initialize(
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client,
    void (*update_settings_func)(WebSettings*)) {
  return InitializeWithOpener(nullptr, web_frame_client, web_view_client,
                              web_widget_client, update_settings_func);
}

WebViewBase* WebViewHelper::InitializeAndLoad(
    const std::string& url,
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client,
    void (*update_settings_func)(WebSettings*)) {
  Initialize(web_frame_client, web_view_client, web_widget_client,
             update_settings_func);

  LoadFrame(WebView()->MainFrameImpl(), url);

  return WebView();
}

WebViewBase* WebViewHelper::InitializeRemote(
    TestWebRemoteFrameClient* web_remote_frame_client,
    RefPtr<SecurityOrigin> security_origin,
    TestWebViewClient* web_view_client) {
  Reset();

  InitializeWebView(web_view_client);

  auto owned_web_remote_frame_client =
      CreateDefaultClientIfNeeded(web_remote_frame_client);
  WebRemoteFrameImpl* frame = WebRemoteFrameImpl::CreateMainFrame(
      web_view_, web_remote_frame_client, nullptr);
  web_remote_frame_client->Bind(frame,
                                std::move(owned_web_remote_frame_client));
  if (!security_origin)
    security_origin = SecurityOrigin::CreateUnique();
  frame->GetFrame()->GetSecurityContext()->SetReplicatedOrigin(
      std::move(security_origin));
  return web_view_;
}

void WebViewHelper::Reset() {
  if (web_view_) {
    DCHECK(!TestWebFrameClient::IsLoading());
    web_view_->Close();
    web_view_ = nullptr;
  }
}

WebLocalFrameBase* WebViewHelper::LocalMainFrame() const {
  return ToWebLocalFrameBase(web_view_->MainFrame());
}

WebRemoteFrameBase* WebViewHelper::RemoteMainFrame() const {
  return ToWebRemoteFrameBase(web_view_->MainFrame());
}

void WebViewHelper::Resize(WebSize size) {
  test_web_view_client_->ClearAnimationScheduled();
  WebView()->Resize(size);
  EXPECT_FALSE(test_web_view_client_->AnimationScheduled());
  test_web_view_client_->ClearAnimationScheduled();
}

void WebViewHelper::InitializeWebView(TestWebViewClient* web_view_client) {
  owned_test_web_view_client_ = CreateDefaultClientIfNeeded(web_view_client);
  web_view_ = static_cast<WebViewBase*>(
      WebView::Create(web_view_client, kWebPageVisibilityStateVisible));
  web_view_->GetSettings()->SetJavaScriptEnabled(true);
  web_view_->GetSettings()->SetPluginsEnabled(true);
  // Enable (mocked) network loads of image URLs, as this simplifies
  // the completion of resource loads upon test shutdown & helps avoid
  // dormant loads trigger Resource leaks for image loads.
  //
  // Consequently, all external image resources must be mocked.
  web_view_->GetSettings()->SetLoadsImagesAutomatically(true);
  web_view_->SetDeviceScaleFactor(
      web_view_client->GetScreenInfo().device_scale_factor);
  web_view_->SetDefaultPageScaleLimits(1, 4);

  test_web_view_client_ = web_view_client;
}

int TestWebFrameClient::loads_in_progress_ = 0;

TestWebFrameClient::TestWebFrameClient()
    : interface_provider_(new service_manager::InterfaceProvider()) {}

void TestWebFrameClient::Bind(WebLocalFrame* frame,
                              std::unique_ptr<TestWebFrameClient> self_owned) {
  DCHECK(!frame_);
  DCHECK(!self_owned || self_owned.get() == this);
  frame_ = frame;
  self_owned_ = std::move(self_owned);
}

void TestWebFrameClient::BindWidgetClient(
    std::unique_ptr<TestWebWidgetClient> client) {
  DCHECK(!owned_widget_client_);
  owned_widget_client_ = std::move(client);
}

void TestWebFrameClient::FrameDetached(WebLocalFrame* frame, DetachType type) {
  if (frame_->FrameWidget()) {
    frame_->FrameWidget()->WillCloseLayerTreeView();
    frame_->FrameWidget()->Close();
  }

  owned_widget_client_.reset();
  frame_->Close();
  self_owned_.reset();
}

WebLocalFrame* TestWebFrameClient::CreateChildFrame(
    WebLocalFrame* parent,
    WebTreeScopeType scope,
    const WebString& name,
    const WebString& fallback_name,
    WebSandboxFlags sandbox_flags,
    const WebParsedFeaturePolicy& container_policy,
    const WebFrameOwnerProperties& frame_owner_properties) {
  return CreateLocalChild(*parent, scope);
}

void TestWebFrameClient::DidStartLoading(bool) {
  ++loads_in_progress_;
}

void TestWebFrameClient::DidStopLoading() {
  DCHECK_GT(loads_in_progress_, 0);
  --loads_in_progress_;
}

TestWebRemoteFrameClient::TestWebRemoteFrameClient() {}

void TestWebRemoteFrameClient::Bind(
    WebRemoteFrame* frame,
    std::unique_ptr<TestWebRemoteFrameClient> self_owned) {
  DCHECK(!frame_);
  DCHECK(!self_owned || self_owned.get() == this);
  frame_ = frame;
  self_owned_ = std::move(self_owned);
}

void TestWebRemoteFrameClient::FrameDetached(DetachType type) {
  frame_->Close();
  self_owned_.reset();
}

WebLayerTreeView* TestWebViewClient::InitializeLayerTreeView() {
  layer_tree_view_ = WTF::MakeUnique<WebLayerTreeViewImplForTesting>();
  return layer_tree_view_.get();
}

WebLayerTreeView* TestWebViewWidgetClient::InitializeLayerTreeView() {
  return test_web_view_client_.InitializeLayerTreeView();
}

WebLayerTreeView* TestWebWidgetClient::InitializeLayerTreeView() {
  layer_tree_view_ = WTF::MakeUnique<WebLayerTreeViewImplForTesting>();
  return layer_tree_view_.get();
}

void TestWebViewWidgetClient::ScheduleAnimation() {
  test_web_view_client_.ScheduleAnimation();
}

void TestWebViewWidgetClient::DidMeaningfulLayout(
    WebMeaningfulLayout layout_type) {
  test_web_view_client_.DidMeaningfulLayout(layout_type);
}

}  // namespace FrameTestHelpers
}  // namespace blink
