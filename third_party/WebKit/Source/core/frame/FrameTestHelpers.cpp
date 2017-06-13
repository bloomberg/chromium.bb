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
TestWebFrameClient* TestClientForFrame(WebFrame* frame) {
  return static_cast<TestWebFrameClient*>(ToWebLocalFrameBase(frame)->Client());
}

void RunServeAsyncRequestsTask(TestWebFrameClient* client) {
  Platform::Current()->GetURLLoaderMockFactory()->ServeAsynchronousRequests();
  if (client->IsLoading()) {
    Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        WTF::Bind(&RunServeAsyncRequestsTask, WTF::Unretained(client)));
  } else {
    testing::ExitRunLoop();
  }
}

TestWebFrameClient* DefaultWebFrameClient() {
  DEFINE_STATIC_LOCAL(TestWebFrameClient, client, ());
  return &client;
}

TestWebWidgetClient* DefaultWebWidgetClient() {
  DEFINE_STATIC_LOCAL(TestWebWidgetClient, client, ());
  return &client;
}

}  // namespace

void LoadFrame(WebFrame* frame, const std::string& url) {
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

void LoadHistoryItem(WebFrame* frame,
                     const WebHistoryItem& item,
                     WebHistoryLoadType load_type,
                     WebCachePolicy cache_policy) {
  WebURLRequest request =
      frame->ToWebLocalFrame()->RequestFromHistoryItem(item, cache_policy);
  frame->ToWebLocalFrame()->Load(request, WebFrameLoadType::kBackForward, item);
  PumpPendingRequestsForFrameToLoad(frame);
}

void ReloadFrame(WebFrame* frame) {
  frame->Reload(WebFrameLoadType::kReload);
  PumpPendingRequestsForFrameToLoad(frame);
}

void ReloadFrameBypassingCache(WebFrame* frame) {
  frame->Reload(WebFrameLoadType::kReloadBypassingCache);
  PumpPendingRequestsForFrameToLoad(frame);
}

void PumpPendingRequestsForFrameToLoad(WebFrame* frame) {
  Platform::Current()->CurrentThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE, WTF::Bind(&RunServeAsyncRequestsTask,
                                 WTF::Unretained(TestClientForFrame(frame))));
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

WebLocalFrameBase* CreateLocalChild(WebRemoteFrame* parent,
                                    const WebString& name,
                                    WebFrameClient* client,
                                    WebWidgetClient* widget_client,
                                    WebFrame* previous_sibling,
                                    const WebFrameOwnerProperties& properties) {
  if (!client)
    client = DefaultWebFrameClient();

  WebLocalFrameBase* frame = ToWebLocalFrameBase(parent->CreateLocalChild(
      WebTreeScopeType::kDocument, name, WebSandboxFlags::kNone, client,
      static_cast<TestWebFrameClient*>(client)
          ->GetInterfaceProviderForTesting(),
      nullptr, previous_sibling, WebParsedFeaturePolicy(), properties,
      nullptr));

  if (!widget_client)
    widget_client = DefaultWebWidgetClient();
  WebFrameWidget::Create(widget_client, frame);

  return frame;
}

WebRemoteFrameImpl* CreateRemoteChild(WebRemoteFrame* parent,
                                      WebRemoteFrameClient* client,
                                      const WebString& name) {
  return ToWebRemoteFrameImpl(parent->CreateRemoteChild(
      WebTreeScopeType::kDocument, name, WebSandboxFlags::kNone,
      WebParsedFeaturePolicy(), client, nullptr));
}

WebViewHelper::WebViewHelper(SettingOverrider* setting_overrider)
    : web_view_(nullptr), setting_overrider_(setting_overrider) {}

WebViewHelper::~WebViewHelper() {
  Reset();
}

WebViewBase* WebViewHelper::InitializeWithOpener(
    WebFrame* opener,
    bool enable_javascript,
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client,
    void (*update_settings_func)(WebSettings*)) {
  Reset();

  if (!web_frame_client)
    web_frame_client = DefaultWebFrameClient();
  if (!web_view_client) {
    owned_test_web_view_client_ = WTF::MakeUnique<TestWebViewClient>();
    web_view_client = owned_test_web_view_client_.get();
  }
  if (!web_widget_client)
    web_widget_client = web_view_client->WidgetClient();
  web_view_ = static_cast<WebViewBase*>(
      WebView::Create(web_view_client, kWebPageVisibilityStateVisible));
  web_view_->GetSettings()->SetJavaScriptEnabled(enable_javascript);
  web_view_->GetSettings()->SetPluginsEnabled(true);
  // Enable (mocked) network loads of image URLs, as this simplifies
  // the completion of resource loads upon test shutdown & helps avoid
  // dormant loads trigger Resource leaks for image loads.
  //
  // Consequently, all external image resources must be mocked.
  web_view_->GetSettings()->SetLoadsImagesAutomatically(true);
  if (update_settings_func)
    update_settings_func(web_view_->GetSettings());
  if (setting_overrider_)
    setting_overrider_->OverrideSettings(web_view_->GetSettings());
  web_view_->SetDeviceScaleFactor(
      web_view_client->GetScreenInfo().device_scale_factor);
  web_view_->SetDefaultPageScaleLimits(1, 4);
  WebLocalFrame* frame = WebLocalFrameBase::Create(
      WebTreeScopeType::kDocument, web_frame_client,
      web_frame_client->GetInterfaceProviderForTesting(), nullptr, opener);
  web_view_->SetMainFrame(frame);
  web_frame_client->SetFrame(frame);
  // TODO(dcheng): The main frame widget currently has a special case.
  // Eliminate this once WebView is no longer a WebWidget.
  blink::WebFrameWidget::Create(web_widget_client, web_view_, frame);

  test_web_view_client_ = web_view_client;

  return web_view_;
}

WebViewBase* WebViewHelper::Initialize(
    bool enable_javascript,
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client,
    void (*update_settings_func)(WebSettings*)) {
  return InitializeWithOpener(nullptr, enable_javascript, web_frame_client,
                              web_view_client, web_widget_client,
                              update_settings_func);
}

WebViewBase* WebViewHelper::InitializeAndLoad(
    const std::string& url,
    bool enable_javascript,
    TestWebFrameClient* web_frame_client,
    TestWebViewClient* web_view_client,
    TestWebWidgetClient* web_widget_client,
    void (*update_settings_func)(WebSettings*)) {
  Initialize(enable_javascript, web_frame_client, web_view_client,
             web_widget_client, update_settings_func);

  LoadFrame(WebView()->MainFrame(), url);

  return WebView();
}

void WebViewHelper::Reset() {
  if (web_view_) {
    DCHECK(web_view_->MainFrame()->IsWebRemoteFrame() ||
           !TestClientForFrame(web_view_->MainFrame())->IsLoading());
    web_view_->WillCloseLayerTreeView();
    web_view_->Close();
    web_view_ = nullptr;
  }
}

void WebViewHelper::Resize(WebSize size) {
  test_web_view_client_->ClearAnimationScheduled();
  WebView()->Resize(size);
  EXPECT_FALSE(test_web_view_client_->AnimationScheduled());
  test_web_view_client_->ClearAnimationScheduled();
}

TestWebFrameClient::TestWebFrameClient() {}

void TestWebFrameClient::FrameDetached(WebLocalFrame* frame, DetachType type) {
  if (frame->FrameWidget())
    frame->FrameWidget()->Close();

  frame->Close();
}

WebLocalFrame* TestWebFrameClient::CreateChildFrame(
    WebLocalFrame* parent,
    WebTreeScopeType scope,
    const WebString& name,
    const WebString& fallback_name,
    WebSandboxFlags sandbox_flags,
    const WebParsedFeaturePolicy& container_policy,
    const WebFrameOwnerProperties& frame_owner_properties) {
  WebLocalFrame* frame = parent->CreateLocalChild(
      scope, this, GetInterfaceProviderForTesting(), nullptr);
  return frame;
}

void TestWebFrameClient::DidStartLoading(bool) {
  ++loads_in_progress_;
}

void TestWebFrameClient::DidStopLoading() {
  DCHECK_GT(loads_in_progress_, 0);
  --loads_in_progress_;
}

TestWebRemoteFrameClient::TestWebRemoteFrameClient()
    : frame_(WebRemoteFrameImpl::Create(WebTreeScopeType::kDocument,
                                        this,
                                        nullptr)) {}

void TestWebRemoteFrameClient::FrameDetached(DetachType type) {
  frame_->Close();
}

WebLayerTreeView* TestWebViewClient::InitializeLayerTreeView() {
  layer_tree_view_ = WTF::WrapUnique(new WebLayerTreeViewImplForTesting);
  return layer_tree_view_.get();
}

WebLayerTreeView* TestWebViewWidgetClient::InitializeLayerTreeView() {
  return test_web_view_client_->InitializeLayerTreeView();
}

WebLayerTreeView* TestWebWidgetClient::InitializeLayerTreeView() {
  layer_tree_view_ = WTF::MakeUnique<WebLayerTreeViewImplForTesting>();
  return layer_tree_view_.get();
}

void TestWebViewWidgetClient::ScheduleAnimation() {
  test_web_view_client_->ScheduleAnimation();
}

void TestWebViewWidgetClient::DidMeaningfulLayout(
    WebMeaningfulLayout layout_type) {
  test_web_view_client_->DidMeaningfulLayout(layout_type);
}

}  // namespace FrameTestHelpers
}  // namespace blink
