// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_WEB_TEST_WEB_VIEW_TEST_PROXY_H_
#define CONTENT_SHELL_RENDERER_WEB_TEST_WEB_VIEW_TEST_PROXY_H_

#include <memory>
#include <string>
#include <utility>

#include "base/callback.h"
#include "base/macros.h"
#include "build/build_config.h"
#include "content/renderer/render_view_impl.h"
#include "content/shell/renderer/web_test/accessibility_controller.h"
#include "content/shell/renderer/web_test/blink_test_runner.h"
#include "content/shell/renderer/web_test/test_runner_for_specific_view.h"
#include "content/shell/renderer/web_test/text_input_controller.h"
#include "third_party/blink/public/platform/web_drag_operation.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/platform/web_url_error.h"
#include "third_party/blink/public/platform/web_url_request.h"
#include "third_party/blink/public/web/web_dom_message_event.h"
#include "third_party/blink/public/web/web_history_commit_type.h"
#include "third_party/blink/public/web/web_navigation_policy.h"
#include "third_party/blink/public/web/web_view_client.h"
#include "third_party/blink/public/web/web_widget_client.h"

namespace blink {
class WebLocalFrame;
class WebString;
class WebView;
struct WebWindowFeatures;
}  // namespace blink

namespace content {
class AccessibilityController;
class BlinkTestRunner;
class TestInterfaces;
class TestRunner;
class TestRunnerForSpecificView;
class TextInputController;

// WebViewTestProxy is used to run web tests. This class is a partial fake
// implementation of RenderViewImpl that overrides the minimal necessary
// portions of RenderViewImpl to allow for use in web tests.
//
// This method of injecting test functionality is an outgrowth of legacy.
// In particular, classic dependency injection does not work easily
// because the RenderWidget class is too large with too much entangled
// state, making it hard to factor out creation points for injection.
//
// While implementing a fake via partial overriding of a class leads to
// a fragile base class problem and implicit coupling of the test code
// and production code, it is the most viable mechanism available without
// a huge refactor.
//
// Historically, the overridden functionality has been small enough to not
// cause too much trouble. If that changes, then this entire testing
// architecture should be revisited.
class WebViewTestProxy : public RenderViewImpl {
 public:
  explicit WebViewTestProxy(CompositorDependencies* compositor_deps,
                            const mojom::CreateViewParams& params,
                            TestInterfaces* interfaces);

  // WebViewClient implementation.
  blink::WebView* CreateView(blink::WebLocalFrame* creator,
                             const blink::WebURLRequest& request,
                             const blink::WebWindowFeatures& features,
                             const blink::WebString& frame_name,
                             blink::WebNavigationPolicy policy,
                             network::mojom::WebSandboxFlags sandbox_flags,
                             const blink::FeaturePolicy::FeatureState&,
                             const blink::SessionStorageNamespaceId&
                                 session_storage_namespace_id) override;
  void PrintPage(blink::WebLocalFrame* frame) override;
  blink::WebString AcceptLanguages() override;

  // Exposed for our TestRunner harness.
  using RenderViewImpl::ApplyPageVisibilityState;

  BlinkTestRunner* blink_test_runner() { return &blink_test_runner_; }
  TestInterfaces* test_interfaces() { return test_interfaces_; }
  AccessibilityController* accessibility_controller() {
    return &accessibility_controller_;
  }
  TestRunnerForSpecificView* view_test_runner() { return &view_test_runner_; }

  void Reset();
  void Install(blink::WebLocalFrame* frame);

 private:
  // RenderViewImpl has no public destructor.
  ~WebViewTestProxy() override;

  TestRunner* GetTestRunner();

  BlinkTestRunner blink_test_runner_{this};

  TestInterfaces* test_interfaces_ = nullptr;

  AccessibilityController accessibility_controller_{this};
  TextInputController text_input_controller_{this};
  TestRunnerForSpecificView view_test_runner_{this};

  DISALLOW_COPY_AND_ASSIGN(WebViewTestProxy);
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_WEB_TEST_WEB_VIEW_TEST_PROXY_H_
