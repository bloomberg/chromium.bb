// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/run_loop.h"
#include "base/test/test_timeouts.h"
#include "cc/base/switches.h"
#include "content/browser/renderer_host/input/synthetic_gesture.h"
#include "content/browser/renderer_host/input/synthetic_smooth_scroll_gesture.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/input/input_event.h"
#include "content/common/input/synthetic_gesture_params.h"
#include "content/common/input/synthetic_smooth_scroll_gesture_params.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/common/input/web_input_event.h"

namespace content {

class SyntheticInputTest : public ContentBrowserTest {
 public:
  SyntheticInputTest() {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(cc::switches::kEnableGpuBenchmarking);
  }

  RenderWidgetHostImpl* GetRenderWidgetHost() const {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void LoadURL(const char* url) {
    const GURL data_url(url);
    EXPECT_TRUE(NavigateToURL(shell(), data_url));

    RenderWidgetHostImpl* host = GetRenderWidgetHost();
    HitTestRegionObserver observer(GetRenderWidgetHost()->GetFrameSinkId());
    host->GetView()->SetSize(gfx::Size(400, 400));

    base::string16 ready_title(base::ASCIIToUTF16("ready"));
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    ignore_result(watcher.WaitAndGetTitle());

    // Wait for the hit test data to be ready after initiating URL loading
    // before returning
    observer.WaitForHitTestData();
  }

  void OnSyntheticGestureCompleted(SyntheticGesture::Result result) {
    EXPECT_EQ(SyntheticGesture::GESTURE_FINISHED, result);
    runner_->Quit();
  }

 protected:
  std::unique_ptr<base::RunLoop> runner_;
};

class GestureScrollObserver : public RenderWidgetHost::InputEventObserver {
 public:
  void OnInputEvent(const blink::WebInputEvent& event) override {
    if (event.GetType() == blink::WebInputEvent::Type::kGestureScrollBegin)
      gesture_scroll_seen_ = true;
  }
  bool HasSeenGestureScrollBegin() const { return gesture_scroll_seen_; }
  bool gesture_scroll_seen_ = false;
};

// This test checks that we destroying a render widget host with an ongoing
// gesture doesn't cause lifetime issues. Namely, that the gesture
// CompletionCallback isn't destroyed before being called or the Mojo pipe
// being closed.
IN_PROC_BROWSER_TEST_F(SyntheticInputTest, DestroyWidgetWithOngoingGesture) {
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));
  WaitForLoadStop(shell()->web_contents());

  GestureScrollObserver gesture_observer;

  GetRenderWidgetHost()->AddInputEventObserver(&gesture_observer);

  // By starting a gesture, there's a Mojo callback that the renderer is
  // waiting on the browser to resolve. If the browser is shutdown before
  // ACKing the callback or closing the channel, we'll DCHECK.
  ASSERT_TRUE(ExecJs(shell()->web_contents(),
                     "chrome.gpuBenchmarking.smoothScrollBy(10000, ()=>{}, "
                     "100, 100, chrome.gpuBenchmarking.TOUCH_INPUT);"));

  while (!gesture_observer.HasSeenGestureScrollBegin()) {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), TestTimeouts::tiny_timeout());
    run_loop.Run();
  }

  shell()->Close();
}

// This test ensures that synthetic wheel scrolling works on all platforms.
IN_PROC_BROWSER_TEST_F(SyntheticInputTest, SmoothScrollWheel) {
  LoadURL(R"HTML(
    data:text/html;charset=utf-8,
    <!DOCTYPE html>
    <meta name='viewport' content='width=device-width'>
    <style>
      body {
        width: 10px;
        height: 2000px;
      }
    </style>
    <script>
      document.title = 'ready';
    </script>
  )HTML");

  SyntheticSmoothScrollGestureParams params;
  params.gesture_source_type = SyntheticGestureParams::MOUSE_INPUT;
  params.anchor = gfx::PointF(1, 1);

  // Note: 256 is precisely chosen since Android's minimum granularity is 64px.
  // All other platforms can specify the delta per-pixel.
  params.distances.push_back(gfx::Vector2d(0, -256));

  // Use a speed that's fast enough that the entire scroll occurs in a single
  // GSU, avoiding precision loss. SyntheticGestures can lose delta over time
  // in slower scrolls.
  params.speed_in_pixels_s = 10000000.f;

  // Use PrecisePixel to avoid animating.
  params.granularity = ui::ScrollGranularity::kScrollByPrecisePixel;

  runner_.reset(new base::RunLoop());

  std::unique_ptr<SyntheticSmoothScrollGesture> gesture(
      new SyntheticSmoothScrollGesture(params));
  GetRenderWidgetHost()->QueueSyntheticGesture(
      std::move(gesture),
      base::BindOnce(&SyntheticInputTest::OnSyntheticGestureCompleted,
                     base::Unretained(this)));

  // Run until we get the OnSyntheticGestureCompleted callback
  runner_->Run();
  runner_.reset();

  EXPECT_EQ(256, EvalJs(shell()->web_contents(),
                        "document.scrollingElement.scrollTop"));
}

}  // namespace content
