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
#include "core/exported/WebViewBase.h"
#include "core/frame/Settings.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/scroll/ScrollbarTheme.h"
#include "public/platform/Platform.h"
#include "public/platform/WebCachePolicy.h"
#include "public/platform/WebMouseEvent.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURLRequest.h"
#include "public/web/WebFrameClient.h"
#include "public/web/WebFrameOwnerProperties.h"
#include "public/web/WebHistoryItem.h"
#include "public/web/WebRemoteFrameClient.h"
#include "public/web/WebSettings.h"
#include "public/web/WebViewClient.h"

namespace blink {

class WebFrame;
class WebLocalFrameBase;
class WebRemoteFrameBase;
class WebRemoteFrameImpl;
class WebSettings;

namespace FrameTestHelpers {

class TestWebFrameClient;
class TestWebRemoteFrameClient;
class TestWebWidgetClient;
class TestWebViewClient;

// Loads a url into the specified WebFrame for testing purposes. Pumps any
// pending resource requests, as well as waiting for the threaded parser to
// finish, before returning.
void LoadFrame(WebFrame*, const std::string& url);
// Same as above, but for WebLocalFrame::LoadHTMLString().
void LoadHTMLString(WebLocalFrame*,
                    const std::string& html,
                    const WebURL& base_url);
// Same as above, but for WebFrame::loadHistoryItem().
void LoadHistoryItem(WebFrame*,
                     const WebHistoryItem&,
                     WebHistoryLoadType,
                     WebCachePolicy);
// Same as above, but for WebFrame::reload().
void ReloadFrame(WebFrame*);
void ReloadFrameBypassingCache(WebFrame*);

// Pumps pending resource requests while waiting for a frame to load. Consider
// using one of the above helper methods whenever possible.
void PumpPendingRequestsForFrameToLoad(WebFrame*);

WebMouseEvent CreateMouseEvent(WebInputEvent::Type,
                               WebMouseEvent::Button,
                               const IntPoint&,
                               int modifiers);

// Helper for creating a local child frame of a local parent frame. The supplied
// TestWebFrameClient will not self-delete when the frame is detached.
WebLocalFrameBase* CreateLocalChild(WebLocalFrame* parent,
                                    WebTreeScopeType,
                                    TestWebFrameClient* = nullptr);

// Similar; however, ownership of the TestWebFrameClient is transferred to
// itself; the TestWebFrameClient will self-delete when the frame is detached.
WebLocalFrameBase* CreateLocalChild(WebLocalFrame* parent,
                                    WebTreeScopeType,
                                    std::unique_ptr<TestWebFrameClient>);

WebLocalFrameBase* CreateProvisional(TestWebFrameClient*,
                                     WebRemoteFrame* old_frame);

// Calls WebRemoteFrame::CreateLocalChild, but with some arguments prefilled
// with default test values (i.e. with a default |client| or |properties| and/or
// with a precalculated |uniqueName|).
WebLocalFrameBase* CreateLocalChild(
    WebRemoteFrame* parent,
    const WebString& name = WebString(),
    TestWebFrameClient* = nullptr,
    WebWidgetClient* = nullptr,
    WebFrame* previous_sibling = nullptr,
    const WebFrameOwnerProperties& = WebFrameOwnerProperties());
WebRemoteFrameImpl* CreateRemoteChild(WebRemoteFrame* parent,
                                      TestWebRemoteFrameClient*,
                                      const WebString& name = WebString());

// Helpers for unit tests with parameterized WebSettings overrides.
typedef void (*SettingOverrideFunction)(WebSettings*);
class SettingOverrider {
 public:
  virtual void OverrideSettings(WebSettings*) = 0;
};

// Forces to use mocked overlay scrollbars instead of the default native theme
// scrollbars to avoid crash in Chromium code when it tries to load UI
// resources that are not available when running blink unit tests, and to
// ensure consistent layout regardless of differences between scrollbar themes.
// WebViewHelper includes this, so this is only needed if a test doesn't use
// WebViewHelper or the test needs a bigger scope of mock scrollbar settings
// than the scope of WebViewHelper.
class UseMockScrollbarSettings {
 public:
  UseMockScrollbarSettings()
      : original_mock_scrollbar_enabled_(Settings::MockScrollbarsEnabled()),
        original_overlay_scrollbars_enabled_(
            RuntimeEnabledFeatures::OverlayScrollbarsEnabled()) {
    Settings::SetMockScrollbarsEnabled(true);
    RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(true);
    EXPECT_TRUE(ScrollbarTheme::GetTheme().UsesOverlayScrollbars());
  }

  UseMockScrollbarSettings(bool use_mock, bool use_overlay)
      : original_mock_scrollbar_enabled_(Settings::MockScrollbarsEnabled()),
        original_overlay_scrollbars_enabled_(
            RuntimeEnabledFeatures::OverlayScrollbarsEnabled()) {
    Settings::SetMockScrollbarsEnabled(use_mock);
    RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(use_overlay);
  }

  ~UseMockScrollbarSettings() {
    Settings::SetMockScrollbarsEnabled(original_mock_scrollbar_enabled_);
    RuntimeEnabledFeatures::SetOverlayScrollbarsEnabled(
        original_overlay_scrollbars_enabled_);
  }

 private:
  bool original_mock_scrollbar_enabled_;
  bool original_overlay_scrollbars_enabled_;
};

class TestWebWidgetClient : public WebWidgetClient {
 public:
  virtual ~TestWebWidgetClient() {}
  bool AllowsBrokenNullLayerTreeView() const override { return true; }
  WebLayerTreeView* InitializeLayerTreeView() override;

 private:
  std::unique_ptr<WebLayerTreeView> layer_tree_view_;
};

class TestWebViewWidgetClient : public TestWebWidgetClient {
 public:
  explicit TestWebViewWidgetClient(TestWebViewClient* test_web_view_client)
      : test_web_view_client_(test_web_view_client) {}
  virtual ~TestWebViewWidgetClient() {}

  WebLayerTreeView* InitializeLayerTreeView() override;
  void ScheduleAnimation() override;
  void DidMeaningfulLayout(WebMeaningfulLayout) override;

 private:
  TestWebViewClient* test_web_view_client_;
};

class TestWebViewClient : public WebViewClient {
 public:
  TestWebViewClient()
      : test_web_widget_client_(this), animation_scheduled_(false) {}
  virtual ~TestWebViewClient() {}
  WebLayerTreeView* InitializeLayerTreeView() override;

  void ScheduleAnimation() override { animation_scheduled_ = true; }
  bool AnimationScheduled() { return animation_scheduled_; }
  void ClearAnimationScheduled() { animation_scheduled_ = false; }
  bool CanHandleGestureEvent() override { return true; }
  bool CanUpdateLayout() override { return true; }

  // TODO(lfg): This is a temporary method to retrieve the WebWidgetClient,
  // while we refactor WebView to not inherit from Webwidget.
  // Returns the WebWidgetClient.
  TestWebWidgetClient* WidgetClient() { return &test_web_widget_client_; }

 private:
  friend class TestWebViewWidgetClient;

  TestWebViewWidgetClient test_web_widget_client_;
  std::unique_ptr<WebLayerTreeView> layer_tree_view_;
  bool animation_scheduled_;
};

// Convenience class for handling the lifetime of a WebView and its associated
// mainframe in tests.
class WebViewHelper {
  WTF_MAKE_NONCOPYABLE(WebViewHelper);

 public:
  WebViewHelper(SettingOverrider* = 0);
  ~WebViewHelper();

  // Creates and initializes the WebView. Implicitly calls reset() first. If
  // a WebFrameClient or a WebViewClient are passed in, they must outlive the
  // WebViewHelper.
  WebViewBase* InitializeWithOpener(
      WebFrame* opener,
      bool enable_javascript = false,
      TestWebFrameClient* = nullptr,
      TestWebViewClient* = nullptr,
      TestWebWidgetClient* = nullptr,
      void (*update_settings_func)(WebSettings*) = nullptr);

  // Same as initializeWithOpener(), but always sets the opener to null.
  WebViewBase* Initialize(bool enable_javascript = false,
                          TestWebFrameClient* = nullptr,
                          TestWebViewClient* = nullptr,
                          TestWebWidgetClient* = nullptr,
                          void (*update_settings_func)(WebSettings*) = 0);

  // Same as initialize() but also performs the initial load of the url. Only
  // returns once the load is complete.
  WebViewBase* InitializeAndLoad(
      const std::string& url,
      bool enable_javascript = false,
      TestWebFrameClient* = nullptr,
      TestWebViewClient* = nullptr,
      TestWebWidgetClient* = nullptr,
      void (*update_settings_func)(WebSettings*) = 0);

  void Resize(WebSize);

  void Reset();

  WebViewBase* WebView() const { return web_view_; }

  WebLocalFrameBase* LocalMainFrame();
  WebRemoteFrameBase* RemoteMainFrame();

 private:
  WebViewBase* web_view_;
  SettingOverrider* setting_overrider_;
  UseMockScrollbarSettings mock_scrollbar_settings_;
  // Non-null if the WebViewHelper owns the TestWebViewClient.
  std::unique_ptr<TestWebViewClient> owned_test_web_view_client_;
  TestWebViewClient* test_web_view_client_;
};

// Minimal implementation of WebFrameClient needed for unit tests that load
// frames. Tests that load frames and need further specialization of
// WebFrameClient behavior should subclass this.
class TestWebFrameClient : public WebFrameClient {
 public:
  TestWebFrameClient();

  WebLocalFrame* Frame() const { return frame_; }
  // Pass ownership of the TestWebFrameClient to |self_owned| here if the
  // TestWebFrameClient should delete itself on frame detach.
  void Bind(WebLocalFrame*,
            std::unique_ptr<TestWebFrameClient> self_owned = nullptr);

  void FrameDetached(WebLocalFrame*, DetachType) override;
  WebLocalFrame* CreateChildFrame(WebLocalFrame* parent,
                                  WebTreeScopeType,
                                  const WebString& name,
                                  const WebString& fallback_name,
                                  WebSandboxFlags,
                                  const WebParsedFeaturePolicy&,
                                  const WebFrameOwnerProperties&) override;
  void DidStartLoading(bool) override;
  void DidStopLoading() override;

  static bool IsLoading() { return loads_in_progress_ > 0; }

  // Tests can override the virtual method below to mock the interface provider.
  virtual blink::InterfaceProvider* GetInterfaceProviderForTesting() {
    return nullptr;
  }

  std::unique_ptr<blink::WebURLLoader> CreateURLLoader() override {
    // TODO(yhirano): Stop using Platform::CreateURLLoader() here.
    return Platform::Current()->CreateURLLoader();
  }

 private:
  static int loads_in_progress_;

  // If set to a non-null value, self-deletes on frame detach.
  std::unique_ptr<TestWebFrameClient> self_owned_;

  // This is null from when the client is created until it is initialized.
  WebLocalFrame* frame_ = nullptr;
};

// Minimal implementation of WebRemoteFrameClient needed for unit tests that
// load remote frames. Tests that load frames and need further specialization
// of WebFrameClient behavior should subclass this.
class TestWebRemoteFrameClient : public WebRemoteFrameClient {
 public:
  TestWebRemoteFrameClient();

  WebRemoteFrameImpl* GetFrame() const { return frame_; }

  // WebRemoteFrameClient overrides:
  void FrameDetached(DetachType) override;
  void ForwardPostMessage(WebLocalFrame* source_frame,
                          WebRemoteFrame* target_frame,
                          WebSecurityOrigin target_origin,
                          WebDOMMessageEvent) override {}

 private:
  Persistent<WebRemoteFrameImpl> const frame_;
};

}  // namespace FrameTestHelpers
}  // namespace blink

#endif  // FrameTestHelpers_h
