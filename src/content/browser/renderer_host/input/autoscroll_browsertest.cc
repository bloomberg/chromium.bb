// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "build/build_config.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/shell/browser/shell.h"
#include "third_party/blink/public/platform/web_input_event.h"
#include "ui/events/base_event_utils.h"

using blink::WebInputEvent;

namespace {

const std::string kAutoscrollDataURL = R"HTML(
  <!DOCTYPE html>
  <meta name='viewport' content='width=device-width'/>
  <style>
  html, body {
    margin: 0;
  }
  .spacer { height: 10000px; }
  </style>
  <div class=spacer></div>
  <script>
    document.title='ready';
  </script>)HTML";
}  // namespace

namespace content {

class AutoscrollBrowserTest : public ContentBrowserTest {
 public:
  AutoscrollBrowserTest() {}
  ~AutoscrollBrowserTest() override {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchASCII("--enable-blink-features",
                                    "MiddleClickAutoscroll");
  }

 protected:
  RenderWidgetHostImpl* GetWidgetHost() {
    return RenderWidgetHostImpl::From(
        shell()->web_contents()->GetRenderViewHost()->GetWidget());
  }

  void LoadURL(const std::string& page_data) {
    const GURL data_url("data:text/html," + page_data);
    NavigateToURL(shell(), data_url);

    RenderWidgetHostImpl* host = GetWidgetHost();
    host->GetView()->SetSize(gfx::Size(400, 400));

    base::string16 ready_title(base::ASCIIToUTF16("ready"));
    TitleWatcher watcher(shell()->web_contents(), ready_title);
    ignore_result(watcher.WaitAndGetTitle());

    MainThreadFrameObserver main_thread_sync(host);
    main_thread_sync.Wait();
  }

  void SimulateMiddleClick(int x, int y, int modifiers) {
    // Simulate and send middle click mouse down.
    blink::WebMouseEvent down_event = SyntheticWebMouseEventBuilder::Build(
        blink::WebInputEvent::kMouseDown, x, y, modifiers);
    down_event.button = blink::WebMouseEvent::Button::kMiddle;
    down_event.SetTimeStamp(ui::EventTimeForNow());
    down_event.SetPositionInScreen(x, y);
    GetWidgetHost()->ForwardMouseEvent(down_event);

    // Simulate and send middle click mouse up.
    blink::WebMouseEvent up_event = SyntheticWebMouseEventBuilder::Build(
        blink::WebInputEvent::kMouseUp, x, y, modifiers);
    up_event.button = blink::WebMouseEvent::Button::kMiddle;
    up_event.SetTimeStamp(ui::EventTimeForNow());
    up_event.SetPositionInScreen(x, y);
    GetWidgetHost()->ForwardMouseEvent(up_event);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoscrollBrowserTest);
};

// TODO(sahel): This test is flaky https://crbug.com/838769
IN_PROC_BROWSER_TEST_F(AutoscrollBrowserTest, DISABLED_AutoscrollFling) {
  LoadURL(kAutoscrollDataURL);

  // Start autoscroll with middle click.
  auto input_msg_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kGestureScrollBegin);
  SimulateMiddleClick(10, 10, blink::WebInputEvent::kNoModifiers);
  input_msg_watcher->WaitForAck();

  // The page should start scrolling with mouse move.
  RenderFrameSubmissionObserver observer(
      GetWidgetHost()->render_frame_metadata_provider());
  blink::WebMouseEvent move_event = SyntheticWebMouseEventBuilder::Build(
      blink::WebInputEvent::kMouseMove, 30, 30,
      blink::WebInputEvent::kNoModifiers);
  move_event.SetTimeStamp(ui::EventTimeForNow());
  move_event.SetPositionInScreen(30, 30);
  GetWidgetHost()->ForwardMouseEvent(move_event);
  gfx::Vector2dF default_scroll_offset;
  while (observer.LastRenderFrameMetadata()
             .root_scroll_offset.value_or(default_scroll_offset)
             .y() <= 0) {
    observer.WaitForMetadataChange();
  }
}

#if !defined(OS_ANDROID)
#define MAYBE_WheelScrollingWorksAfterAutoscrollCancel \
  WheelScrollingWorksAfterAutoscrollCancel
#else
#define MAYBE_WheelScrollingWorksAfterAutoscrollCancel \
  DISABLED_WheelScrollingWorksAfterAutoscrollCancel
#endif
// Checks that wheel scrolling works after autoscroll cancelation.
IN_PROC_BROWSER_TEST_F(AutoscrollBrowserTest,
                       MAYBE_WheelScrollingWorksAfterAutoscrollCancel) {
  LoadURL(kAutoscrollDataURL);

  // Start autoscroll with middle click.
  auto input_msg_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kGestureScrollBegin);
  SimulateMiddleClick(10, 10, blink::WebInputEvent::kNoModifiers);
  input_msg_watcher->WaitForAck();

  // Without moving the mouse cancel the autoscroll fling with another click.
  input_msg_watcher = std::make_unique<InputMsgWatcher>(
      GetWidgetHost(), blink::WebInputEvent::kGestureScrollEnd);
  SimulateMiddleClick(10, 10, blink::WebInputEvent::kNoModifiers);
  input_msg_watcher->WaitForAck();

  // The mouse wheel scrolling must work after autoscroll cancellation.
  RenderFrameSubmissionObserver observer(
      GetWidgetHost()->render_frame_metadata_provider());
  blink::WebMouseWheelEvent wheel_event =
      SyntheticWebMouseWheelEventBuilder::Build(10, 10, 0, -53, 0, true);
  wheel_event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  GetWidgetHost()->ForwardWheelEvent(wheel_event);
  gfx::Vector2dF default_scroll_offset;
  while (observer.LastRenderFrameMetadata()
             .root_scroll_offset.value_or(default_scroll_offset)
             .y() <= 0) {
    observer.WaitForMetadataChange();
  }
}

}  // namespace content
