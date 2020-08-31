// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/platform/input/input_handler_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/test/bind_test_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/task_environment.h"
#include "base/test/trace_event_analyzer.h"
#include "build/build_config.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/trees/swap_promise_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_input_event_attribution.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_mouse_wheel_event.h"
#include "third_party/blink/public/common/input/web_pointer_event.h"
#include "third_party/blink/public/common/input/web_touch_event.h"
#include "third_party/blink/public/platform/input/input_handler_proxy.h"
#include "third_party/blink/public/platform/input/input_handler_proxy_client.h"
#include "third_party/blink/renderer/platform/widget/input/compositor_thread_event_queue.h"
#include "third_party/blink/renderer/platform/widget/input/event_with_callback.h"
#include "third_party/blink/renderer/platform/widget/input/scroll_predictor.h"
#include "ui/events/types/scroll_input_type.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/latency/latency_info.h"

using cc::InputHandler;
using cc::ScrollBeginThreadState;
using cc::TouchAction;
using testing::_;
using testing::DoAll;
using testing::Field;
using testing::Mock;
using testing::Return;
using testing::SetArgPointee;

namespace blink {
namespace test {

namespace {

enum InputHandlerProxyTestType {
  ROOT_SCROLL_NORMAL_HANDLER,
  ROOT_SCROLL_SYNCHRONOUS_HANDLER,
  CHILD_SCROLL_NORMAL_HANDLER,
  CHILD_SCROLL_SYNCHRONOUS_HANDLER,
};
static const InputHandlerProxyTestType test_types[] = {
    ROOT_SCROLL_NORMAL_HANDLER,
    ROOT_SCROLL_SYNCHRONOUS_HANDLER,
    CHILD_SCROLL_NORMAL_HANDLER,
    CHILD_SCROLL_SYNCHRONOUS_HANDLER,
};

MATCHER_P(WheelEventsMatch, expected, "") {
  return WheelEventsMatch(arg, expected);
}

std::unique_ptr<WebInputEvent> CreateGestureScrollPinch(
    WebInputEvent::Type type,
    WebGestureDevice source_device,
    base::TimeTicks event_time,
    float delta_y_or_scale = 0,
    int x = 0,
    int y = 0) {
  WebGestureEvent gesture(type, WebInputEvent::kNoModifiers, event_time,
                          source_device);
  if (type == WebInputEvent::Type::kGestureScrollUpdate) {
    gesture.data.scroll_update.delta_y = delta_y_or_scale;
  } else if (type == WebInputEvent::Type::kGesturePinchUpdate) {
    gesture.data.pinch_update.scale = delta_y_or_scale;
    gesture.SetPositionInWidget(gfx::PointF(x, y));
  }
  return gesture.Clone();
}

class MockInputHandler : public cc::InputHandler {
 public:
  MockInputHandler() {}
  ~MockInputHandler() override {}

  MOCK_METHOD0(PinchGestureBegin, void());
  MOCK_METHOD2(PinchGestureUpdate,
               void(float magnify_delta, const gfx::Point& anchor));
  MOCK_METHOD2(PinchGestureEnd, void(const gfx::Point& anchor, bool snap));

  MOCK_METHOD0(SetNeedsAnimateInput, void());

  MOCK_METHOD2(ScrollBegin,
               ScrollStatus(cc::ScrollState*, ui::ScrollInputType type));
  MOCK_METHOD2(RootScrollBegin,
               ScrollStatus(cc::ScrollState*, ui::ScrollInputType type));
  MOCK_METHOD2(ScrollUpdate,
               cc::InputHandlerScrollResult(cc::ScrollState*, base::TimeDelta));
  MOCK_METHOD1(ScrollEnd, void(bool));
  MOCK_METHOD2(RecordScrollBegin,
               void(ui::ScrollInputType type,
                    cc::ScrollBeginThreadState state));
  MOCK_METHOD1(RecordScrollEnd, void(ui::ScrollInputType type));
  MOCK_METHOD0(ScrollingShouldSwitchtoMainThread, bool());
  void NotifyInputEvent() override {}

  std::unique_ptr<cc::SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) override {
    return nullptr;
  }

  std::unique_ptr<cc::EventsMetricsManager::ScopedMonitor>
  GetScopedEventMetricsMonitor(
      std::unique_ptr<cc::EventMetrics> event_metrics) override {
    return nullptr;
  }

  cc::ScrollElasticityHelper* CreateScrollElasticityHelper() override {
    return nullptr;
  }
  bool GetScrollOffsetForLayer(cc::ElementId element_id,
                               gfx::ScrollOffset* offset) override {
    return false;
  }
  bool ScrollLayerTo(cc::ElementId element_id,
                     const gfx::ScrollOffset& offset) override {
    return false;
  }

  void BindToClient(cc::InputHandlerClient* client) override {}

  cc::InputHandlerPointerResult MouseDown(const gfx::PointF& mouse_position,
                                          const bool shift_modifier) override {
    cc::InputHandlerPointerResult pointer_result;
    pointer_result.type = cc::kScrollbarScroll;
    pointer_result.scroll_offset = gfx::ScrollOffset(0, 1);
    return pointer_result;
  }

  cc::InputHandlerPointerResult MouseUp(
      const gfx::PointF& mouse_position) override {
    cc::InputHandlerPointerResult pointer_result;
    pointer_result.type = cc::kScrollbarScroll;
    return pointer_result;
  }

  void MouseLeave() override {}

  cc::ElementId FindFrameElementIdAtPoint(
      const gfx::PointF& mouse_position) override {
    return cc::ElementId();
  }

  cc::InputHandlerPointerResult MouseMoveAt(
      const gfx::Point& mouse_position) override {
    return cc::InputHandlerPointerResult();
  }

  MOCK_CONST_METHOD1(
      GetEventListenerProperties,
      cc::EventListenerProperties(cc::EventListenerClass event_class));
  MOCK_METHOD2(EventListenerTypeForTouchStartOrMoveAt,
               cc::InputHandler::TouchStartOrMoveEventListenerType(
                   const gfx::Point& point,
                   cc::TouchAction* touch_action));
  MOCK_CONST_METHOD1(HasBlockingWheelEventHandlerAt, bool(const gfx::Point&));

  MOCK_METHOD0(RequestUpdateForSynchronousInputHandler, void());
  MOCK_METHOD1(SetSynchronousInputHandlerRootScrollOffset,
               void(const gfx::ScrollOffset& root_offset));

  bool IsCurrentlyScrollingViewport() const override {
    return is_scrolling_root_;
  }
  void set_is_scrolling_root(bool is) { is_scrolling_root_ = is; }

  MOCK_METHOD3(GetSnapFlingInfoAndSetAnimatingSnapTarget,
               bool(const gfx::Vector2dF& natural_displacement,
                    gfx::Vector2dF* initial_offset,
                    gfx::Vector2dF* target_offset));
  MOCK_METHOD1(ScrollEndForSnapFling, void(bool));

 private:
  bool is_scrolling_root_ = true;
  DISALLOW_COPY_AND_ASSIGN(MockInputHandler);
};

class MockSynchronousInputHandler : public SynchronousInputHandler {
 public:
  MOCK_METHOD6(UpdateRootLayerState,
               void(const gfx::ScrollOffset& total_scroll_offset,
                    const gfx::ScrollOffset& max_scroll_offset,
                    const gfx::SizeF& scrollable_size,
                    float page_scale_factor,
                    float min_page_scale_factor,
                    float max_page_scale_factor));
};

class MockInputHandlerProxyClient : public InputHandlerProxyClient {
 public:
  MockInputHandlerProxyClient() {}
  ~MockInputHandlerProxyClient() override {}

  void WillShutdown() override {}

  MOCK_METHOD1(DispatchNonBlockingEventToMainThread_,
               void(const WebInputEvent&));

  MOCK_METHOD2(GenerateScrollBeginAndSendToMainThread,
               void(const WebGestureEvent& update_event,
                    const WebInputEventAttribution&));

  void DispatchNonBlockingEventToMainThread(
      std::unique_ptr<WebInputEvent> event,
      const ui::LatencyInfo& latency_info,
      const WebInputEventAttribution&) override {
    CHECK(event.get());
    DispatchNonBlockingEventToMainThread_(*event.get());
  }

  MOCK_METHOD5(DidOverscroll,
               void(const gfx::Vector2dF& accumulated_overscroll,
                    const gfx::Vector2dF& latest_overscroll_delta,
                    const gfx::Vector2dF& current_fling_velocity,
                    const gfx::PointF& causal_event_viewport_point,
                    const cc::OverscrollBehavior& overscroll_behavior));
  void DidAnimateForInput() override {}
  void DidStartScrollingViewport() override {}
  MOCK_METHOD3(SetWhiteListedTouchAction,
               void(cc::TouchAction touch_action,
                    uint32_t unique_touch_event_id,
                    InputHandlerProxy::EventDisposition event_disposition));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputHandlerProxyClient);
};

class MockInputHandlerProxyClientWithDidAnimateForInput
    : public MockInputHandlerProxyClient {
 public:
  MockInputHandlerProxyClientWithDidAnimateForInput() {}
  ~MockInputHandlerProxyClientWithDidAnimateForInput() override {}

  MOCK_METHOD0(DidAnimateForInput, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockInputHandlerProxyClientWithDidAnimateForInput);
};

WebTouchPoint CreateWebTouchPoint(WebTouchPoint::State state,
                                  float x,
                                  float y) {
  WebTouchPoint point;
  point.state = state;
  point.SetPositionInScreen(x, y);
  point.SetPositionInWidget(x, y);
  return point;
}

const cc::InputHandler::ScrollStatus kImplThreadScrollState(
    cc::InputHandler::SCROLL_ON_IMPL_THREAD,
    cc::MainThreadScrollingReason::kNotScrollingOnMain);

const cc::InputHandler::ScrollStatus kMainThreadScrollState(
    cc::InputHandler::SCROLL_ON_MAIN_THREAD,
    cc::MainThreadScrollingReason::kHandlingScrollFromMainThread);

const cc::InputHandler::ScrollStatus kScrollIgnoredScrollState(
    cc::InputHandler::SCROLL_IGNORED,
    cc::MainThreadScrollingReason::kNotScrollable);

}  // namespace

class TestInputHandlerProxy : public InputHandlerProxy {
 public:
  TestInputHandlerProxy(cc::InputHandler* input_handler,
                        InputHandlerProxyClient* client,
                        bool force_input_to_main_thread)
      : InputHandlerProxy(input_handler, client, force_input_to_main_thread) {}
  void RecordMainThreadScrollingReasonsForTest(WebGestureDevice device,
                                               uint32_t reasons) {
    RecordMainThreadScrollingReasons(device, reasons);
  }

  MOCK_METHOD0(SetNeedsAnimateInput, void());

  EventDisposition HitTestTouchEventForTest(
      const WebTouchEvent& touch_event,
      bool* is_touching_scrolling_layer,
      cc::TouchAction* white_listed_touch_action) {
    return HitTestTouchEvent(touch_event, is_touching_scrolling_layer,
                             white_listed_touch_action);
  }

  EventDisposition HandleMouseWheelForTest(
      const WebMouseWheelEvent& wheel_event) {
    return HandleMouseWheel(wheel_event);
  }

  // This is needed because the tests can't directly call
  // DispatchQueuedInputEvents since it is private.
  void DispatchQueuedInputEventsHelper() { DispatchQueuedInputEvents(); }
};

class InputHandlerProxyTest
    : public testing::Test,
      public testing::WithParamInterface<InputHandlerProxyTestType> {
 public:
  InputHandlerProxyTest()
      : synchronous_root_scroll_(GetParam() == ROOT_SCROLL_SYNCHRONOUS_HANDLER),
        install_synchronous_handler_(
            GetParam() == ROOT_SCROLL_SYNCHRONOUS_HANDLER ||
            GetParam() == CHILD_SCROLL_SYNCHRONOUS_HANDLER),
        expected_disposition_(InputHandlerProxy::DID_HANDLE) {
    input_handler_ = std::make_unique<TestInputHandlerProxy>(
        &mock_input_handler_, &mock_client_,
        /*force_input_to_main_thread=*/false);
    scroll_result_did_scroll_.did_scroll = true;
    scroll_result_did_not_scroll_.did_scroll = false;

    if (install_synchronous_handler_) {
      EXPECT_CALL(mock_input_handler_,
                  RequestUpdateForSynchronousInputHandler())
          .Times(1);
      input_handler_->SetSynchronousInputHandler(
          &mock_synchronous_input_handler_);
    }

    mock_input_handler_.set_is_scrolling_root(synchronous_root_scroll_);

    // Set a default device so tests don't always have to set this.
    gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  }

  virtual ~InputHandlerProxyTest() = default;

// This is defined as a macro so the line numbers can be traced back to the
// correct spot when it fails.
#define EXPECT_SET_NEEDS_ANIMATE_INPUT(times)                              \
  do {                                                                     \
    EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(times); \
  } while (false)

// This is defined as a macro because when an expectation is not satisfied the
// only output you get out of gmock is the line number that set the expectation.
#define VERIFY_AND_RESET_MOCKS()                                     \
  do {                                                               \
    testing::Mock::VerifyAndClearExpectations(&mock_input_handler_); \
    testing::Mock::VerifyAndClearExpectations(                       \
        &mock_synchronous_input_handler_);                           \
    testing::Mock::VerifyAndClearExpectations(&mock_client_);        \
  } while (false)

  void Animate(base::TimeTicks time) { input_handler_->Animate(time); }

  void SetSmoothScrollEnabled(bool value) {}

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 protected:
  void GestureScrollStarted();
  void ScrollHandlingSwitchedToMainThread();
  void GestureScrollIgnored();
  void FlingAndSnap();

  const bool synchronous_root_scroll_;
  const bool install_synchronous_handler_;
  testing::StrictMock<MockInputHandler> mock_input_handler_;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler_;
  std::unique_ptr<TestInputHandlerProxy> input_handler_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;
  WebGestureEvent gesture_;
  InputHandlerProxy::EventDisposition expected_disposition_;
  base::HistogramTester histogram_tester_;
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  cc::InputHandlerScrollResult scroll_result_did_not_scroll_;
};

// The helper basically returns the EventDisposition that is returned by
// RouteToTypeSpecificHandler. This is done by passing in a callback when
// calling HandleInputEventWithLatencyInfo. By design, DispatchSingleInputEvent
// will then call this callback with the disposition returned by
// RouteToTypeSpecificHandler and that is what gets returned by this helper.
InputHandlerProxy::EventDisposition HandleInputEventWithLatencyInfo(
    TestInputHandlerProxy* input_handler,
    const WebInputEvent& event) {
  std::unique_ptr<WebInputEvent> scoped_input_event(event.Clone());
  InputHandlerProxy::EventDisposition event_disposition =
      InputHandlerProxy::DID_NOT_HANDLE;
  input_handler->HandleInputEventWithLatencyInfo(
      std::move(scoped_input_event), ui::LatencyInfo(),
      base::BindLambdaForTesting(
          [&event_disposition](
              InputHandlerProxy::EventDisposition disposition,
              std::unique_ptr<WebInputEvent> event,
              const ui::LatencyInfo& latency_info,
              std::unique_ptr<InputHandlerProxy::DidOverscrollParams> callback,
              const WebInputEventAttribution& attribution) {
            event_disposition = disposition;
          }));
  return event_disposition;
}

// This helper forces the CompositorThreadEventQueue to be flushed.
InputHandlerProxy::EventDisposition HandleInputEventAndFlushEventQueue(
    testing::StrictMock<MockInputHandler>& mock_input_handler,
    TestInputHandlerProxy* input_handler,
    const WebInputEvent& event) {
  EXPECT_CALL(mock_input_handler, SetNeedsAnimateInput())
      .Times(testing::AnyNumber());
  std::unique_ptr<WebInputEvent> scoped_input_event(event.Clone());
  InputHandlerProxy::EventDisposition event_disposition =
      InputHandlerProxy::DID_NOT_HANDLE;
  input_handler->HandleInputEventWithLatencyInfo(
      std::move(scoped_input_event), ui::LatencyInfo(),
      base::BindLambdaForTesting(
          [&event_disposition](
              InputHandlerProxy::EventDisposition disposition,
              std::unique_ptr<WebInputEvent> event,
              const ui::LatencyInfo& latency_info,
              std::unique_ptr<InputHandlerProxy::DidOverscrollParams> callback,
              const WebInputEventAttribution& attribution) {
            event_disposition = disposition;
          }));

  input_handler->DispatchQueuedInputEventsHelper();
  return event_disposition;
}

class InputHandlerProxyEventQueueTest : public testing::Test {
 public:
  InputHandlerProxyEventQueueTest()
      : input_handler_proxy_(&mock_input_handler_,
                             &mock_client_,
                             /*force_input_to_main_thread=*/false) {
    if (input_handler_proxy_.compositor_event_queue_) {
      input_handler_proxy_.compositor_event_queue_ =
          std::make_unique<CompositorThreadEventQueue>();
    }
    SetScrollPredictionEnabled(true);
  }

  ~InputHandlerProxyEventQueueTest() = default;

  void HandleGestureEvent(WebInputEvent::Type type,
                          float delta_y_or_scale = 0,
                          int x = 0,
                          int y = 0) {
    HandleGestureEventWithSourceDevice(type, WebGestureDevice::kTouchscreen,
                                       delta_y_or_scale, x, y);
  }

  void HandleGestureEventWithSourceDevice(WebInputEvent::Type type,
                                          WebGestureDevice source_device,
                                          float delta_y_or_scale = 0,
                                          int x = 0,
                                          int y = 0) {
    InjectInputEvent(CreateGestureScrollPinch(
        type, source_device, input_handler_proxy_.tick_clock_->NowTicks(),
        delta_y_or_scale, x, y));
  }

  void InjectInputEvent(std::unique_ptr<WebInputEvent> event) {
    ui::LatencyInfo latency;
    input_handler_proxy_.HandleInputEventWithLatencyInfo(
        std::move(event), latency,
        base::BindOnce(
            &InputHandlerProxyEventQueueTest::DidHandleInputEventAndOverscroll,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void HandleMouseEvent(WebInputEvent::Type type, int x = 0, int y = 0) {
    WebMouseEvent mouse_event(type, WebInputEvent::kNoModifiers,
                              WebInputEvent::GetStaticTimeStampForTests());

    mouse_event.SetPositionInWidget(gfx::PointF(x, y));
    mouse_event.button = WebMouseEvent::Button::kLeft;
    HandleInputEventWithLatencyInfo(&input_handler_proxy_, mouse_event);
  }

  void DidHandleInputEventAndOverscroll(
      InputHandlerProxy::EventDisposition event_disposition,
      std::unique_ptr<WebInputEvent> input_event,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<InputHandlerProxy::DidOverscrollParams> overscroll_params,
      const WebInputEventAttribution& attribution) {
    event_disposition_recorder_.push_back(event_disposition);
    latency_info_recorder_.push_back(latency_info);
  }

  base::circular_deque<std::unique_ptr<EventWithCallback>>& event_queue() {
    return input_handler_proxy_.compositor_event_queue_->queue_;
  }

  void SetInputHandlerProxyTickClockForTesting(
      const base::TickClock* tick_clock) {
    input_handler_proxy_.SetTickClockForTesting(tick_clock);
  }

  void DeliverInputForBeginFrame() {
    constexpr base::TimeDelta interval = base::TimeDelta::FromMilliseconds(16);
    base::TimeTicks frame_time =
        WebInputEvent::GetStaticTimeStampForTests() +
        (next_begin_frame_number_ - viz::BeginFrameArgs::kStartingFrameNumber) *
            interval;
    input_handler_proxy_.DeliverInputForBeginFrame(viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, frame_time,
        frame_time + interval, interval, viz::BeginFrameArgs::NORMAL));
  }

  void DeliverInputForHighLatencyMode() {
    input_handler_proxy_.DeliverInputForHighLatencyMode();
  }

  void SetScrollPredictionEnabled(bool enabled) {
    input_handler_proxy_.scroll_predictor_ =
        enabled ? std::make_unique<ScrollPredictor>() : nullptr;
  }

  std::unique_ptr<InputPredictor::InputData>
  GestureScrollEventPredictionAvailable() {
    return input_handler_proxy_.scroll_predictor_->predictor_
        ->GeneratePrediction(WebInputEvent::GetStaticTimeStampForTests());
  }

  base::TimeTicks NowTimestampForEvents() {
    return input_handler_proxy_.tick_clock_->NowTicks();
  }

 protected:
  testing::StrictMock<MockInputHandler> mock_input_handler_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;
  TestInputHandlerProxy input_handler_proxy_;
  std::vector<InputHandlerProxy::EventDisposition> event_disposition_recorder_;
  std::vector<ui::LatencyInfo> latency_info_recorder_;

  uint64_t next_begin_frame_number_ = viz::BeginFrameArgs::kStartingFrameNumber;

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::WeakPtrFactory<InputHandlerProxyEventQueueTest> weak_ptr_factory_{this};
};

// Tests that changing source devices mid gesture scroll is handled gracefully.
// For example, when a touch scroll is in progress and the user initiates a
// scrollbar scroll before the touch scroll has had a chance to dispatch a GSE.
TEST_P(InputHandlerProxyTest, NestedGestureBasedScrolls) {
  // Touchpad initiates a scroll.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kWheel,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  // Before ScrollEnd for touchpad is fired, user starts a thumb drag. This is
  // expected to immediately end the touchpad scroll.
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(ui::ScrollInputType::kWheel))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  // TODO(crbug.com/1060708): This should be called once.
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kScrollbar,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(0, 20));
  mouse_event.button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), mouse_event));

  VERIFY_AND_RESET_MOCKS();

  // Touchpad GSE comes in while a scrollbar drag is in progress. This is
  // expected to be dropped because a scrollbar scroll is currently active.
  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(ui::ScrollInputType::kWheel))
      .Times(1);
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  VERIFY_AND_RESET_MOCKS();

  // The GSE from the scrollbar needs to be handled.
  EXPECT_CALL(mock_input_handler_,
              RecordScrollEnd(ui::ScrollInputType::kScrollbar))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(1);
  mouse_event.SetType(WebInputEvent::Type::kMouseUp);
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), mouse_event));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelNoListener) {
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelBlockingListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(true));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelBlockingAndPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(true));
  // We will not call GetEventListenerProperties because we early out when we
  // hit blocking region.
  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelEventOutsideBlockingListener) {
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Gt(10))))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Le(10))))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kBlocking));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  wheel.SetPositionInScreen(0, 5);
  wheel.SetPositionInWidget(0, 5);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest,
       MouseWheelEventOutsideBlockingListenerWithPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Gt(10))))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(mock_input_handler_,
              HasBlockingWheelEventHandlerAt(
                  testing::Property(&gfx::Point::y, testing::Le(10))))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillRepeatedly(
          testing::Return(cc::EventListenerProperties::kBlockingAndPassive));

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  wheel.SetPositionInScreen(0, 5);
  wheel.SetPositionInWidget(0, 5);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), wheel));
  VERIFY_AND_RESET_MOCKS();
}

// Tests that changing source devices when an animated scroll is in progress
// ends the current scroll offset animation and ensures that a new one gets
// created.
TEST_P(InputHandlerProxyTest, ScrollbarScrollEndOnDeviceChange) {
  // A scrollbar scroll begins.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kScrollbar,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  WebMouseEvent mouse_event(WebInputEvent::Type::kMouseDown,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  mouse_event.SetPositionInWidget(gfx::PointF(0, 20));
  mouse_event.button = WebMouseEvent::Button::kLeft;
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), mouse_event));

  EXPECT_EQ(input_handler_->currently_active_gesture_device(),
            WebGestureDevice::kScrollbar);
  VERIFY_AND_RESET_MOCKS();

  // A mousewheel tick takes place before the scrollbar scroll ends.
  EXPECT_CALL(mock_input_handler_,
              RecordScrollEnd(ui::ScrollInputType::kScrollbar))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(ui::ScrollInputType::kWheel,
                        cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  EXPECT_EQ(input_handler_->currently_active_gesture_device(),
            WebGestureDevice::kTouchpad);

  VERIFY_AND_RESET_MOCKS();

  // Mousewheel GSE is then fired and the mousewheel scroll ends.
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(ui::ScrollInputType::kWheel))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Mouse up gets ignored as the scrollbar scroll already ended before the
  // mousewheel tick took place.
  EXPECT_CALL(mock_input_handler_,
              RecordScrollEnd(ui::ScrollInputType::kScrollbar))
      .Times(1);
  mouse_event.SetType(WebInputEvent::Type::kMouseUp);
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), mouse_event));

  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::GestureScrollStarted() {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  // The event should not be marked as handled if scrolling is not possible.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  // Mark the event as handled if scroll happens.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}
TEST_P(InputHandlerProxyTest, GestureScrollStarted) {
  GestureScrollStarted();
}

TEST_P(InputHandlerProxyTest, GestureScrollOnMainThread) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kMainThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 40;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollIgnored) {
  // We shouldn't handle the GestureScrollBegin.
  // Instead, we should get a DROP_EVENT result, indicating
  // that we could determine that there's nothing that could scroll or otherwise
  // react to this gesture sequence and thus we should drop the whole gesture
  // sequence on the floor, except for the ScrollEnd.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kScrollIgnoredScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  // GSB is dropped and not sent to the main thread, GSE shouldn't get sent to
  // the main thread, either.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollByPage) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  gesture_.data.scroll_begin.delta_hint_units =
      ui::ScrollGranularity::kScrollByPage;
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 1;
  gesture_.data.scroll_update.delta_units =
      ui::ScrollGranularity::kScrollByPage;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollBeginThatTargetViewport) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, RootScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  gesture_.data.scroll_begin.target_viewport = true;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::FlingAndSnap() {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  // The event should be dropped if InputHandler decides to snap.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  gesture_.data.scroll_update.inertial_phase =
      WebGestureEvent::InertialPhaseState::kMomentum;
  EXPECT_CALL(mock_input_handler_,
              GetSnapFlingInfoAndSetAnimatingSnapTarget(_, _, _))
      .WillOnce(DoAll(testing::SetArgPointee<1>(gfx::Vector2dF(0, 0)),
                      testing::SetArgPointee<2>(gfx::Vector2dF(0, 100)),
                      testing::Return(true)));
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, SnapFlingIgnoresFollowingGSUAndGSE) {
  FlingAndSnap();
  // The next GestureScrollUpdate should also be ignored, and will not ask for
  // snap position.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;

  EXPECT_CALL(mock_input_handler_,
              GetSnapFlingInfoAndSetAnimatingSnapTarget(_, _, _))
      .Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  VERIFY_AND_RESET_MOCKS();

  // The GestureScrollEnd should also be ignored.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.data.scroll_end.inertial_phase =
      WebGestureEvent::InertialPhaseState::kMomentum;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(0);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GesturePinch) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchBegin);
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 1.5;
  gesture_.SetPositionInWidget(gfx::PointF(7, 13));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.5, gfx::Point(7, 13)));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 0.5;
  gesture_.SetPositionInWidget(gfx::PointF(9, 6));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(.5, gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchEnd);
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(9, 6), true));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GesturePinchAfterScrollOnMainThread) {
  // Scrolls will start by being sent to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kMainThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 40;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  // However, after the pinch gesture starts, they should go to the impl
  // thread.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchBegin);
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 1.5;
  gesture_.SetPositionInWidget(gfx::PointF(7, 13));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.5, gfx::Point(7, 13)));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 0.5;
  gesture_.SetPositionInWidget(gfx::PointF(9, 6));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(.5, gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGesturePinchEnd);
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(9, 6), true));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  // After the pinch gesture ends, they should go to back to the main
  // thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::ScrollHandlingSwitchedToMainThread() {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  // HandleGestureScrollBegin will set gesture_scroll_on_impl_thread_.
  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = -40;
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();

  // The scroll handling switches to the main thread, a GSB is sent to the main
  // thread to initiate the hit testing and compute the scroll chain.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(0);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client_, GenerateScrollBeginAndSendToMainThread(_, _));
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(mock_input_handler_,
                                               input_handler_.get(), gesture_));

  VERIFY_AND_RESET_MOCKS();
}
TEST_P(InputHandlerProxyTest, WheelScrollHandlingSwitchedToMainThread) {
  gesture_.SetSourceDevice(WebGestureDevice::kTouchpad);
  ScrollHandlingSwitchedToMainThread();
}
TEST_P(InputHandlerProxyTest, TouchScrollHandlingSwitchedToMainThread) {
  gesture_.SetSourceDevice(WebGestureDevice::kTouchscreen);
  ScrollHandlingSwitchedToMainThread();
}

TEST_P(InputHandlerProxyTest,
       GestureScrollOnImplThreadFlagClearedAfterScrollEnd) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  gesture_.SetType(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  // |gesture_scroll_on_impl_thread_| should be false once a GestureScrollEnd
  // gets handled.
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest,
       BeginScrollWhenGestureScrollOnImplThreadFlagIsSet) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);

  gesture_.SetType(WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, HitTestTouchEventNonNullTouchAction) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Eq(0)), _))
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kMax;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Gt(0)), _))
      .WillOnce(
          testing::Invoke([](const gfx::Point&, cc::TouchAction* touch_action) {
            *touch_action = cc::TouchAction::kPanUp;
            return cc::InputHandler::TouchStartOrMoveEventListenerType::
                HANDLER_ON_SCROLLING_LAYER;
          }));
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);

  bool is_touching_scrolling_layer;
  cc::TouchAction white_listed_touch_action = cc::TouchAction::kAuto;
  EXPECT_EQ(expected_disposition_, input_handler_->HitTestTouchEventForTest(
                                       touch, &is_touching_scrolling_layer,
                                       &white_listed_touch_action));
  EXPECT_TRUE(is_touching_scrolling_layer);
  EXPECT_EQ(white_listed_touch_action, cc::TouchAction::kPanUp);
  VERIFY_AND_RESET_MOCKS();
}

// Tests that the whitelisted touch action is correctly set when a touch is
// made non blocking due to an ongoing fling. https://crbug.com/1048098.
TEST_F(InputHandlerProxyEventQueueTest, AckTouchActionNonBlockingForFling) {
  // Simulate starting a compositor scroll and then flinging. This is setup for
  // the real checks below.
  {
    float delta = 10;

    // ScrollBegin
    {
      EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
          .WillOnce(Return(kImplThreadScrollState));
      EXPECT_CALL(
          mock_input_handler_,
          RecordScrollBegin(_, ScrollBeginThreadState::kScrollingOnCompositor))
          .Times(1);

      HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin, delta);
      Mock::VerifyAndClearExpectations(&mock_input_handler_);
    }

    // ScrollUpdate
    {
      EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
      EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
          .WillOnce(Return(false));
      EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _)).Times(1);

      HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, delta);
      DeliverInputForBeginFrame();
      Mock::VerifyAndClearExpectations(&mock_input_handler_);
    }

    // Start a fling - ScrollUpdate with momentum
    {
      cc::InputHandlerScrollResult scroll_result_did_scroll;
      scroll_result_did_scroll.did_scroll = true;
      EXPECT_CALL(mock_input_handler_, ScrollUpdate(_, _))
          .WillOnce(Return(scroll_result_did_scroll));
      EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
      EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
          .WillOnce(Return(false));
      EXPECT_CALL(mock_input_handler_,
                  GetSnapFlingInfoAndSetAnimatingSnapTarget(_, _, _))
          .WillOnce(Return(false));

      auto gsu_fling = CreateGestureScrollPinch(
          WebInputEvent::Type::kGestureScrollUpdate,
          WebGestureDevice::kTouchscreen, NowTimestampForEvents(), delta,
          /*x=*/0, /*y=*/0);
      static_cast<WebGestureEvent*>(gsu_fling.get())
          ->data.scroll_update.inertial_phase =
          WebGestureEvent::InertialPhaseState::kMomentum;
      InjectInputEvent(std::move(gsu_fling));
      DeliverInputForBeginFrame();
    }
  }

  // We're now in an active gesture fling. Simulate the user touching down on
  // the screen. If this touch hits a blocking region (e.g. touch-action or a
  // non-passive touchstart listener), we won't actually treat it as blocking;
  // because of the ongoing fling it will be treated as non blocking. However,
  // we also have to ensure that the whitelisted_touch_action reported is also
  // kAuto so that the browser knows that it shouldn't wait for an ACK with an
  // allowed touch-action before dispatching more scrolls.
  {
    // Simulate hitting a blocking region on the scrolling layer, as if there
    // was a non-passive touchstart handler.
    EXPECT_CALL(mock_input_handler_,
                EventListenerTypeForTouchStartOrMoveAt(_, _))
        .WillOnce(DoAll(SetArgPointee<1>(TouchAction::kNone),
                        Return(InputHandler::TouchStartOrMoveEventListenerType::
                                   HANDLER_ON_SCROLLING_LAYER)));

    std::unique_ptr<WebTouchEvent> touch_start =
        std::make_unique<WebTouchEvent>(
            WebInputEvent::Type::kTouchStart, WebInputEvent::kNoModifiers,
            WebInputEvent::GetStaticTimeStampForTests());
    touch_start->touches_length = 1;
    touch_start->touch_start_or_first_touch_move = true;
    touch_start->touches[0] =
        CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);

    // This is the call this test is checking: we expect that the client will
    // report the touch as non-blocking and also that the whitelisted touch
    // action matches the non blocking expectatithe whitelisted touch action
    // matches the non blocking expectation (i.e. all touches are allowed).
    EXPECT_CALL(
        mock_client_,
        SetWhiteListedTouchAction(
            TouchAction::kAuto, touch_start->unique_touch_event_id,
            InputHandlerProxy::DID_NOT_HANDLE_NON_BLOCKING_DUE_TO_FLING))
        .WillOnce(Return());

    InjectInputEvent(std::move(touch_start));
  }
}

TEST_P(InputHandlerProxyTest, HitTestTouchEventNullTouchAction) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Eq(0)), _))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Gt(0)), _))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              HANDLER_ON_SCROLLING_LAYER));
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::Type::kTouchMove,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 3;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);

  bool is_touching_scrolling_layer;
  cc::TouchAction* white_listed_touch_action = nullptr;
  EXPECT_EQ(expected_disposition_, input_handler_->HitTestTouchEventForTest(
                                       touch, &is_touching_scrolling_layer,
                                       white_listed_touch_action));
  EXPECT_TRUE(is_touching_scrolling_layer);
  EXPECT_TRUE(!white_listed_touch_action);
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestNegative) {
  // None of the three touch points fall in the touch region. So the event
  // should be dropped.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchEndOrCancel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .Times(2)
      .WillRepeatedly(testing::Invoke([](const gfx::Point&,
                                         cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kPanUp;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(cc::TouchAction::kPanUp, 1,
                                        InputHandlerProxy::DROP_EVENT))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStateStationary, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestPositive) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Eq(0)), _))
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kAuto;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Gt(0)), _))
      .WillOnce(
          testing::Invoke([](const gfx::Point&, cc::TouchAction* touch_action) {
            *touch_action = cc::TouchAction::kPanY;
            return cc::InputHandler::TouchStartOrMoveEventListenerType::
                HANDLER_ON_SCROLLING_LAYER;
          }));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(cc::TouchAction::kPanY, 1,
                                                      expected_disposition_))
      .WillOnce(testing::Return());
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestPassivePositive) {
  // One of the touch points is not on a touch-region. So the event should be
  // sent to the impl thread.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .Times(3)
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kPanRight;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }))
      .WillRepeatedly(testing::Invoke([](const gfx::Point&,
                                         cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kPanX;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(
                                cc::TouchAction::kPanRight, 1,
                                InputHandlerProxy::DID_HANDLE_NON_BLOCKING))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touches[1] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  touch.touches[2] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, TouchStartPassiveAndTouchEndBlocking) {
  // The touch start is not in a touch-region but there is a touch end handler
  // so to maintain targeting we need to dispatch the touch start as
  // non-blocking but drop all touch moves.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchEndOrCancel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kBlocking));
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::TouchAction::kNone;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(
                                cc::TouchAction::kNone, 1,
                                InputHandlerProxy::DID_HANDLE_NON_BLOCKING))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  touch.unique_touch_event_id = 1;
  touch.touches_length = 1;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  touch.touch_start_or_first_touch_move = true;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  touch.SetType(WebInputEvent::Type::kTouchMove);
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = false;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, TouchMoveBlockingAddedAfterPassiveTouchStart) {
  // The touch start is not in a touch-region but there is a touch end handler
  // so to maintain targeting we need to dispatch the touch start as
  // non-blocking but drop all touch moves.
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(_, _, _))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE_NON_BLOCKING,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));

  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::HANDLER));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(_, _, _))
      .WillOnce(testing::Return());

  touch.SetType(WebInputEvent::Type::kTouchMove);
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStateMoved, 10, 10);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE_NON_BLOCKING,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch));
  VERIFY_AND_RESET_MOCKS();
}

TEST(SynchronousInputHandlerProxyTest, StartupShutdown) {
  testing::StrictMock<MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  InputHandlerProxy proxy(&mock_input_handler, &mock_client, false);

  // When adding a SynchronousInputHandler, immediately request an
  // UpdateRootLayerStateForSynchronousInputHandler() call.
  EXPECT_CALL(mock_input_handler, RequestUpdateForSynchronousInputHandler())
      .Times(1);
  proxy.SetSynchronousInputHandler(&mock_synchronous_input_handler);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);

  EXPECT_CALL(mock_input_handler, RequestUpdateForSynchronousInputHandler())
      .Times(0);
  proxy.SetSynchronousInputHandler(nullptr);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST(SynchronousInputHandlerProxyTest, UpdateRootLayerState) {
  testing::NiceMock<MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  InputHandlerProxy proxy(&mock_input_handler, &mock_client, false);

  proxy.SetSynchronousInputHandler(&mock_synchronous_input_handler);

  // When adding a SynchronousInputHandler, immediately request an
  // UpdateRootLayerStateForSynchronousInputHandler() call.
  EXPECT_CALL(
      mock_synchronous_input_handler,
      UpdateRootLayerState(gfx::ScrollOffset(1, 2), gfx::ScrollOffset(3, 4),
                           gfx::SizeF(5, 6), 7, 8, 9))
      .Times(1);
  proxy.UpdateRootLayerStateForSynchronousInputHandler(
      gfx::ScrollOffset(1, 2), gfx::ScrollOffset(3, 4), gfx::SizeF(5, 6), 7, 8,
      9);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST(SynchronousInputHandlerProxyTest, SetOffset) {
  testing::NiceMock<MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  InputHandlerProxy proxy(&mock_input_handler, &mock_client, false);

  proxy.SetSynchronousInputHandler(&mock_synchronous_input_handler);

  EXPECT_CALL(mock_input_handler, SetSynchronousInputHandlerRootScrollOffset(
                                      gfx::ScrollOffset(5, 6)));
  proxy.SynchronouslySetRootScrollOffset(gfx::ScrollOffset(5, 6));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST_F(InputHandlerProxyEventQueueTest,
       MouseEventOnScrollbarInitiatesGestureScroll) {
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  // Test mousedown on the scrollbar. Expect to get GSB and GSU.
  HandleMouseEvent(WebInputEvent::Type::kMouseDown);
  EXPECT_EQ(2ul, event_queue().size());
  EXPECT_EQ(event_queue()[0]->event().GetType(),
            WebInputEvent::Type::kGestureScrollBegin);
  EXPECT_EQ(event_queue()[1]->event().GetType(),
            WebInputEvent::Type::kGestureScrollUpdate);

  // Test mouseup on the scrollbar. Expect to get GSE.
  HandleMouseEvent(WebInputEvent::Type::kMouseUp);
  EXPECT_EQ(3ul, event_queue().size());
  EXPECT_EQ(event_queue()[2]->event().GetType(),
            WebInputEvent::Type::kGestureScrollEnd);
}

TEST_F(InputHandlerProxyEventQueueTest, VSyncAlignedGestureScroll) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);

  // GestureScrollBegin will be processed immediately.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[0]);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);

  // GestureScrollUpdate will be queued.
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(-20,
            static_cast<const WebGestureEvent&>(event_queue().front()->event())
                .data.scroll_update.delta_y);
  EXPECT_EQ(1ul, event_queue().front()->coalesced_count());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -40);

  // GestureScrollUpdate will be coalesced.
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(-60,
            static_cast<const WebGestureEvent&>(event_queue().front()->event())
                .data.scroll_update.delta_y);
  EXPECT_EQ(2ul, event_queue().front()->coalesced_count());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(0);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // GestureScrollEnd will be queued.
  EXPECT_EQ(2ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));

  // Dispatch all queued events.
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  DeliverInputForBeginFrame();
  EXPECT_EQ(0ul, event_queue().size());
  // Should run callbacks for every original events.
  EXPECT_EQ(4ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[1]);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[2]);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[3]);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

#if defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
// Flaky under sanitizers and in other "slow" bot configs:
// https://crbug.com/1029250
#define MAYBE_VSyncAlignedGestureScrollPinchScroll \
  DISABLED_VSyncAlignedGestureScrollPinchScroll
#else
#define MAYBE_VSyncAlignedGestureScrollPinchScroll \
  VSyncAlignedGestureScrollPinchScroll
#endif

TEST_F(InputHandlerProxyEventQueueTest,
       MAYBE_VSyncAlignedGestureScrollPinchScroll) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  // Start scroll in the first frame.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);

  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Continue scroll in the second frame, pinch, then start another scroll.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true)).Times(2);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  // Two |GesturePinchUpdate| will be coalesced.
  EXPECT_CALL(mock_input_handler_,
              PinchGestureUpdate(0.7f, gfx::Point(13, 17)));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(), false));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(2);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 1.4f, 13, 17);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 0.5f, 13, 17);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -70);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -5);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  EXPECT_EQ(8ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(12ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, VSyncAlignedQueueingTime) {
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(10));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(40));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -40);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(20));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(10));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // Dispatch all queued events.
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(70));
  DeliverInputForBeginFrame();
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, VSyncAlignedCoalesceScrollAndPinch) {
  // Start scroll in the first frame.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  // GSUs and GPUs in one sequence should be coalesced into 1 GSU and 1 GPU.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -7);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 2.0f, 13, 10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -6);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 0.2f, 2, 20);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 0.25f, 3, 30);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // Only the first GSB was dispatched.
  EXPECT_EQ(11ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  EXPECT_EQ(WebInputEvent::Type::kGesturePinchBegin,
            event_queue()[0]->event().GetType());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            event_queue()[1]->event().GetType());
  EXPECT_EQ(-35, static_cast<const WebGestureEvent&>(event_queue()[1]->event())
                     .data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            event_queue()[2]->event().GetType());
  EXPECT_EQ(2.0f, static_cast<const WebGestureEvent&>(event_queue()[2]->event())
                      .data.pinch_update.scale);
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchEnd,
            event_queue()[3]->event().GetType());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            event_queue()[4]->event().GetType());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollBegin,
            event_queue()[5]->event().GetType());
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchBegin,
            event_queue()[6]->event().GetType());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            event_queue()[7]->event().GetType());
  EXPECT_EQ(-85, static_cast<const WebGestureEvent&>(event_queue()[7]->event())
                     .data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            event_queue()[8]->event().GetType());
  EXPECT_EQ(0.5f, static_cast<const WebGestureEvent&>(event_queue()[8]->event())
                      .data.pinch_update.scale);
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchEnd,
            event_queue()[9]->event().GetType());
  EXPECT_EQ(WebInputEvent::Type::kGestureScrollEnd,
            event_queue()[10]->event().GetType());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, VSyncAlignedCoalesceTouchpadPinch) {
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput());

  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchBegin,
                                     WebGestureDevice::kTouchpad);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchUpdate,
                                     WebGestureDevice::kTouchpad, 1.1f, 10, 20);
  // The second update should coalesce with the first.
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchUpdate,
                                     WebGestureDevice::kTouchpad, 1.1f, 10, 20);
  // The third update has a different anchor so it should not be coalesced.
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchUpdate,
                                     WebGestureDevice::kTouchpad, 1.1f, 11, 21);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGesturePinchEnd,
                                     WebGestureDevice::kTouchpad);

  // Only the PinchBegin was dispatched.
  EXPECT_EQ(3ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  ASSERT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            event_queue()[0]->event().GetType());
  EXPECT_FLOAT_EQ(1.21f,
                  static_cast<const WebGestureEvent&>(event_queue()[0]->event())
                      .data.pinch_update.scale);
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchUpdate,
            event_queue()[1]->event().GetType());
  EXPECT_EQ(WebInputEvent::Type::kGesturePinchEnd,
            event_queue()[2]->event().GetType());
}

TEST_F(InputHandlerProxyEventQueueTest, OriginalEventsTracing) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(2);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true))
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(2);

  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(_, _));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(_, _));

  trace_analyzer::Start("*");
  // Simulate scroll.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // Simulate scroll and pinch.
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 2.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  // Dispatch all events.
  DeliverInputForBeginFrame();

  // Retrieve tracing data.
  auto analyzer = trace_analyzer::Stop();
  trace_analyzer::TraceEventVector begin_events;
  trace_analyzer::Query begin_query = trace_analyzer::Query::EventPhaseIs(
      TRACE_EVENT_PHASE_NESTABLE_ASYNC_BEGIN);
  analyzer->FindEvents(begin_query, &begin_events);

  trace_analyzer::TraceEventVector end_events;
  trace_analyzer::Query end_query =
      trace_analyzer::Query::EventPhaseIs(TRACE_EVENT_PHASE_NESTABLE_ASYNC_END);
  analyzer->FindEvents(end_query, &end_events);

  EXPECT_EQ(7ul, begin_events.size());
  EXPECT_EQ(7ul, end_events.size());
  EXPECT_EQ(static_cast<int>(WebInputEvent::Type::kGestureScrollUpdate),
            end_events[0]->GetKnownArgAsInt("type"));
  EXPECT_EQ(3, end_events[0]->GetKnownArgAsInt("coalesced_count"));
  EXPECT_EQ(static_cast<int>(WebInputEvent::Type::kGestureScrollEnd),
            end_events[1]->GetKnownArgAsInt("type"));

  EXPECT_EQ(static_cast<int>(WebInputEvent::Type::kGestureScrollBegin),
            end_events[2]->GetKnownArgAsInt("type"));
  EXPECT_EQ(static_cast<int>(WebInputEvent::Type::kGesturePinchBegin),
            end_events[3]->GetKnownArgAsInt("type"));
  // Original scroll and pinch updates will be stored in the coalesced
  // PinchUpdate of the <ScrollUpdate, PinchUpdate> pair.
  // The ScrollUpdate of the pair doesn't carry original events and won't be
  // traced.
  EXPECT_EQ(static_cast<int>(WebInputEvent::Type::kGesturePinchUpdate),
            end_events[4]->GetKnownArgAsInt("type"));
  EXPECT_EQ(4, end_events[4]->GetKnownArgAsInt("coalesced_count"));
  EXPECT_EQ(static_cast<int>(WebInputEvent::Type::kGesturePinchEnd),
            end_events[5]->GetKnownArgAsInt("type"));
  EXPECT_EQ(static_cast<int>(WebInputEvent::Type::kGestureScrollEnd),
            end_events[6]->GetKnownArgAsInt("type"));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, TouchpadGestureScrollEndFlushQueue) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(2);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true))
      .Times(::testing::AtLeast(1));

  // Simulate scroll.
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollBegin,
                                     WebGestureDevice::kTouchpad);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollUpdate,
                                     WebGestureDevice::kTouchpad, -20);

  // Both GSB and the first GSU will be dispatched immediately since the first
  // GSU has blocking wheel event source.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  // The rest of the GSU events will get queued since they have non-blocking
  // wheel event source.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(::testing::AtLeast(1));
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollUpdate,
                                     WebGestureDevice::kTouchpad, -20);
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  // Touchpad GSE will flush the queue.
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollEnd,
                                     WebGestureDevice::kTouchpad);

  EXPECT_EQ(0ul, event_queue().size());
  // GSB, GSU(with blocking wheel source), GSU(with non-blocking wheel
  // source), and GSE are the sent events.
  EXPECT_EQ(4ul, event_disposition_recorder_.size());

  EXPECT_FALSE(
      input_handler_proxy_.gesture_scroll_on_impl_thread_for_testing());

  // Starting a new scroll sequence should have the same behavior (namely that
  // the first scroll update is not queued but immediately dispatched).
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollBegin,
                                     WebGestureDevice::kTouchpad);
  HandleGestureEventWithSourceDevice(WebInputEvent::Type::kGestureScrollUpdate,
                                     WebGestureDevice::kTouchpad, -20);

  // Both GSB and the first GSU must be dispatched immediately since the first
  // GSU has blocking wheel event source.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(6ul, event_disposition_recorder_.size());
}

TEST_F(InputHandlerProxyEventQueueTest, CoalescedLatencyInfo) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);
  DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  // Should run callbacks for every original events.
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  EXPECT_EQ(5ul, latency_info_recorder_.size());
  EXPECT_EQ(false, latency_info_recorder_[1].coalesced());
  // Coalesced events should have latency set to coalesced.
  EXPECT_EQ(true, latency_info_recorder_[2].coalesced());
  EXPECT_EQ(true, latency_info_recorder_[3].coalesced());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, CoalescedEventSwitchToMainThread) {
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  cc::InputHandlerScrollResult scroll_result_did_not_scroll_;
  scroll_result_did_scroll_.did_scroll = true;
  scroll_result_did_not_scroll_.did_scroll = false;

  // scroll begin on main thread
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kMainThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(2);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  DeliverInputForBeginFrame();
  EXPECT_EQ(3ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            event_disposition_recorder_.back());
  // GSU should not be coalesced
  EXPECT_EQ(false, latency_info_recorder_[1].coalesced());
  EXPECT_EQ(false, latency_info_recorder_[2].coalesced());

  // pinch start, handle scroll and pinch on compositor.
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(_, _));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(_, _));

  HandleGestureEvent(WebInputEvent::Type::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  EXPECT_EQ(2ul, event_queue().size());
  DeliverInputForBeginFrame();

  EXPECT_EQ(7ul, event_disposition_recorder_.size());
  EXPECT_EQ(false, latency_info_recorder_[4].coalesced());
  // Coalesced events should have latency set to coalesced.
  EXPECT_EQ(true, latency_info_recorder_[5].coalesced());
  EXPECT_EQ(true, latency_info_recorder_[6].coalesced());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_.back());

  // Pinch end, handle scroll on main thread.
  HandleGestureEvent(WebInputEvent::Type::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -30);
  DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  // Should run callbacks for every original events.
  EXPECT_EQ(10ul, event_disposition_recorder_.size());
  EXPECT_EQ(10ul, latency_info_recorder_.size());
  // Latency should not be set to coalesced when send to main thread
  EXPECT_EQ(false, latency_info_recorder_[8].coalesced());
  EXPECT_EQ(false, latency_info_recorder_[9].coalesced());
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            event_disposition_recorder_.back());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_F(InputHandlerProxyEventQueueTest, ScrollPredictorTest) {
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(2);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillOnce(testing::Return(scroll_result_did_scroll_));

  // No prediction when start with a GSB
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(8));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  DeliverInputForBeginFrame();
  EXPECT_FALSE(GestureScrollEventPredictionAvailable());

  // Test predictor returns last GSU delta.
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(8));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(8));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -15);
  DeliverInputForBeginFrame();
  auto result = GestureScrollEventPredictionAvailable();
  EXPECT_TRUE(result);
  EXPECT_NE(0, result->pos.y());
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Predictor has been reset after a new GSB.
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(_)).Times(1);
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  tick_clock.Advance(base::TimeDelta::FromMilliseconds(8));
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  DeliverInputForBeginFrame();
  EXPECT_FALSE(GestureScrollEventPredictionAvailable());
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollEnd);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

// Test deliver input w/o prediction enabled.
TEST_F(InputHandlerProxyEventQueueTest, DeliverInputWithHighLatencyMode) {
  SetScrollPredictionEnabled(false);

  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;
  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(2);
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  DeliverInputForBeginFrame();
  // 3 queued event be delivered.
  EXPECT_EQ(3ul, event_disposition_recorder_.size());
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_.back());

  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::Type::kGestureScrollUpdate, -10);
  DeliverInputForHighLatencyMode();
  // 2 queued event be delivered.
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_.back());

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

class InputHandlerProxyMainThreadScrollingReasonTest
    : public InputHandlerProxyTest {
 public:
  enum TestEventType {
    Touch,
    MouseWheel,
  };

  InputHandlerProxyMainThreadScrollingReasonTest() : InputHandlerProxyTest() {}
  ~InputHandlerProxyMainThreadScrollingReasonTest() = default;

  void SetupEvents(TestEventType type) {
    touch_start_ = WebTouchEvent(WebInputEvent::Type::kTouchStart,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
    touch_end_ = WebTouchEvent(WebInputEvent::Type::kTouchEnd,
                               WebInputEvent::kNoModifiers,
                               WebInputEvent::GetStaticTimeStampForTests());
    wheel_event_ = WebMouseWheelEvent(
        WebInputEvent::Type::kMouseWheel, WebInputEvent::kControlKey,
        WebInputEvent::GetStaticTimeStampForTests());
    gesture_scroll_begin_ = WebGestureEvent(
        WebInputEvent::Type::kGestureScrollBegin, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        type == TestEventType::MouseWheel ? WebGestureDevice::kTouchpad
                                          : WebGestureDevice::kTouchscreen);
    gesture_scroll_end_ = WebGestureEvent(
        WebInputEvent::Type::kGestureScrollEnd, WebInputEvent::kNoModifiers,
        WebInputEvent::GetStaticTimeStampForTests(),
        type == TestEventType::MouseWheel ? WebGestureDevice::kTouchpad
                                          : WebGestureDevice::kTouchscreen);
    touch_start_.touches_length = 1;
    touch_start_.touch_start_or_first_touch_move = true;
    touch_start_.touches[0] =
        CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 10, 10);

    touch_end_.touches_length = 1;
  }

  base::HistogramBase::Sample GetBucketSample(uint32_t reason) {
    if (reason == cc::MainThreadScrollingReason::kNotScrollingOnMain)
      return 0;

    uint32_t bucket = 1;
    while ((reason = reason >> 1))
      bucket++;
    return bucket;
  }

 protected:
  WebTouchEvent touch_start_;
  WebTouchEvent touch_end_;
  WebMouseWheelEvent wheel_event_;
  WebGestureEvent gesture_scroll_begin_;
  WebGestureEvent gesture_scroll_end_;
};

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollNotScrollOnMain) {
  // Touch start with passive event listener.
  SetupEvents(TestEventType::Touch);

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Gt(0)), _))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(_, _, _))
      .WillOnce(testing::Return());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(
      expected_disposition_,
      HandleInputEventAndFlushEventQueue(
          mock_input_handler_, input_handler_.get(), gesture_scroll_begin_));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Renderer4.MainThreadGestureScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(cc::MainThreadScrollingReason::kNotScrollingOnMain),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(
      expected_disposition_,
      HandleInputEventAndFlushEventQueue(
          mock_input_handler_, input_handler_.get(), gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollTouchEventHandlerRegion) {
  // The touch event hits a touch event handler that is acked from the
  // compositor thread.
  SetupEvents(TestEventType::Touch);

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Gt(0)), _))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              HANDLER_ON_SCROLLING_LAYER));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(_, _, _))
      .WillOnce(testing::Return());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventAndFlushEventQueue(
                mock_input_handler_, input_handler_.get(), touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(
      expected_disposition_,
      HandleInputEventAndFlushEventQueue(
          mock_input_handler_, input_handler_.get(), gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Renderer4.MainThreadGestureScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(cc::MainThreadScrollingReason::kNotScrollingOnMain),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(
      expected_disposition_,
      HandleInputEventAndFlushEventQueue(
          mock_input_handler_, input_handler_.get(), gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollTouchEventHandlerRegionAndHandlingScrollFromMainThread) {
  // The touch event hits a touch event handler and should block on main thread.
  // Since ScrollBegin doesn't allow the gesture to scroll on impl. We report
  // TouchEventHandler reason as well as HandlingScrollFromMainThread. Since we
  // do not collect HandlingScrollFromMainThread when there are other reasons
  // present, TouchEventHandler is the only reason being collected in the
  // histogram.
  SetupEvents(TestEventType::Touch);

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Gt(0)), _))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              HANDLER_ON_SCROLLING_LAYER));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(_, _, _))
      .WillOnce(testing::Return());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_, HandleInputEventWithLatencyInfo(
                                       input_handler_.get(), touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kMainThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Renderer4.MainThreadGestureScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(
              cc::MainThreadScrollingReason::kHandlingScrollFromMainThread),
          1)));

  // Handle touch end event so that input handler proxy is out of the state of
  // DID_NOT_HANDLE.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_end_));

  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(), touch_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       GestureScrollHandlingScrollFromMainThread) {
  // Gesture scrolling on main thread. We only record
  // HandlingScrollFromMainThread when it's the only available reason.
  SetupEvents(TestEventType::Touch);
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(
                  testing::Property(&gfx::Point::x, testing::Gt(0)), _))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(_, _, _))
      .WillOnce(testing::Return());
  EXPECT_CALL(mock_input_handler_, GetEventListenerProperties(_))
      .WillRepeatedly(testing::Return(cc::EventListenerProperties::kPassive));

  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_, HandleInputEventWithLatencyInfo(
                                       input_handler_.get(), touch_start_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kMainThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "Renderer4.MainThreadGestureScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(
              cc::MainThreadScrollingReason::kHandlingScrollFromMainThread),
          1)));

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest, WheelScrollHistogram) {
  // Firstly check if input handler can correctly record main thread scrolling
  // reasons.
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);
  input_handler_->RecordMainThreadScrollingReasonsForTest(
      WebGestureDevice::kTouchpad,
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects |
          cc::MainThreadScrollingReason::kThreadedScrollingDisabled |
          cc::MainThreadScrollingReason::kFrameOverlay |
          cc::MainThreadScrollingReason::kHandlingScrollFromMainThread);

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(
          base::Bucket(
              GetBucketSample(cc::MainThreadScrollingReason::
                                  kHasBackgroundAttachmentFixedObjects),
              1),
          base::Bucket(
              GetBucketSample(
                  cc::MainThreadScrollingReason::kThreadedScrollingDisabled),
              1),
          base::Bucket(
              GetBucketSample(cc::MainThreadScrollingReason::kFrameOverlay),
              1)));

  // We only want to record "Handling scroll from main thread" reason if it's
  // the only reason. If it's not the only reason, the "real" reason for
  // scrolling on main is something else, and we only want to pay attention to
  // that reason. So we should only include this reason in the histogram when
  // its on its own.
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);
  input_handler_->RecordMainThreadScrollingReasonsForTest(
      WebGestureDevice::kTouchpad,
      cc::MainThreadScrollingReason::kHandlingScrollFromMainThread);

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(
          base::Bucket(
              GetBucketSample(cc::MainThreadScrollingReason::
                                  kHasBackgroundAttachmentFixedObjects),
              1),
          base::Bucket(
              GetBucketSample(
                  cc::MainThreadScrollingReason::kThreadedScrollingDisabled),
              1),
          base::Bucket(
              GetBucketSample(cc::MainThreadScrollingReason::kFrameOverlay), 1),
          base::Bucket(
              GetBucketSample(
                  cc::MainThreadScrollingReason::kHandlingScrollFromMainThread),
              1)));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollNotScrollingOnMain) {
  // Even if a scroller is composited, we still need to record its main thread
  // scrolling reason if it is blocked on a main thread event handler.
  SetupEvents(TestEventType::MouseWheel);

  // We can scroll on impl for an wheel event with passive event listener.
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_, HandleInputEventWithLatencyInfo(
                                       input_handler_.get(), wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnCompositor))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(cc::MainThreadScrollingReason::kNotScrollingOnMain),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollWheelEventHandlerRegion) {
  // Wheel event with blocking event listener. If there is a wheel event handler
  // at the point, we do not need to call GetEventListenerProperties since it
  // indicates kBlocking.
  SetupEvents(TestEventType::MouseWheel);
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_, HandleInputEventWithLatencyInfo(
                                       input_handler_.get(), wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(
          _, cc::ScrollBeginThreadState::kScrollingOnCompositorBlockedOnMain))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(
              cc::MainThreadScrollingReason::kWheelEventHandlerRegion),
          1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(true));
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollWheelEventHandlerRegionAndHandlingScrollFromMainThread) {
  // Wheel scrolling on main thread. Because we also block scrolling with wheel
  // event handler, we should record that reason as well.
  SetupEvents(TestEventType::MouseWheel);
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_, HandleInputEventWithLatencyInfo(
                                       input_handler_.get(), wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kMainThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(
              cc::MainThreadScrollingReason::kWheelEventHandlerRegion),
          1)));

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_end_));
}

TEST_P(InputHandlerProxyMainThreadScrollingReasonTest,
       WheelScrollHandlingScrollFromMainThread) {
  // Gesture scrolling on main thread. We only record
  // HandlingScrollFromMainThread when it's the only available reason.
  SetupEvents(TestEventType::MouseWheel);
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_))
      .WillRepeatedly(testing::Return(false));
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  EXPECT_EQ(expected_disposition_, HandleInputEventWithLatencyInfo(
                                       input_handler_.get(), wheel_event_));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(_, _))
      .WillOnce(testing::Return(kMainThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      RecordScrollBegin(_, cc::ScrollBeginThreadState::kScrollingOnMain))
      .Times(1);
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_begin_));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(
          GetBucketSample(
              cc::MainThreadScrollingReason::kHandlingScrollFromMainThread),
          1)));

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_, RecordScrollEnd(_)).Times(1);
  EXPECT_EQ(expected_disposition_,
            HandleInputEventWithLatencyInfo(input_handler_.get(),
                                            gesture_scroll_end_));
}

// Tests that turning on the force_input_to_main_thread flag causes all events
// to be return DID_NOT_HANDLE for forwarding to the main thread.
class InputHandlerProxyForceHandlingOnMainThread : public testing::Test {
 public:
  InputHandlerProxyForceHandlingOnMainThread()
      : input_handler_proxy_(&mock_input_handler_,
                             &mock_client_,
                             /*force_input_to_main_thread=*/true) {}

  ~InputHandlerProxyForceHandlingOnMainThread() = default;

 protected:
  testing::StrictMock<MockInputHandler> mock_input_handler_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;
  TestInputHandlerProxy input_handler_proxy_;
};

TEST_F(InputHandlerProxyForceHandlingOnMainThread, MouseWheel) {
  // We shouldn't be checking the status of event handlers at all.
  EXPECT_CALL(mock_input_handler_, HasBlockingWheelEventHandlerAt(_)).Times(0);
  EXPECT_CALL(mock_input_handler_, GetEventListenerProperties(_)).Times(0);

  WebMouseWheelEvent wheel(WebInputEvent::Type::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  // The input event must return DID_NOT_HANDLE, indicating it should be
  // handled on the main thread.
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventWithLatencyInfo(&input_handler_proxy_, wheel));
}

TEST_F(InputHandlerProxyForceHandlingOnMainThread, TouchEvents) {
  // Shouldn't query event listener state at all since we're forcing events to
  // the main thread.
  EXPECT_CALL(mock_input_handler_, EventListenerTypeForTouchStartOrMoveAt(_, _))
      .Times(0);

  WebTouchEvent touch(WebInputEvent::Type::kTouchStart,
                      WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);

  // The input event must return DID_NOT_HANDLE, indicating it should be
  // handled on the main thread.
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventWithLatencyInfo(&input_handler_proxy_, touch));

  WebTouchEvent touch_move(WebInputEvent::Type::kTouchMove,
                           WebInputEvent::kNoModifiers,
                           WebInputEvent::GetStaticTimeStampForTests());

  touch_move.touches_length = 1;
  touch_move.touch_start_or_first_touch_move = true;
  touch_move.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::State::kStatePressed, 0, 0);

  // The input event must return DID_NOT_HANDLE, indicating it should be
  // handled on the main thread.
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventWithLatencyInfo(&input_handler_proxy_, touch_move));

  touch_move.touch_start_or_first_touch_move = false;

  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventWithLatencyInfo(&input_handler_proxy_, touch_move));
}

TEST_F(InputHandlerProxyForceHandlingOnMainThread, GestureEvents) {
  WebGestureEvent gesture(WebInputEvent::Type::kGestureScrollBegin,
                          WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests(),
                          WebGestureDevice::kTouchscreen);

  // The input event must return DID_NOT_HANDLE, indicating it should be
  // handled on the main thread.
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventWithLatencyInfo(&input_handler_proxy_, gesture));
  gesture.SetType(WebInputEvent::Type::kGestureScrollUpdate);
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventWithLatencyInfo(&input_handler_proxy_, gesture));
  gesture.SetType(WebInputEvent::Type::kGestureScrollEnd);
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            HandleInputEventWithLatencyInfo(&input_handler_proxy_, gesture));
}

class InputHandlerProxyMomentumScrollJankTest : public testing::Test {
 public:
  InputHandlerProxyMomentumScrollJankTest()
      : input_handler_proxy_(&mock_input_handler_,
                             &mock_client_,
                             /*force_input_to_main_thread=*/false) {
    tick_clock_.SetNowTicks(base::TimeTicks::Now());
    // Disable scroll predictor for this test.
    input_handler_proxy_.scroll_predictor_ = nullptr;
    input_handler_proxy_.SetTickClockForTesting(&tick_clock_);
  }

  ~InputHandlerProxyMomentumScrollJankTest() = default;

  void HandleScrollBegin() {
    WebGestureEvent gesture(WebInputEvent::Type::kGestureScrollBegin,
                            WebInputEvent::kNoModifiers, tick_clock_.NowTicks(),
                            WebGestureDevice::kTouchscreen);
    HandleGesture(gesture.Clone());
  }

  void HandleScrollEnd() {
    WebGestureEvent gesture(WebInputEvent::Type::kGestureScrollEnd,
                            WebInputEvent::kNoModifiers, tick_clock_.NowTicks(),
                            WebGestureDevice::kTouchscreen);
    HandleGesture(gesture.Clone());
  }

  void HandleScrollUpdate(bool is_momentum) {
    WebGestureEvent gesture(WebInputEvent::Type::kGestureScrollUpdate,
                            WebInputEvent::kNoModifiers, tick_clock_.NowTicks(),
                            WebGestureDevice::kTouchscreen);
    gesture.data.scroll_update.delta_y = -20;
    if (is_momentum) {
      gesture.data.scroll_update.inertial_phase =
          WebGestureEvent::InertialPhaseState::kMomentum;
    }
    HandleGesture(gesture.Clone());
  }

  void AdvanceClock(uint32_t milliseconds) {
    tick_clock_.Advance(base::TimeDelta::FromMilliseconds(milliseconds));
  }

  void AddNonJankyEvents(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
      AdvanceClock(16);
      HandleScrollUpdate(true /* is_momentum */);
      DeliverInputForBeginFrame();
    }
  }

  void DeliverInputForBeginFrame() {
    constexpr base::TimeDelta interval = base::TimeDelta::FromMilliseconds(16);
    base::TimeTicks frame_time =
        base::TimeTicks() +
        (next_begin_frame_number_ - viz::BeginFrameArgs::kStartingFrameNumber) *
            interval;
    input_handler_proxy_.DeliverInputForBeginFrame(viz::BeginFrameArgs::Create(
        BEGINFRAME_FROM_HERE, 0, next_begin_frame_number_++, frame_time,
        frame_time + interval, interval, viz::BeginFrameArgs::NORMAL));
  }

 protected:
  void HandleGesture(std::unique_ptr<WebInputEvent> event) {
    ui::LatencyInfo latency;
    input_handler_proxy_.HandleInputEventWithLatencyInfo(
        std::move(event), latency, base::DoNothing());
  }

  uint64_t next_begin_frame_number_ = viz::BeginFrameArgs::kStartingFrameNumber;

  testing::NiceMock<MockInputHandler> mock_input_handler_;
  testing::NiceMock<MockInputHandlerProxyClient> mock_client_;
  TestInputHandlerProxy input_handler_proxy_;
  base::SimpleTestTickClock tick_clock_;
};

TEST_F(InputHandlerProxyMomentumScrollJankTest, TestJank) {
  cc::InputHandlerScrollResult scroll_result_did_scroll;
  scroll_result_did_scroll.did_scroll = true;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll));

  base::HistogramTester histogram_tester;
  HandleScrollBegin();

  // Flush one update, the first update is always ignored.
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  DeliverInputForBeginFrame();

  // Enqueue three updates, they will be coalesced and count as two janks.
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  // Enqueue two updates, they will be coalesced and count as one jank
  // (https://crbug.com/952930).
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  // Enqueue two updates, they will be coalesced and count as one
  // jank(https://crbug.com/952930).
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  // Add 93 non-janky events, bringing us to a total of 100 events.
  AddNonJankyEvents(93);

  HandleScrollEnd();
  DeliverInputForBeginFrame();

  histogram_tester.ExpectUniqueSample("Renderer4.MomentumScrollJankPercentage",
                                      4, 1);
}

TEST_F(InputHandlerProxyMomentumScrollJankTest, TestJankMultipleGestures) {
  cc::InputHandlerScrollResult scroll_result_did_scroll;
  scroll_result_did_scroll.did_scroll = true;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll));

  base::HistogramTester histogram_tester;

  for (int i = 0; i < 3; ++i) {
    HandleScrollBegin();

    // Flush one update, the first update is always ignored.
    AdvanceClock(16);
    HandleScrollUpdate(true /* is_momentum */);
    DeliverInputForBeginFrame();

    // Enqueue two updates, they will be coalesced and count as one jank
    // (https://crbug.com/952930).
    AdvanceClock(16);
    HandleScrollUpdate(true /* is_momentum */);
    AdvanceClock(16);
    HandleScrollUpdate(true /* is_momentum */);
    AdvanceClock(1);
    DeliverInputForBeginFrame();

    // Add 98 non-janky events, bringing us to a total of 100 events.
    AddNonJankyEvents(98);

    HandleScrollEnd();
    DeliverInputForBeginFrame();

    histogram_tester.ExpectUniqueSample(
        "Renderer4.MomentumScrollJankPercentage", 1, i + 1);
  }
}

TEST_F(InputHandlerProxyMomentumScrollJankTest, TestJankRounding) {
  cc::InputHandlerScrollResult scroll_result_did_scroll;
  scroll_result_did_scroll.did_scroll = true;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll));

  base::HistogramTester histogram_tester;

  HandleScrollBegin();

  // Flush one update, the first update is always ignored.
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  DeliverInputForBeginFrame();

  // Enqueue two updates, they will be coalesced and count as one jank
  // (https://crbug.com/952930).
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  // Add 500 non-janky events. Even with this many events, our round-up logic
  // should cause us to report 1% jank.
  AddNonJankyEvents(500);

  HandleScrollEnd();
  DeliverInputForBeginFrame();

  histogram_tester.ExpectUniqueSample("Renderer4.MomentumScrollJankPercentage",
                                      1, 1);
}

TEST_F(InputHandlerProxyMomentumScrollJankTest, TestSimpleNoJank) {
  cc::InputHandlerScrollResult scroll_result_did_scroll;
  scroll_result_did_scroll.did_scroll = true;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll));

  base::HistogramTester histogram_tester;
  HandleScrollBegin();

  // Flush one update, the first update is always ignored.
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  // Enqueue one updates, no jank.
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  // Enqueue one updates, no jank.
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  HandleScrollEnd();
  DeliverInputForBeginFrame();

  histogram_tester.ExpectUniqueSample("Renderer4.MomentumScrollJankPercentage",
                                      0, 1);
}

TEST_F(InputHandlerProxyMomentumScrollJankTest, TestFirstGestureNoJank) {
  cc::InputHandlerScrollResult scroll_result_did_scroll;
  scroll_result_did_scroll.did_scroll = true;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll));

  base::HistogramTester histogram_tester;
  HandleScrollBegin();

  // Even with 3 coalesced frames, the first gesture should not trigger a jank.
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(16);
  HandleScrollUpdate(true /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  HandleScrollEnd();
  DeliverInputForBeginFrame();

  histogram_tester.ExpectTotalCount("Renderer4.MomentumScrollJankPercentage",
                                    0);
}

TEST_F(InputHandlerProxyMomentumScrollJankTest, TestNonMomentumNoJank) {
  cc::InputHandlerScrollResult scroll_result_did_scroll;
  scroll_result_did_scroll.did_scroll = true;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollUpdate(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0)),
                   _))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll));

  base::HistogramTester histogram_tester;
  HandleScrollBegin();

  // Flush one update, the first update is always ignored.
  AdvanceClock(16);
  HandleScrollUpdate(false /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  // Enqueue three updates, these will not cause jank, as none are momentum.
  AdvanceClock(16);
  HandleScrollUpdate(false /* is_momentum */);
  AdvanceClock(16);
  HandleScrollUpdate(false /* is_momentum */);
  AdvanceClock(16);
  HandleScrollUpdate(false /* is_momentum */);
  AdvanceClock(1);
  DeliverInputForBeginFrame();

  HandleScrollEnd();
  DeliverInputForBeginFrame();

  histogram_tester.ExpectTotalCount("Renderer4.MomentumScrollJankPercentage",
                                    0);
}

INSTANTIATE_TEST_SUITE_P(AnimateInput,
                         InputHandlerProxyTest,
                         testing::ValuesIn(test_types));

INSTANTIATE_TEST_SUITE_P(AnimateInput,
                         InputHandlerProxyMainThreadScrollingReasonTest,
                         testing::ValuesIn(test_types));
}  // namespace test
}  // namespace blink
