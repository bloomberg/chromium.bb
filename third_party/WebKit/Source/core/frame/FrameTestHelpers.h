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

#ifndef FrameTestHelpers_h
#define FrameTestHelpers_h

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <memory>
#include <string>

#include "base/macros.h"
#include "core/exported/WebViewImpl.h"
#include "core/frame/Settings.h"
#include "platform/WebTaskRunner.h"
#include "platform/runtime_enabled_features.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "platform/testing/UseMockScrollbarSettings.h"
#include "platform/testing/WebLayerTreeViewImplForTesting.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMouseEvent.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/modules/fetch/fetch_api_request.mojom-shared.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameOwnerProperties.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebRemoteFrameClient.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"
#include "services/service_manager/public/cpp/interface_provider.h"

#define EXPECT_FLOAT_POINT_EQ(expected, actual)    \
  do {                                             \
    EXPECT_FLOAT_EQ((expected).X(), (actual).X()); \
    EXPECT_FLOAT_EQ((expected).Y(), (actual).Y()); \
  } while (false)

#define EXPECT_FLOAT_SIZE_EQ(expected, actual)               \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).Width(), (actual).Width());   \
    EXPECT_FLOAT_EQ((expected).Height(), (actual).Height()); \
  } while (false)

#define EXPECT_FLOAT_RECT_EQ(expected, actual)               \
  do {                                                       \
    EXPECT_FLOAT_EQ((expected).X(), (actual).X());           \
    EXPECT_FLOAT_EQ((expected).Y(), (actual).Y());           \
    EXPECT_FLOAT_EQ((expected).Width(), (actual).Width());   \
    EXPECT_FLOAT_EQ((expected).Height(), (actual).Height()); \
  } while (false)

namespace blink {

class WebFrame;
class WebLocalFrameImpl;
class WebRemoteFrameImpl;
class WebSettings;

namespace FrameTestHelpers {

class TestWebFrameClient;
class TestWebRemoteFrameClient;
class TestWebWidgetClient;
class TestWebViewClient;

// Loads a url into the specified WebLocalFrame for testing purposes. Pumps any
// pending resource requests, as well as waiting for the threaded parser to
// finish, before returning.
void LoadFrame(WebLocalFrame*, const std::string& url);
// Same as above, but for WebLocalFrame::LoadHTMLString().
void LoadHTMLString(WebLocalFrame*,
                    const std::string& html,
                    const WebURL& base_url);
// Same as above, but for WebLocalFrame::RequestFromHistoryItem/Load.
void LoadHistoryItem(WebLocalFrame*,
                     const WebHistoryItem&,
                     WebHistoryLoadType,
                     mojom::FetchCacheMode);
// Same as above, but for WebLocalFrame::Reload().
void ReloadFrame(WebLocalFrame*);
void ReloadFrameBypassingCache(WebLocalFrame*);

// Pumps pending resource requests while waiting for a frame to load. Consider
// using one of the above helper methods whenever possible.
void PumpPendingRequestsForFrameToLoad(WebFrame*);

WebMouseEvent CreateMouseEvent(WebInputEvent::Type,
                               WebMouseEvent::Button,
                               const IntPoint&,
                               int modifiers);

// Helpers for creating frames for test purposes. All methods that accept raw
// pointer client arguments allow nullptr as a valid argument; if a client
// pointer is null, the test framework will automatically create and manage the
// lifetime of that client interface. Otherwise, the caller is responsible for
// ensuring that non-null clients outlive the created frame.

// Helper for creating a local child frame of a local parent frame.
WebLocalFrameImpl* CreateLocalChild(WebLocalFrame& parent,
                                    WebTreeScopeType,
                                    TestWebFrameClient* = nullptr);

// Similar, but unlike the overload which takes the client as a raw pointer,
// ownership of the TestWebFrameClient is transferred to the test framework.
// TestWebFrameClient may not be null.
WebLocalFrameImpl* CreateLocalChild(WebLocalFrame& parent,
                                    WebTreeScopeType,
                                    std::unique_ptr<TestWebFrameClient>);

// Helper for creating a provisional local frame that can replace a remote
// frame.
WebLocalFrameImpl* CreateProvisional(WebRemoteFrame& old_frame,
                                     TestWebFrameClient* = nullptr);

// Helper for creating a remote frame. Generally used when creating a remote
// frame to swap into the frame tree.
// TODO(dcheng): Consider allowing security origin to be passed here once the
// frame tree moves back to core.
WebRemoteFrameImpl* CreateRemote(TestWebRemoteFrameClient* = nullptr);

// Helper for creating a local child frame of a remote parent frame.
WebLocalFrameImpl* CreateLocalChild(
    WebRemoteFrame& parent,
    const WebString& name = WebString(),
    const WebFrameOwnerProperties& = WebFrameOwnerProperties(),
    WebFrame* previous_sibling = nullptr,
    TestWebFrameClient* = nullptr,
    TestWebWidgetClient* = nullptr);

// Helper for creating a remote child frame of a remote parent frame.
WebRemoteFrameImpl* CreateRemoteChild(WebRemoteFrame& parent,
                                      const WebString& name = WebString(),
                                      scoped_refptr<SecurityOrigin> = nullptr,
                                      TestWebRemoteFrameClient* = nullptr);

class TestWebWidgetClient : public WebWidgetClient {
 public:
  ~TestWebWidgetClient() override = default;

  // WebWidgetClient:
  bool AllowsBrokenNullLayerTreeView() const override { return true; }
  WebLayerTreeView* InitializeLayerTreeView() override;

 private:
  std::unique_ptr<WebLayerTreeView> layer_tree_view_;
};

class TestWebViewWidgetClient : public TestWebWidgetClient {
 public:
  explicit TestWebViewWidgetClient(TestWebViewClient& test_web_view_client)
      : test_web_view_client_(test_web_view_client) {}
  ~TestWebViewWidgetClient() override = default;

  // TestWebViewWidgetClient:
  WebLayerTreeView* InitializeLayerTreeView() override;
  void ScheduleAnimation() override;
  void DidMeaningfulLayout(WebMeaningfulLayout) override;

 private:
  TestWebViewClient& test_web_view_client_;
};

class TestWebViewClient : public WebViewClient {
 public:
  ~TestWebViewClient() override = default;

  WebLayerTreeViewImplForTesting* GetLayerTreeViewForTesting();

  // WebViewClient:
  WebLayerTreeView* InitializeLayerTreeView() override;
  void ScheduleAnimation() override { animation_scheduled_ = true; }
  bool AnimationScheduled() { return animation_scheduled_; }
  void ClearAnimationScheduled() { animation_scheduled_ = false; }
  bool CanHandleGestureEvent() override { return true; }
  bool CanUpdateLayout() override { return true; }

 private:
  friend class TestWebViewWidgetClient;

  std::unique_ptr<WebLayerTreeViewImplForTesting> layer_tree_view_;
  bool animation_scheduled_ = false;
};

// Convenience class for handling the lifetime of a WebView and its associated
// mainframe in tests.
class WebViewHelper {
 public:
  WebViewHelper();
  ~WebViewHelper();

  // Helpers for creating the main frame. All methods that accept raw
  // pointer client arguments allow nullptr as a valid argument; if a client
  // pointer is null, the test framework will automatically create and manage
  // the lifetime of that client interface. Otherwise, the caller is responsible
  // for ensuring that non-null clients outlive the created frame.

  // Creates and initializes the WebView with a main WebLocalFrame.
  WebViewImpl* InitializeWithOpener(
      WebFrame* opener,
      TestWebFrameClient* = nullptr,
      TestWebViewClient* = nullptr,
      TestWebWidgetClient* = nullptr,
      void (*update_settings_func)(WebSettings*) = nullptr);

  // Same as InitializeWithOpener(), but always sets the opener to null.
  WebViewImpl* Initialize(TestWebFrameClient* = nullptr,
                          TestWebViewClient* = nullptr,
                          TestWebWidgetClient* = nullptr,
                          void (*update_settings_func)(WebSettings*) = nullptr);

  // Same as Initialize() but also performs the initial load of the url. Only
  // returns once the load is complete.
  WebViewImpl* InitializeAndLoad(
      const std::string& url,
      TestWebFrameClient* = nullptr,
      TestWebViewClient* = nullptr,
      TestWebWidgetClient* = nullptr,
      void (*update_settings_func)(WebSettings*) = nullptr);

  // Creates and initializes the WebView with a main WebRemoteFrame. Passing
  // nullptr as the SecurityOrigin results in a frame with a unique security
  // origin.
  WebViewImpl* InitializeRemote(TestWebRemoteFrameClient* = nullptr,
                                scoped_refptr<SecurityOrigin> = nullptr,
                                TestWebViewClient* = nullptr);

  // Load the 'Ahem' font to this WebView.
  // The 'Ahem' font is the only font whose font metrics is consistent across
  // platforms, but it's not guaranteed to be available.
  // See external/wpt/css/fonts/ahem/README for more about the 'Ahem' font.
  void LoadAhem();

  void Resize(WebSize);

  void Reset();

  WebViewImpl* GetWebView() const { return web_view_; }

  WebLocalFrameImpl* LocalMainFrame() const;
  WebRemoteFrameImpl* RemoteMainFrame() const;

  void SetViewportSize(const WebSize&);

 private:
  void InitializeWebView(TestWebViewClient*, class WebView* opener);

  WebViewImpl* web_view_;
  UseMockScrollbarSettings mock_scrollbar_settings_;
  // Non-null if the WebViewHelper owns the TestWebViewClient.
  std::unique_ptr<TestWebViewClient> owned_test_web_view_client_;
  TestWebViewClient* test_web_view_client_;

  DISALLOW_COPY_AND_ASSIGN(WebViewHelper);
};

// Minimal implementation of WebFrameClient needed for unit tests that load
// frames. Tests that load frames and need further specialization of
// WebFrameClient behavior should subclass this.
class TestWebFrameClient : public WebFrameClient {
 public:
  TestWebFrameClient();
  ~TestWebFrameClient() override = default;

  static bool IsLoading() { return loads_in_progress_ > 0; }

  WebLocalFrame* Frame() const { return frame_; }
  // Pass ownership of the TestWebFrameClient to |self_owned| here if the
  // TestWebFrameClient should delete itself on frame detach.
  void Bind(WebLocalFrame*,
            std::unique_ptr<TestWebFrameClient> self_owned = nullptr);
  // Note: only needed for local roots.
  void BindWidgetClient(std::unique_ptr<TestWebWidgetClient>);

  // WebFrameClient:
  void FrameDetached(DetachType) override;
  WebLocalFrame* CreateChildFrame(WebLocalFrame* parent,
                                  WebTreeScopeType,
                                  const WebString& name,
                                  const WebString& fallback_name,
                                  WebSandboxFlags,
                                  const ParsedFeaturePolicy&,
                                  const WebFrameOwnerProperties&) override;
  void DidStartLoading(bool) override;
  void DidStopLoading() override;
  service_manager::InterfaceProvider* GetInterfaceProvider() override {
    return interface_provider_.get();
  }
  std::unique_ptr<blink::WebURLLoaderFactory> CreateURLLoaderFactory()
      override {
    // TODO(kinuko,toyoshim): Stop using Platform's URLLoaderFactory, but create
    // its own WebURLLoaderFactoryWithMock. (crbug.com/751425)
    return Platform::Current()->CreateDefaultURLLoaderFactory();
  }

 private:
  static int loads_in_progress_;

  // If set to a non-null value, self-deletes on frame detach.
  std::unique_ptr<TestWebFrameClient> self_owned_;

  // Use service_manager::InterfaceProvider::TestApi to provide test interfaces
  // through this client.
  std::unique_ptr<service_manager::InterfaceProvider> interface_provider_;

  // This is null from when the client is created until it is initialized with
  // Bind().
  WebLocalFrame* frame_ = nullptr;

  std::unique_ptr<TestWebWidgetClient> owned_widget_client_;
};

// Minimal implementation of WebRemoteFrameClient needed for unit tests that
// load remote frames. Tests that load frames and need further specialization
// of WebFrameClient behavior should subclass this.
class TestWebRemoteFrameClient : public WebRemoteFrameClient {
 public:
  TestWebRemoteFrameClient();
  ~TestWebRemoteFrameClient() override = default;

  WebRemoteFrame* Frame() const { return frame_; }
  // Pass ownership of the TestWebFrameClient to |self_owned| here if the
  // TestWebRemoteFrameClient should delete itself on frame detach.
  void Bind(WebRemoteFrame*,
            std::unique_ptr<TestWebRemoteFrameClient> self_owned = nullptr);

  // WebRemoteFrameClient:
  void FrameDetached(DetachType) override;
  void ForwardPostMessage(WebLocalFrame* source_frame,
                          WebRemoteFrame* target_frame,
                          WebSecurityOrigin target_origin,
                          WebDOMMessageEvent) override {}

 private:
  // If set to a non-null value, self-deletes on frame detach.
  std::unique_ptr<TestWebRemoteFrameClient> self_owned_;

  // This is null from when the client is created until it is initialized with
  // Bind().
  WebRemoteFrame* frame_ = nullptr;
};

}  // namespace FrameTestHelpers
}  // namespace blink

#endif  // FrameTestHelpers_h
