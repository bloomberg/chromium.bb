// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/blink/input_handler_proxy.h"

#include <memory>

#include "base/bind.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/trace_event_analyzer.h"
#include "cc/input/main_thread_scrolling_reason.h"
#include "cc/trees/swap_promise_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebFloatPoint.h"
#include "third_party/WebKit/public/platform/WebFloatSize.h"
#include "third_party/WebKit/public/platform/WebGestureCurve.h"
#include "third_party/WebKit/public/platform/WebInputEvent.h"
#include "third_party/WebKit/public/platform/WebKeyboardEvent.h"
#include "third_party/WebKit/public/platform/WebMouseWheelEvent.h"
#include "third_party/WebKit/public/platform/WebPoint.h"
#include "third_party/WebKit/public/platform/WebTouchEvent.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/blink/compositor_thread_event_queue.h"
#include "ui/events/blink/did_overscroll_params.h"
#include "ui/events/blink/event_with_callback.h"
#include "ui/events/blink/input_handler_proxy.h"
#include "ui/events/blink/input_handler_proxy_client.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/gfx/geometry/scroll_offset.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/latency/latency_info.h"

using blink::WebFloatPoint;
using blink::WebFloatSize;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseWheelEvent;
using blink::WebPoint;
using blink::WebSize;
using blink::WebTouchEvent;
using blink::WebTouchPoint;
using testing::Field;

namespace ui {
namespace test {

namespace {

const char* kCoalescedCountHistogram =
    "Event.CompositorThreadEventQueue.CoalescedCount";
const char* kContinuousHeadQueueingTimeHistogram =
    "Event.CompositorThreadEventQueue.Continuous.HeadQueueingTime";
const char* kContinuousTailQueueingTimeHistogram =
    "Event.CompositorThreadEventQueue.Continuous.TailQueueingTime";
const char* kNonContinuousQueueingTimeHistogram =
    "Event.CompositorThreadEventQueue.NonContinuous.QueueingTime";

enum InputHandlerProxyTestType {
  ROOT_SCROLL_NORMAL_HANDLER,
  ROOT_SCROLL_SYNCHRONOUS_HANDLER,
  CHILD_SCROLL_NORMAL_HANDLER,
  CHILD_SCROLL_SYNCHRONOUS_HANDLER,
};
static const InputHandlerProxyTestType test_types[] = {
    ROOT_SCROLL_NORMAL_HANDLER, ROOT_SCROLL_SYNCHRONOUS_HANDLER,
    CHILD_SCROLL_NORMAL_HANDLER, CHILD_SCROLL_SYNCHRONOUS_HANDLER};

double InSecondsF(const base::TimeTicks& time) {
  return (time - base::TimeTicks()).InSecondsF();
}

MATCHER_P(WheelEventsMatch, expected, "") {
  return WheelEventsMatch(arg, expected);
}

WebGestureEvent CreateFling(base::TimeTicks timestamp,
                            WebGestureDevice source_device,
                            WebFloatPoint velocity,
                            WebFloatPoint point,
                            WebFloatPoint global_point,
                            int modifiers) {
  WebGestureEvent fling(WebInputEvent::kGestureFlingStart, modifiers,
                        (timestamp - base::TimeTicks()).InSecondsF(),
                        source_device);
  // Touchpad fling is handled on broswer.
  DCHECK(source_device != blink::kWebGestureDeviceTouchpad);
  fling.data.fling_start.velocity_x = velocity.x;
  fling.data.fling_start.velocity_y = velocity.y;
  fling.SetPositionInWidget(point);
  fling.SetPositionInScreen(global_point);
  return fling;
}

WebGestureEvent CreateFling(WebGestureDevice source_device,
                            WebFloatPoint velocity,
                            WebFloatPoint point,
                            WebFloatPoint global_point,
                            int modifiers) {
  return CreateFling(base::TimeTicks(), source_device, velocity, point,
                     global_point, modifiers);
}

WebScopedInputEvent CreateGestureScrollFlingPinch(
    WebInputEvent::Type type,
    WebGestureDevice source_device,
    float delta_y_or_scale = 0,
    int x = 0,
    int y = 0) {
  WebGestureEvent gesture(type, WebInputEvent::kNoModifiers,
                          WebInputEvent::GetStaticTimeStampForTests(),
                          source_device);
  if (type == WebInputEvent::kGestureScrollUpdate) {
    gesture.data.scroll_update.delta_y = delta_y_or_scale;
  } else if (type == WebInputEvent::kGestureFlingStart) {
    // Touchpad fling is handled on broswer.
    DCHECK(source_device != blink::kWebGestureDeviceTouchpad);
    gesture.data.fling_start.velocity_y = delta_y_or_scale;
  } else if (type == WebInputEvent::kGesturePinchUpdate) {
    gesture.data.pinch_update.scale = delta_y_or_scale;
    gesture.SetPositionInWidget(gfx::PointF(x, y));
  }
  return WebInputEventTraits::Clone(gesture);
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
               ScrollStatus(cc::ScrollState*,
                            cc::InputHandler::ScrollInputType type));
  MOCK_METHOD2(RootScrollBegin,
               ScrollStatus(cc::ScrollState*,
                            cc::InputHandler::ScrollInputType type));
  MOCK_METHOD1(ScrollAnimatedBegin, ScrollStatus(cc::ScrollState*));
  MOCK_METHOD3(ScrollAnimated,
               ScrollStatus(const gfx::Point& viewport_point,
                            const gfx::Vector2dF& scroll_delta,
                            base::TimeDelta));
  MOCK_METHOD1(ScrollBy, cc::InputHandlerScrollResult(cc::ScrollState*));
  MOCK_METHOD2(ScrollEnd, void(cc::ScrollState*, bool));
  MOCK_METHOD0(FlingScrollBegin, cc::InputHandler::ScrollStatus());
  MOCK_METHOD0(ScrollingShouldSwitchtoMainThread, bool());

  std::unique_ptr<cc::SwapPromiseMonitor> CreateLatencyInfoSwapPromiseMonitor(
      ui::LatencyInfo* latency) override {
    return nullptr;
  }

  cc::ScrollElasticityHelper* CreateScrollElasticityHelper() override {
    return NULL;
  }
  bool GetScrollOffsetForLayer(int layer_id,
                               gfx::ScrollOffset* offset) override {
    return false;
  }
  bool ScrollLayerTo(int layer_id, const gfx::ScrollOffset& offset) override {
    return false;
  }

  void BindToClient(cc::InputHandlerClient* client,
                    bool touchpad_and_wheel_scroll_latching_enabled) override {}

  void MouseDown() override {}
  void MouseUp() override {}
  void MouseLeave() override {}

  void MouseMoveAt(const gfx::Point& mouse_position) override {}

  MOCK_CONST_METHOD2(IsCurrentlyScrollingLayerAt,
                     bool(const gfx::Point& point,
                          cc::InputHandler::ScrollInputType type));

  MOCK_CONST_METHOD1(
      GetEventListenerProperties,
      cc::EventListenerProperties(cc::EventListenerClass event_class));
  MOCK_METHOD2(EventListenerTypeForTouchStartOrMoveAt,
               cc::InputHandler::TouchStartOrMoveEventListenerType(
                   const gfx::Point& point,
                   cc::TouchAction* touch_action));

  MOCK_METHOD0(RequestUpdateForSynchronousInputHandler, void());
  MOCK_METHOD1(SetSynchronousInputHandlerRootScrollOffset,
               void(const gfx::ScrollOffset& root_offset));

  bool IsCurrentlyScrollingViewport() const override {
    return is_scrolling_root_;
  }
  void set_is_scrolling_root(bool is) { is_scrolling_root_ = is; }

 private:
  bool is_scrolling_root_ = true;
  DISALLOW_COPY_AND_ASSIGN(MockInputHandler);
};

class MockSynchronousInputHandler : public SynchronousInputHandler {
 public:
  MOCK_METHOD0(SetNeedsSynchronousAnimateInput, void());
  MOCK_METHOD6(UpdateRootLayerState,
               void(const gfx::ScrollOffset& total_scroll_offset,
                    const gfx::ScrollOffset& max_scroll_offset,
                    const gfx::SizeF& scrollable_size,
                    float page_scale_factor,
                    float min_page_scale_factor,
                    float max_page_scale_factor));
};

// A simple WebGestureCurve implementation that flings at a constant velocity
// indefinitely.
class FakeWebGestureCurve : public blink::WebGestureCurve {
 public:
  FakeWebGestureCurve(const blink::WebFloatSize& velocity,
                      const blink::WebFloatSize& cumulative_scroll)
      : velocity_(velocity), cumulative_scroll_(cumulative_scroll) {}

  ~FakeWebGestureCurve() override {}

  bool Advance(double time,
               gfx::Vector2dF& out_current_velocity,
               gfx::Vector2dF& out_delta_to_scroll) override {
    out_current_velocity = velocity_;
    gfx::Vector2dF displacement = velocity_;
    displacement.Scale(time);
    out_delta_to_scroll = displacement - cumulative_scroll_;
    cumulative_scroll_ = displacement;

    // The curve is always active.
    return true;
  }

 private:
  gfx::Vector2dF velocity_;
  gfx::Vector2dF cumulative_scroll_;

  DISALLOW_COPY_AND_ASSIGN(FakeWebGestureCurve);
};

class MockInputHandlerProxyClient
    : public InputHandlerProxyClient {
 public:
  MockInputHandlerProxyClient() {}
  ~MockInputHandlerProxyClient() override {}

  void WillShutdown() override {}

  MOCK_METHOD1(DispatchNonBlockingEventToMainThread_,
               void(const WebInputEvent&));

  MOCK_METHOD1(GenerateScrollBeginAndSendToMainThread,
               void(const blink::WebGestureEvent& update_event));

  void DispatchNonBlockingEventToMainThread(
      WebScopedInputEvent event,
      const ui::LatencyInfo& latency_info) override {
    CHECK(event.get());
    DispatchNonBlockingEventToMainThread_(*event.get());
  }

  std::unique_ptr<blink::WebGestureCurve> CreateFlingAnimationCurve(
      WebGestureDevice deviceSource,
      const WebFloatPoint& velocity,
      const WebSize& cumulative_scroll) override {
    return std::make_unique<FakeWebGestureCurve>(
        blink::WebFloatSize(velocity.x, velocity.y),
        blink::WebFloatSize(cumulative_scroll.width, cumulative_scroll.height));
  }

  MOCK_METHOD5(DidOverscroll,
               void(const gfx::Vector2dF& accumulated_overscroll,
                    const gfx::Vector2dF& latest_overscroll_delta,
                    const gfx::Vector2dF& current_fling_velocity,
                    const gfx::PointF& causal_event_viewport_point,
                    const cc::OverscrollBehavior& overscroll_behavior));
  void DidStopFlinging() override {}
  void DidAnimateForInput() override {}
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

WebTouchPoint CreateWebTouchPoint(WebTouchPoint::State state, float x,
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
                        bool touchpad_and_wheel_scroll_latching_enabled,
                        bool async_wheel_events_enabled)
      : InputHandlerProxy(input_handler,
                          client,
                          touchpad_and_wheel_scroll_latching_enabled,
                          async_wheel_events_enabled) {}
  void RecordMainThreadScrollingReasonsForTest(blink::WebGestureDevice device,
                                               uint32_t reasons) {
    RecordMainThreadScrollingReasons(device, reasons);
  }

  EventDisposition HitTestTouchEventForTest(
      const blink::WebTouchEvent& touch_event,
      bool* is_touching_scrolling_layer,
      cc::TouchAction* white_listed_touch_action) {
    return HitTestTouchEvent(touch_event, is_touching_scrolling_layer,
                             white_listed_touch_action);
  }
};

class InputHandlerProxyTest
    : public testing::Test,
      public testing::WithParamInterface<InputHandlerProxyTestType> {
 public:
  InputHandlerProxyTest(bool touchpad_and_wheel_scroll_latching_enabled = true,
                        bool async_wheel_events_enabled = true)
      : synchronous_root_scroll_(GetParam() == ROOT_SCROLL_SYNCHRONOUS_HANDLER),
        install_synchronous_handler_(
            GetParam() == ROOT_SCROLL_SYNCHRONOUS_HANDLER ||
            GetParam() == CHILD_SCROLL_SYNCHRONOUS_HANDLER),
        expected_disposition_(InputHandlerProxy::DID_HANDLE),
        touchpad_and_wheel_scroll_latching_enabled_(
            touchpad_and_wheel_scroll_latching_enabled),
        async_wheel_events_enabled_(async_wheel_events_enabled) {
    input_handler_.reset(
        new TestInputHandlerProxy(&mock_input_handler_, &mock_client_,
                                  touchpad_and_wheel_scroll_latching_enabled_,
                                  async_wheel_events_enabled_));
    scroll_result_did_scroll_.did_scroll = true;
    scroll_result_did_not_scroll_.did_scroll = false;

    if (install_synchronous_handler_) {
      EXPECT_CALL(mock_input_handler_,
                  RequestUpdateForSynchronousInputHandler())
          .Times(1);
      input_handler_->SetOnlySynchronouslyAnimateRootFlings(
          &mock_synchronous_input_handler_);
    }

    mock_input_handler_.set_is_scrolling_root(synchronous_root_scroll_);

    // Set a default device so tests don't always have to set this.
    gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchpad);
  }

  virtual ~InputHandlerProxyTest() { input_handler_.reset(); }

// This is defined as a macro so the line numbers can be traced back to the
// correct spot when it fails.
#define EXPECT_SET_NEEDS_ANIMATE_INPUT(times)                                \
  do {                                                                       \
    if (synchronous_root_scroll_) {                                          \
      EXPECT_CALL(mock_synchronous_input_handler_,                           \
                  SetNeedsSynchronousAnimateInput())                         \
          .Times(times);                                                     \
      EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(0);     \
    } else {                                                                 \
      EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(times); \
      EXPECT_CALL(mock_synchronous_input_handler_,                           \
                  SetNeedsSynchronousAnimateInput())                         \
          .Times(0);                                                         \
    }                                                                        \
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

  void Animate(base::TimeTicks time) {
    if (synchronous_root_scroll_) {
      input_handler_->SynchronouslyAnimate(time);
    } else {
      input_handler_->Animate(time);
    }
  }

  void StartFling(base::TimeTicks timestamp,
                  WebGestureDevice source_device,
                  WebFloatPoint velocity,
                  WebFloatPoint position) {
    expected_disposition_ = InputHandlerProxy::DID_HANDLE;
    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
        .WillOnce(testing::Return(kImplThreadScrollState));
    gesture_.SetType(WebInputEvent::kGestureScrollBegin);
    gesture_.SetSourceDevice(source_device);
    EXPECT_EQ(expected_disposition_,
              input_handler_->HandleInputEvent(gesture_));

    VERIFY_AND_RESET_MOCKS();

    EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
        .WillOnce(testing::Return(kImplThreadScrollState));
    EXPECT_SET_NEEDS_ANIMATE_INPUT(1);

    gesture_ =
        CreateFling(timestamp, source_device, velocity, position, position, 0);
    EXPECT_EQ(expected_disposition_,
              input_handler_->HandleInputEvent(gesture_));

    VERIFY_AND_RESET_MOCKS();
  }

  void CancelFling(base::TimeTicks timestamp) {
    gesture_.SetTimeStampSeconds(InSecondsF(timestamp));
    gesture_.SetType(WebInputEvent::kGestureFlingCancel);
    EXPECT_EQ(expected_disposition_,
              input_handler_->HandleInputEvent(gesture_));

    VERIFY_AND_RESET_MOCKS();
  }

  void SetSmoothScrollEnabled(bool value) {
    input_handler_->smooth_scroll_enabled_ = value;
  }

  base::HistogramTester& histogram_tester() {
    return histogram_tester_;
  }

 protected:
  void GestureScrollStarted();
  void ScrollHandlingSwitchedToMainThread();
  void GestureScrollIgnored();

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
  bool touchpad_and_wheel_scroll_latching_enabled_;
  bool async_wheel_events_enabled_;
};

class InputHandlerProxyWithoutWheelScrollLatchingTest
    : public InputHandlerProxyTest {
 public:
  InputHandlerProxyWithoutWheelScrollLatchingTest()
      : InputHandlerProxyTest(false, false) {}
};

class InputHandlerProxyEventQueueTest : public testing::TestWithParam<bool> {
 public:
  InputHandlerProxyEventQueueTest() : weak_ptr_factory_(this) {
    feature_list_.InitAndEnableFeature(features::kVsyncAlignedInputEvents);
  }

  ~InputHandlerProxyEventQueueTest() { input_handler_proxy_.reset(); }

  void SetUp() override {
    bool wheel_scroll_latching_enabled = GetParam();
    async_wheel_events_enabled_ = wheel_scroll_latching_enabled;
    event_disposition_recorder_.clear();
    latency_info_recorder_.clear();
    input_handler_proxy_ = std::make_unique<TestInputHandlerProxy>(
        &mock_input_handler_, &mock_client_, wheel_scroll_latching_enabled,
        async_wheel_events_enabled_);
    if (input_handler_proxy_->compositor_event_queue_)
      input_handler_proxy_->compositor_event_queue_ =
          std::make_unique<CompositorThreadEventQueue>();
  }

  void HandleGestureEvent(WebInputEvent::Type type,
                          float delta_y_or_scale = 0,
                          int x = 0,
                          int y = 0) {
    HandleGestureEventWithSourceDevice(
        type, blink::kWebGestureDeviceTouchscreen, delta_y_or_scale, x, y);
  }

  void HandleGestureEventWithSourceDevice(WebInputEvent::Type type,
                                          WebGestureDevice source_device,
                                          float delta_y_or_scale = 0,
                                          int x = 0,
                                          int y = 0) {
    LatencyInfo latency;
    input_handler_proxy_->HandleInputEventWithLatencyInfo(
        CreateGestureScrollFlingPinch(type, source_device, delta_y_or_scale, x,
                                      y),
        latency,
        base::Bind(
            &InputHandlerProxyEventQueueTest::DidHandleInputEventAndOverscroll,
            weak_ptr_factory_.GetWeakPtr()));
  }

  void DidHandleInputEventAndOverscroll(
      InputHandlerProxy::EventDisposition event_disposition,
      WebScopedInputEvent input_event,
      const ui::LatencyInfo& latency_info,
      std::unique_ptr<ui::DidOverscrollParams> overscroll_params) {
    event_disposition_recorder_.push_back(event_disposition);
    latency_info_recorder_.push_back(latency_info);
  }

  base::circular_deque<std::unique_ptr<EventWithCallback>>& event_queue() {
    return input_handler_proxy_->compositor_event_queue_->queue_;
  }

  void SetInputHandlerProxyTickClockForTesting(base::TickClock* tick_clock) {
    input_handler_proxy_->SetTickClockForTesting(tick_clock);
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  testing::StrictMock<MockInputHandler> mock_input_handler_;
  std::unique_ptr<TestInputHandlerProxy> input_handler_proxy_;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client_;
  std::vector<InputHandlerProxy::EventDisposition> event_disposition_recorder_;
  std::vector<ui::LatencyInfo> latency_info_recorder_;
  bool async_wheel_events_enabled_;

  base::MessageLoop loop_;
  base::WeakPtrFactory<InputHandlerProxyEventQueueTest> weak_ptr_factory_;
};

TEST_P(InputHandlerProxyTest, MouseWheelNoListener) {
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelBlockingListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kBlocking));

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MouseWheelBlockingAndPassiveListener) {
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(
          testing::Return(cc::EventListenerProperties::kBlockingAndPassive));

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));
  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::GestureScrollStarted() {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_,input_handler_->HandleInputEvent(gesture_));

  // The event should not be marked as handled if scrolling is not possible.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(false));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // Mark the event as handled if scroll happens.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}
TEST_P(InputHandlerProxyTest, GestureScrollStarted) {
  GestureScrollStarted();
}
TEST_P(InputHandlerProxyWithoutWheelScrollLatchingTest, GestureScrollStarted) {
  GestureScrollStarted();
}

TEST_P(InputHandlerProxyTest, GestureScrollOnMainThread) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(::testing::_, ::testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 40;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::GestureScrollIgnored() {
  // We shouldn't handle the GestureScrollBegin.
  // Instead, we should get a DROP_EVENT result, indicating
  // that we could determine that there's nothing that could scroll or otherwise
  // react to this gesture sequence and thus we should drop the whole gesture
  // sequence on the floor, except for the ScrollEnd.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kScrollIgnoredScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // GSB is dropped and not sent to the main thread, GSE shouldn't get sent to
  // the main thread, either.
  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}
TEST_P(InputHandlerProxyTest, GestureScrollIgnored) {
  GestureScrollIgnored();
}
TEST_P(InputHandlerProxyWithoutWheelScrollLatchingTest, GestureScrollIgnored) {
  GestureScrollIgnored();
}

TEST_P(InputHandlerProxyTest, GestureScrollByPage) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.data.scroll_begin.delta_hint_units =
      WebGestureEvent::ScrollUnits::kPage;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 1;
  gesture_.data.scroll_update.delta_units = WebGestureEvent::ScrollUnits::kPage;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

// Mac does not smooth scroll wheel events (crbug.com/574283).
#if !defined(OS_MACOSX)
TEST_P(InputHandlerProxyTest, GestureScrollByCoarsePixels) {
#else
TEST_P(InputHandlerProxyTest, DISABLED_GestureScrollByCoarsePixels) {
#endif
  SetSmoothScrollEnabled(true);
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.data.scroll_begin.delta_hint_units =
      WebGestureEvent::ScrollUnits::kPixels;
  EXPECT_CALL(mock_input_handler_, ScrollAnimatedBegin(::testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_units =
      WebGestureEvent::ScrollUnits::kPixels;

  EXPECT_CALL(mock_input_handler_,
              ScrollAnimated(::testing::_, ::testing::_, ::testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollBeginThatTargetViewport) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, RootScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.data.scroll_begin.target_viewport = true;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GesturePinch) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchBegin);
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 1.5;
  gesture_.SetPositionInWidget(gfx::PointF(7, 13));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.5, gfx::Point(7, 13)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 0.5;
  gesture_.data.pinch_update.zoom_disabled = true;
  gesture_.SetPositionInWidget(gfx::PointF(9, 6));
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            input_handler_->HandleInputEvent(gesture_));
  gesture_.data.pinch_update.zoom_disabled = false;

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 0.5;
  gesture_.SetPositionInWidget(gfx::PointF(9, 6));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(.5, gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchEnd);
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(9, 6), true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GesturePinchWithWheelHandler) {
  // We will send the synthetic wheel event to the widget.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchBegin);
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kBlocking));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 1.5;
  gesture_.SetPositionInWidget(gfx::PointF(7, 13));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 0.5;
  gesture_.SetPositionInWidget(gfx::PointF(9, 6));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchEnd);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
}

TEST_P(InputHandlerProxyTest, GesturePinchAfterScrollOnMainThread) {
  // Scrolls will start by being sent to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(::testing::_, ::testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = 40;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // However, after the pinch gesture starts, they should go to the impl
  // thread.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchBegin);
  ;
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kNone));
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 1.5;
  gesture_.SetPositionInWidget(gfx::PointF(7, 13));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(1.5, gfx::Point(7, 13)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y =
      -40;  // -Y means scroll down - i.e. in the +Y direction.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchUpdate);
  gesture_.data.pinch_update.scale = 0.5;
  gesture_.SetPositionInWidget(gfx::PointF(9, 6));
  EXPECT_CALL(mock_input_handler_, PinchGestureUpdate(.5, gfx::Point(9, 6)));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGesturePinchEnd);
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(9, 6), true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After the pinch gesture ends, they should go to back to the main
  // thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  gesture_.data.scroll_update.delta_y = 0;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .WillOnce(testing::Return());
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

void InputHandlerProxyTest::ScrollHandlingSwitchedToMainThread() {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(::testing::_, ::testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  // HandleGestureScrollBegin will set gesture_scroll_on_impl_thread_.
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = -40;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  // The scroll handling switches to the main thread, a GSB is sent to the main
  // thread to initiate the hit testing and compute the scroll chain.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollingShouldSwitchtoMainThread())
      .WillOnce(testing::Return(true));
  EXPECT_CALL(mock_client_,
              GenerateScrollBeginAndSendToMainThread(::testing::_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}
TEST_P(InputHandlerProxyTest, WheelScrollHandlingSwitchedToMainThread) {
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchpad);
  ScrollHandlingSwitchedToMainThread();
}
TEST_P(InputHandlerProxyTest, TouchScrollHandlingSwitchedToMainThread) {
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  ScrollHandlingSwitchedToMainThread();
}

TEST_P(InputHandlerProxyTest, GestureFlingStartedTouchscreen) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);

  gesture_.SetType(WebInputEvent::kGestureFlingStart);
  gesture_.data.fling_start.velocity_x = 10;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));

  // Verify that a GestureFlingCancel during an animation cancels it.
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureFlingOnMainThreadTouchscreen) {
  // We should send all events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, FlingScrollBegin()).Times(0);

  gesture_.SetType(WebInputEvent::kGestureFlingStart);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Even if we didn't start a fling ourselves, we still need to send the cancel
  // event to the widget.
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
}

TEST_P(InputHandlerProxyTest, GestureFlingIgnoredTouchscreen) {
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  expected_disposition_ = InputHandlerProxy::DROP_EVENT;
  VERIFY_AND_RESET_MOCKS();

  // Flings ignored by the InputHandler should be dropped, signalling the end
  // of the touch scroll sequence.
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kScrollIgnoredScrollState));

  gesture_.SetType(WebInputEvent::kGestureFlingStart);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Subsequent scrolls should behave normally, even without an intervening
  // GestureFlingCancel, as the original GestureFlingStart was dropped.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureFlingAnimatesTouchscreen) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  WebFloatPoint fling_global_point = WebFloatPoint(17, 23);
  // Note that for touchscreen the control modifier is not special.
  int modifiers = WebInputEvent::kControlKey;
  gesture_ = CreateFling(blink::kWebGestureDeviceTouchscreen, fling_delta,
                         fling_point, fling_global_point, modifiers);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
  // The first animate call should let us pick up an animation start time, but
  // we shouldn't actually move anywhere just yet. The first frame after the
  // fling start will typically include the last scroll from the gesture that
  // lead to the scroll (either wheel or gesture scroll), so there should be no
  // visible hitch.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();

  // The second call should start scrolling in the -X direction.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(100);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureFlingWithValidTimestamp) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  WebFloatPoint fling_global_point = WebFloatPoint(17, 23);
  int modifiers = WebInputEvent::kControlKey;
  gesture_ = CreateFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
                         fling_point, fling_global_point, modifiers);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
  // With a valid time stamp, the first animate call should skip start time
  // initialization and immediately begin scroll update production. This reduces
  // the likelihood of a hitch between the scroll preceding the fling and
  // the first scroll generated by the fling.
  // Scrolling should start in the -X direction.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += dt;
  Animate(time);

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureFlingWithInvalidTimestamp) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  base::TimeDelta start_time_offset = base::TimeDelta::FromMilliseconds(10);
  gesture_ = WebGestureEvent(
      WebInputEvent::kGestureFlingStart, WebInputEvent::kControlKey,
      start_time_offset.InSecondsF(), blink::kWebGestureDeviceTouchscreen);
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  WebFloatPoint fling_global_point = WebFloatPoint(17, 23);
  int modifiers = WebInputEvent::kControlKey;
  gesture_.SetTimeStampSeconds(start_time_offset.InSecondsF());
  gesture_.data.fling_start.velocity_x = fling_delta.x;
  gesture_.data.fling_start.velocity_y = fling_delta.y;
  gesture_.SetPositionInWidget(fling_point);
  gesture_.SetPositionInScreen(fling_global_point);
  gesture_.SetModifiers(modifiers);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
  // Event though a time stamp was provided for the fling event, it will be
  // ignored as its too far in the past relative to the first animate call's
  // timestamp.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  base::TimeTicks time =
      base::TimeTicks() + start_time_offset + base::TimeDelta::FromSeconds(1);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();

  // Further animation ticks should update the fling as usual.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(10);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureScrollOnImplThreadFlagClearedAfterFling) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  WebFloatPoint fling_global_point = WebFloatPoint(17, 23);
  int modifiers = WebInputEvent::kControlKey | WebInputEvent::kAltKey;
  gesture_ = CreateFling(blink::kWebGestureDeviceTouchscreen, fling_delta,
                         fling_point, fling_global_point, modifiers);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // |gesture_scroll_on_impl_thread_| should still be true after
  // a GestureFlingStart is sent.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();
  // The first animate call should let us pick up an animation start time, but
  // we shouldn't actually move anywhere just yet. The first frame after the
  // fling start will typically include the last scroll from the gesture that
  // lead to the scroll (either wheel or gesture scroll), so there should be no
  // visible hitch.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();

  // The second call should start scrolling in the -X direction.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(100);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // |gesture_scroll_on_impl_thread_| should be false once
  // the fling has finished (note no GestureScrollEnd has been sent).
  EXPECT_TRUE(!input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest,
       BeginScrollWhenGestureScrollOnImplThreadFlagIsSet) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  WebFloatPoint fling_global_point = WebFloatPoint(17, 23);
  int modifiers = WebInputEvent::kControlKey | WebInputEvent::kAltKey;
  gesture_ = CreateFling(blink::kWebGestureDeviceTouchscreen, fling_delta,
                         fling_point, fling_global_point, modifiers);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // |gesture_scroll_on_impl_thread_| should still be true after
  // a GestureFlingStart is sent.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  // gesture_scroll_on_impl_thread_ is still true when this scroll begins. As a
  // result, this scroll begin will cancel the previous fling.
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  // After sending a GestureScrollBegin, the member variable
  // |gesture_scroll_on_impl_thread_| should be true.
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureFlingStopsAtContentEdgeTouchscreen) {
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(::testing::_, ::testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  // HandleGestureScrollBegin will set gesture_scroll_on_impl_thread_.
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  gesture_.SetType(WebInputEvent::kGestureFlingStart);
  WebFloatPoint fling_delta = WebFloatPoint(100, 100);
  gesture_.data.fling_start.velocity_x = fling_delta.x;
  gesture_.data.fling_start.velocity_y = fling_delta.y;
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();

  // The first animate doesn't cause any scrolling.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // The second animate starts scrolling in the positive X and Y directions.
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  time += base::TimeDelta::FromMilliseconds(100);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // The third animate overscrolls in the positive Y direction but scrolls
  // somewhat.
  cc::InputHandlerScrollResult overscroll;
  overscroll.did_scroll = true;
  overscroll.did_overscroll_root = true;
  overscroll.accumulated_root_overscroll = gfx::Vector2dF(0, 100);
  overscroll.unused_scroll_delta = gfx::Vector2dF(0, 10);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Lt(0))))
      .WillOnce(testing::Return(overscroll));
  EXPECT_CALL(
      mock_client_,
      DidOverscroll(overscroll.accumulated_root_overscroll,
                    overscroll.unused_scroll_delta,
                    testing::Property(&gfx::Vector2dF::y, testing::Lt(0)),
                    testing::_, overscroll.overscroll_behavior));
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  time += base::TimeDelta::FromMilliseconds(100);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // The next call to animate will no longer scroll vertically.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Eq(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(100);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureFlingNotCancelledBySmallTimeDelta) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  WebFloatPoint fling_global_point = WebFloatPoint(17, 23);
  int modifiers = WebInputEvent::kControlKey;
  gesture_ = CreateFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
                         fling_point, fling_global_point, modifiers);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
  // With an animation timestamp equivalent to the starting timestamp, the
  // animation will simply be rescheduled.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  // A small time delta should not stop the fling, even if the client
  // reports no scrolling.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  time += base::TimeDelta::FromMicroseconds(5);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  // A time delta of zero should not stop the fling, and neither should it
  // trigger scrolling on the client.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());

  // Lack of movement on the client, with a non-trivial scroll delta, should
  // terminate the fling.
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_x, testing::Lt(1))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  time += base::TimeDelta::FromMilliseconds(100);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_P(InputHandlerProxyTest, GestureFlingCancelledAfterBothAxesStopScrolling) {
  cc::InputHandlerScrollResult overscroll;
  overscroll.did_scroll = true;
  overscroll.did_overscroll_root = true;

  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  gesture_.SetType(WebInputEvent::kGestureFlingStart);
  WebFloatPoint fling_delta = WebFloatPoint(100, 100);
  gesture_.data.fling_start.velocity_x = fling_delta.x;
  gesture_.data.fling_start.velocity_y = fling_delta.y;
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();

  // The first animate doesn't cause any scrolling.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // The second animate starts scrolling in the positive X and Y directions.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(10);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // The third animate hits the bottom content edge.
  overscroll.accumulated_root_overscroll = gfx::Vector2dF(0, 100);
  overscroll.unused_scroll_delta = gfx::Vector2dF(0, 100);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Lt(0))))
      .WillOnce(testing::Return(overscroll));
  EXPECT_CALL(
      mock_client_,
      DidOverscroll(overscroll.accumulated_root_overscroll,
                    overscroll.unused_scroll_delta,
                    testing::Property(&gfx::Vector2dF::y, testing::Lt(0)),
                    testing::_, overscroll.overscroll_behavior));
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  time += base::TimeDelta::FromMilliseconds(10);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // The next call to animate will no longer scroll vertically.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Eq(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  time += base::TimeDelta::FromMilliseconds(10);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // The next call will hit the right edge.
  overscroll.accumulated_root_overscroll = gfx::Vector2dF(100, 100);
  overscroll.unused_scroll_delta = gfx::Vector2dF(100, 0);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_x, testing::Lt(0))))
      .WillOnce(testing::Return(overscroll));
  EXPECT_CALL(
      mock_client_,
      DidOverscroll(overscroll.accumulated_root_overscroll,
                    overscroll.unused_scroll_delta,
                    testing::Property(&gfx::Vector2dF::x, testing::Lt(0)),
                    testing::_, overscroll.overscroll_behavior));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  time += base::TimeDelta::FromMilliseconds(10);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // The next call to animate will no longer scroll horizontally or vertically,
  // and the fling should be cancelled.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(0);
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_)).Times(0);
  time += base::TimeDelta::FromMilliseconds(10);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_P(InputHandlerProxyTest, HitTestTouchEventNonNullTouchAction) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Eq(0)), testing::_))
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionMax;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(
          testing::Invoke([](const gfx::Point&, cc::TouchAction* touch_action) {
            *touch_action = cc::kTouchActionPanUp;
            return cc::InputHandler::TouchStartOrMoveEventListenerType::
                HANDLER_ON_SCROLLING_LAYER;
          }));
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);

  bool is_touching_scrolling_layer;
  cc::TouchAction white_listed_touch_action = cc::kTouchActionAuto;
  EXPECT_EQ(expected_disposition_, input_handler_->HitTestTouchEventForTest(
                                       touch, &is_touching_scrolling_layer,
                                       &white_listed_touch_action));
  EXPECT_TRUE(is_touching_scrolling_layer);
  EXPECT_EQ(white_listed_touch_action, cc::kTouchActionPanUp);
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, HitTestTouchEventNullTouchAction) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Eq(0)), testing::_))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              HANDLER_ON_SCROLLING_LAYER));
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::kTouchMove, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.touches_length = 3;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);

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
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .Times(2)
      .WillRepeatedly(testing::Invoke([](const gfx::Point&,
                                         cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionPanUp;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(cc::kTouchActionPanUp, 1,
                                        InputHandlerProxy::DROP_EVENT))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStateStationary, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, MultiTouchPointHitTestPositive) {
  // One of the touch points is on a touch-region. So the event should be sent
  // to the main thread.
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Eq(0)), testing::_))
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionAuto;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(
          testing::Invoke([](const gfx::Point&, cc::TouchAction* touch_action) {
            *touch_action = cc::kTouchActionPanY;
            return cc::InputHandler::TouchStartOrMoveEventListenerType::
                HANDLER_ON_SCROLLING_LAYER;
          }));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(cc::kTouchActionPanY, 1,
                                        InputHandlerProxy::DID_NOT_HANDLE))
      .WillOnce(testing::Return());
  // Since the second touch point hits a touch-region, there should be no
  // hit-testing for the third touch point.

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

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
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .Times(3)
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionPanRight;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }))
      .WillRepeatedly(testing::Invoke([](const gfx::Point&,
                                         cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionPanX;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(
                                cc::kTouchActionPanRight, 1,
                                InputHandlerProxy::DID_HANDLE_NON_BLOCKING))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());

  touch.unique_touch_event_id = 1;
  touch.touches_length = 3;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touches[1] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  touch.touches[2] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, -10, 10);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

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
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .WillOnce(testing::Invoke([](const gfx::Point&,
                                   cc::TouchAction* touch_action) {
        *touch_action = cc::kTouchActionNone;
        return cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER;
      }));
  EXPECT_CALL(mock_client_, SetWhiteListedTouchAction(
                                cc::kTouchActionNone, 1,
                                InputHandlerProxy::DID_HANDLE_NON_BLOCKING))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  touch.unique_touch_event_id = 1;
  touch.touches_length = 1;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  touch.touch_start_or_first_touch_move = true;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(touch));

  touch.SetType(WebInputEvent::kTouchMove);
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = false;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            input_handler_->HandleInputEvent(touch));
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
  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());

  WebTouchEvent touch(WebInputEvent::kTouchStart, WebInputEvent::kNoModifiers,
                      WebInputEvent::GetStaticTimeStampForTests());
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStatePressed, 0, 0);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE_NON_BLOCKING,
            input_handler_->HandleInputEvent(touch));

  EXPECT_CALL(mock_input_handler_,
              EventListenerTypeForTouchStartOrMoveAt(testing::_, testing::_))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::HANDLER));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());

  touch.SetType(WebInputEvent::kTouchMove);
  touch.touches_length = 1;
  touch.touch_start_or_first_touch_move = true;
  touch.touches[0] = CreateWebTouchPoint(WebTouchPoint::kStateMoved, 10, 10);
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            input_handler_->HandleInputEvent(touch));
  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, GestureFlingCancelledByKeyboardEvent) {
  // We shouldn't send any events to the widget for this gesture.
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE,
            input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();

  // Keyboard events received during a scroll should have no effect.
  WebKeyboardEvent key_event(WebInputEvent::kKeyDown,
                             WebInputEvent::kNoModifiers,
                             WebInputEvent::GetStaticTimeStampForTests());
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            input_handler_->HandleInputEvent(key_event));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();

  // On the fling start, animation should be scheduled, but no scrolling occurs.
  gesture_.SetType(WebInputEvent::kGestureFlingStart);
  WebFloatPoint fling_delta = WebFloatPoint(100, 100);
  gesture_.data.fling_start.velocity_x = fling_delta.x;
  gesture_.data.fling_start.velocity_y = fling_delta.y;
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();

  // Keyboard events received during a fling should cancel the active fling.
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  EXPECT_EQ(InputHandlerProxy::DID_NOT_HANDLE,
            input_handler_->HandleInputEvent(key_event));
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();

  // The call to animate should have no effect, as the fling was cancelled.
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // A fling cancel should be dropped, as there is nothing to cancel.
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            input_handler_->HandleInputEvent(gesture_));
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_P(InputHandlerProxyTest, GestureFlingCancelledByWheelEvent) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();

  // Wheel events received during a scroll shouldn't cancel the fling, but will
  // cause scrolling.
  cc::InputHandlerScrollResult result;

  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kBlocking));

  WebMouseWheelEvent wheel_event(WebInputEvent::kMouseWheel,
                                 WebInputEvent::kNoModifiers,
                                 WebInputEvent::GetStaticTimeStampForTests());
  input_handler_->HandleInputEvent(wheel_event);
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();

  // On the fling start, animation should be scheduled, but no scrolling occurs.
  gesture_.SetType(WebInputEvent::kGestureFlingStart);
  WebFloatPoint fling_delta = WebFloatPoint(100, 100);
  gesture_.data.fling_start.velocity_x = fling_delta.x;
  gesture_.data.fling_start.velocity_y = fling_delta.y;
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  EXPECT_TRUE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();

  // Wheel events received during a fling should cancel the active fling, and
  // cause a scroll.
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));

  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kBlocking));

  input_handler_->HandleInputEvent(wheel_event);
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
  VERIFY_AND_RESET_MOCKS();

  // The call to animate should have no effect, as the fling was cancelled.
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  Animate(time);
  VERIFY_AND_RESET_MOCKS();

  // A fling cancel should be dropped, as there is nothing to cancel.
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  EXPECT_EQ(InputHandlerProxy::DROP_EVENT,
            input_handler_->HandleInputEvent(gesture_));
  EXPECT_FALSE(input_handler_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_P(InputHandlerProxyTest, GestureFlingWithNegativeTimeDelta) {
  // We shouldn't send any events to the widget for this gesture.
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));

  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // On the fling start, we should schedule an animation but not actually start
  // scrolling.
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(100, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  WebFloatPoint fling_global_point = WebFloatPoint(17, 23);
  int modifiers = WebInputEvent::kControlKey;
  gesture_ = CreateFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
                         fling_point, fling_global_point, modifiers);
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // If we get a negative time delta, that is, the Animation tick time happens
  // before the fling's start time then we should *not* try scrolling and
  // instead reset the fling start time.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_)).Times(0);
  time -= base::TimeDelta::FromMilliseconds(5);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();

  // The first call should have reset the start time so subsequent calls should
  // generate scroll events.
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_x, testing::Lt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));

  Animate(time + base::TimeDelta::FromMilliseconds(1));

  VERIFY_AND_RESET_MOCKS();

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, FlingBoost) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  base::TimeTicks last_animate_time = time;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  StartFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
             fling_point);

  // Now cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling if a fling cancel
  // is deferred.
  time += dt;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Animate calls within the deferred cancellation window should continue.
  time += dt;
  float expected_delta =
      (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::Property(&cc::ScrollState::delta_x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  Animate(time);
  last_animate_time = time;

  VERIFY_AND_RESET_MOCKS();

  // GestureScrollUpdates in the same direction and at sufficient speed should
  // be swallowed by the fling.
  time += dt;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_x = fling_delta.x;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Animate calls within the deferred cancellation window should continue.
  time += dt;
  expected_delta = (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::Property(&cc::ScrollState::delta_x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  Animate(time);
  last_animate_time = time;

  VERIFY_AND_RESET_MOCKS();

  // GestureFlingStart in the same direction and at sufficient speed should
  // boost the active fling.

  gesture_ = CreateFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
                         fling_point, fling_point, 0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();

  time += dt;
  // Note we get *2x* as much delta because 2 flings have combined.
  expected_delta = 2 * (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::Property(&cc::ScrollState::delta_x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  Animate(time);
  last_animate_time = time;

  VERIFY_AND_RESET_MOCKS();

  // Repeated GestureFlingStarts should accumulate.

  CancelFling(time);
  gesture_ = CreateFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
                         fling_point, fling_point, 0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();

  time += dt;
  // Note we get *3x* as much delta because 3 flings have combined.
  expected_delta = 3 * (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::Property(&cc::ScrollState::delta_x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  Animate(time);
  last_animate_time = time;

  VERIFY_AND_RESET_MOCKS();

  // GestureFlingCancel should terminate the fling if no boosting gestures are
  // received within the timeout window.

  time += dt;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureFlingCancel);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  time += base::TimeDelta::FromMilliseconds(100);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, NoFlingBoostIfScrollDelayed) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  StartFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
             fling_point);

  // Cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling if a fling cancel
  // is deferred.
  time += dt;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // If no GestureScrollUpdate or GestureFlingStart is received within the
  // timeout window, the fling should be cancelled and scrolling should resume.
  time += base::TimeDelta::FromMilliseconds(100);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, NoFlingBoostIfNotAnimated) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  StartFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
             fling_point);

  // Animate fling once.
  time += dt;
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  Animate(time);

  // Cancel the fling after long delay of no animate. The fling cancellation
  // should be deferred to allow fling boosting events to arrive.
  time += base::TimeDelta::FromMilliseconds(100);
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling if a fling cancel
  // is deferred.
  time += dt;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Should exit scroll bosting on GestureScrollUpdate due to long delay
  // since last animate. Cancel old fling and start new scroll.
  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_y = -40;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, ScrollBy(testing::_))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, NoFlingBoostIfFlingInDifferentDirection) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  StartFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
             fling_point);

  // Cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // If the new fling is orthogonal to the existing fling, no boosting should
  // take place, with the new fling replacing the old.
  WebFloatPoint orthogonal_fling_delta =
      WebFloatPoint(fling_delta.y, -fling_delta.x);
  gesture_ = CreateFling(time, blink::kWebGestureDeviceTouchscreen,
                         orthogonal_fling_delta, fling_point, fling_point, 0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Note that the new fling delta uses the orthogonal, unboosted fling
  // velocity.
  time += dt;
  float expected_delta = dt.InSecondsF() * -orthogonal_fling_delta.y;
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::Property(&cc::ScrollState::delta_y,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, NoFlingBoostIfScrollInDifferentDirection) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  StartFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
             fling_point);

  // Cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling if a fling cancel
  // is deferred.
  time += dt;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // If the GestureScrollUpdate is in a different direction than the fling,
  // the fling should be cancelled and scrolling should resume.
  time += dt;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_x = -fling_delta.x;
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::Property(&cc::ScrollState::delta_x,
                                         testing::Eq(fling_delta.x))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, NoFlingBoostIfFlingTooSlow) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  StartFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
             fling_point);

  // Cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // If the new fling is too slow, no boosting should take place, with the new
  // fling replacing the old.
  WebFloatPoint small_fling_delta = WebFloatPoint(100, 0);
  gesture_ = CreateFling(time, blink::kWebGestureDeviceTouchscreen,
                         small_fling_delta, fling_point, fling_point, 0);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Note that the new fling delta uses the *slow*, unboosted fling velocity.
  time += dt;
  float expected_delta = dt.InSecondsF() * -small_fling_delta.x;
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::Property(&cc::ScrollState::delta_x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, NoFlingBoostIfPreventBoostingFlagIsSet) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);

  StartFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
             fling_point);

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));

  // Cancel the fling. The fling cancellation should not be deferred because of
  // prevent boosting flag set.
  gesture_.data.fling_cancel.prevent_boosting = true;
  time += dt;
  CancelFling(time);

  // VERIFY_AND_RESET_MOCKS already called by CancelFling
}

TEST_P(InputHandlerProxyTest, FlingBoostTerminatedDuringScrollSequence) {
  base::TimeDelta dt = base::TimeDelta::FromMilliseconds(10);
  base::TimeTicks time = base::TimeTicks() + dt;
  base::TimeTicks last_animate_time = time;
  WebFloatPoint fling_delta = WebFloatPoint(1000, 0);
  WebFloatPoint fling_point = WebFloatPoint(7, 13);
  StartFling(time, blink::kWebGestureDeviceTouchscreen, fling_delta,
             fling_point);

  // Now cancel the fling.  The fling cancellation should be deferred to allow
  // fling boosting events to arrive.
  time += dt;
  CancelFling(time);

  // The GestureScrollBegin should be swallowed by the fling.
  time += dt;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // Now animate the fling to completion (in this case, the fling should
  // terminate because the input handler reports a failed scroll). As the fling
  // was cancelled during an active scroll sequence, a synthetic
  // GestureScrollBegin should be processed, resuming the scroll.
  time += dt;
  float expected_delta =
      (time - last_animate_time).InSecondsF() * -fling_delta.x;
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::Property(&cc::ScrollState::delta_x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_not_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false));
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  Animate(time);

  VERIFY_AND_RESET_MOCKS();

  // Subsequent GestureScrollUpdates after the cancelled, boosted fling should
  // cause scrolling as usual.
  time += dt;
  expected_delta = 7.3f;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureScrollUpdate);
  gesture_.data.scroll_update.delta_x = -expected_delta;
  EXPECT_CALL(mock_input_handler_,
              ScrollBy(testing::Property(&cc::ScrollState::delta_x,
                                         testing::Eq(expected_delta))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  // GestureScrollEnd should terminate the resumed scroll properly.
  time += dt;
  gesture_.SetTimeStampSeconds(InSecondsF(time));
  gesture_.SetType(WebInputEvent::kGestureScrollEnd);
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, DidReceiveInputEvent_ForFlingTouchscreen) {
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  testing::StrictMock<MockInputHandlerProxyClientWithDidAnimateForInput>
      mock_client;
  input_handler_.reset(
      new TestInputHandlerProxy(&mock_input_handler_, &mock_client,
                                touchpad_and_wheel_scroll_latching_enabled_,
                                async_wheel_events_enabled_));
  if (install_synchronous_handler_) {
    EXPECT_CALL(mock_input_handler_, RequestUpdateForSynchronousInputHandler())
        .Times(1);
    input_handler_->SetOnlySynchronouslyAnimateRootFlings(
        &mock_synchronous_input_handler_);
  }
  mock_input_handler_.set_is_scrolling_root(synchronous_root_scroll_);

  // A GSB must be sent before a GFS.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  gesture_.SetType(WebInputEvent::kGestureScrollBegin);
  gesture_.SetSourceDevice(blink::kWebGestureDeviceTouchscreen);
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(gesture_));

  VERIFY_AND_RESET_MOCKS();

  gesture_.SetType(WebInputEvent::kGestureFlingStart);
  WebFloatPoint fling_delta = WebFloatPoint(100, 100);
  gesture_.data.fling_start.velocity_x = fling_delta.x;
  gesture_.data.fling_start.velocity_y = fling_delta.y;
  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE,
            input_handler_->HandleInputEvent(gesture_));
  VERIFY_AND_RESET_MOCKS();

  EXPECT_SET_NEEDS_ANIMATE_INPUT(1);
  EXPECT_CALL(mock_client, DidAnimateForInput());
  base::TimeTicks time = base::TimeTicks() + base::TimeDelta::FromSeconds(10);
  Animate(time);

  VERIFY_AND_RESET_MOCKS();
}

TEST(SynchronousInputHandlerProxyTest, StartupShutdown) {
  testing::StrictMock<MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  ui::InputHandlerProxy proxy(&mock_input_handler, &mock_client, false, false);

  // When adding a SynchronousInputHandler, immediately request an
  // UpdateRootLayerStateForSynchronousInputHandler() call.
  EXPECT_CALL(mock_input_handler, RequestUpdateForSynchronousInputHandler())
      .Times(1);
  proxy.SetOnlySynchronouslyAnimateRootFlings(&mock_synchronous_input_handler);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);

  EXPECT_CALL(mock_input_handler, RequestUpdateForSynchronousInputHandler())
      .Times(0);
  proxy.SetOnlySynchronouslyAnimateRootFlings(nullptr);

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST(SynchronousInputHandlerProxyTest, UpdateRootLayerState) {
  testing::NiceMock<MockInputHandler> mock_input_handler;
  testing::StrictMock<MockInputHandlerProxyClient> mock_client;
  testing::StrictMock<MockSynchronousInputHandler>
      mock_synchronous_input_handler;
  ui::InputHandlerProxy proxy(&mock_input_handler, &mock_client, false, false);

  proxy.SetOnlySynchronouslyAnimateRootFlings(&mock_synchronous_input_handler);

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
  ui::InputHandlerProxy proxy(&mock_input_handler, &mock_client, false, false);

  proxy.SetOnlySynchronouslyAnimateRootFlings(&mock_synchronous_input_handler);

  EXPECT_CALL(mock_input_handler, SetSynchronousInputHandlerRootScrollOffset(
                                      gfx::ScrollOffset(5, 6)));
  proxy.SynchronouslySetRootScrollOffset(gfx::ScrollOffset(5, 6));

  testing::Mock::VerifyAndClearExpectations(&mock_input_handler);
  testing::Mock::VerifyAndClearExpectations(&mock_client);
  testing::Mock::VerifyAndClearExpectations(&mock_synchronous_input_handler);
}

TEST_P(InputHandlerProxyTest, MainThreadScrollingMouseWheelHistograms) {
  input_handler_->RecordMainThreadScrollingReasonsForTest(
      blink::kWebGestureDeviceTouchpad,
      cc::MainThreadScrollingReason::kHasBackgroundAttachmentFixedObjects |
          cc::MainThreadScrollingReason::kThreadedScrollingDisabled |
          cc::MainThreadScrollingReason::kPageOverlay |
          cc::MainThreadScrollingReason::kHandlingScrollFromMainThread);

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(1, 1), base::Bucket(3, 1),
                           base::Bucket(5, 1)));

  // We only want to record "Handling scroll from main thread" reason if it's
  // the only reason. If it's not the only reason, the "real" reason for
  // scrolling on main is something else, and we only want to pay attention to
  // that reason. So we should only include this reason in the histogram when
  // its on its own.
  input_handler_->RecordMainThreadScrollingReasonsForTest(
      blink::kWebGestureDeviceTouchpad,
      cc::MainThreadScrollingReason::kHandlingScrollFromMainThread);

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.MainThreadWheelScrollReason"),
      testing::ElementsAre(base::Bucket(1, 1), base::Bucket(3, 1),
                           base::Bucket(5, 1), base::Bucket(14, 1)));
}

TEST_P(InputHandlerProxyTest, GestureScrollingThreadStatusHistogram) {
  VERIFY_AND_RESET_MOCKS();

  WebTouchEvent touch_start(WebInputEvent::kTouchStart,
                            WebInputEvent::kNoModifiers,
                            WebInputEvent::GetStaticTimeStampForTests());
  touch_start.touches_length = 1;
  touch_start.touch_start_or_first_touch_move = true;
  touch_start.touches[0] =
      CreateWebTouchPoint(WebTouchPoint::kStatePressed, 10, 10);

  WebGestureEvent gesture_scroll_begin(
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::kWebGestureDeviceTouchscreen);

  WebGestureEvent gesture_scroll_end(
      WebInputEvent::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::kWebGestureDeviceTouchscreen);

  // Touch start with passive event listener.
  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(testing::Return(
          cc::InputHandler::TouchStartOrMoveEventListenerType::NO_HANDLER));
  EXPECT_CALL(
      mock_input_handler_,
      GetEventListenerProperties(cc::EventListenerClass::kTouchStartOrMove))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());

  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(touch_start));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Renderer4.GestureScrollingThreadStatus"),
              testing::ElementsAre(base::Bucket(0, 1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end));

  VERIFY_AND_RESET_MOCKS();

  // Touch event with HANDLER_ON_SCROLLING_LAYER event listener.
  EXPECT_CALL(
      mock_input_handler_,
      EventListenerTypeForTouchStartOrMoveAt(
          testing::Property(&gfx::Point::x, testing::Gt(0)), testing::_))
      .WillOnce(
          testing::Return(cc::InputHandler::TouchStartOrMoveEventListenerType::
                              HANDLER_ON_SCROLLING_LAYER));
  EXPECT_CALL(mock_client_,
              SetWhiteListedTouchAction(testing::_, testing::_, testing::_))
      .WillOnce(testing::Return());

  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(touch_start));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Renderer4.GestureScrollingThreadStatus"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end));

  VERIFY_AND_RESET_MOCKS();

  // Gesture scrolling on main thread.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin));

  EXPECT_THAT(histogram_tester().GetAllSamples(
                  "Renderer4.GestureScrollingThreadStatus"),
              testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1),
                                   base::Bucket(2, 1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyTest, WheelScrollingThreadStatusHistogram) {
  VERIFY_AND_RESET_MOCKS();

  WebMouseWheelEvent wheel(WebInputEvent::kMouseWheel,
                           WebInputEvent::kControlKey,
                           WebInputEvent::GetStaticTimeStampForTests());

  WebGestureEvent gesture_scroll_begin(
      WebInputEvent::kGestureScrollBegin, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::kWebGestureDeviceTouchpad);

  WebGestureEvent gesture_scroll_end(
      WebInputEvent::kGestureScrollEnd, WebInputEvent::kNoModifiers,
      WebInputEvent::GetStaticTimeStampForTests(),
      blink::kWebGestureDeviceTouchpad);

  // Wheel event with passive event listener.
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kPassive));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE_NON_BLOCKING;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.WheelScrollingThreadStatus"),
      testing::ElementsAre(base::Bucket(0, 1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end));

  VERIFY_AND_RESET_MOCKS();

  // Wheel event with blocking event listener.
  EXPECT_CALL(mock_input_handler_,
              GetEventListenerProperties(cc::EventListenerClass::kMouseWheel))
      .WillOnce(testing::Return(cc::EventListenerProperties::kBlocking));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_, input_handler_->HandleInputEvent(wheel));

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.WheelScrollingThreadStatus"),
      testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end));

  VERIFY_AND_RESET_MOCKS();

  // Wheel scrolling on main thread.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kMainThreadScrollState));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_begin));

  EXPECT_THAT(
      histogram_tester().GetAllSamples("Renderer4.WheelScrollingThreadStatus"),
      testing::ElementsAre(base::Bucket(0, 1), base::Bucket(1, 1),
                           base::Bucket(2, 1)));

  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));
  expected_disposition_ = InputHandlerProxy::DID_NOT_HANDLE;
  EXPECT_EQ(expected_disposition_,
            input_handler_->HandleInputEvent(gesture_scroll_end));

  VERIFY_AND_RESET_MOCKS();
}

TEST_P(InputHandlerProxyEventQueueTest, VSyncAlignedGestureScroll) {
  base::HistogramTester histogram_tester;

  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);

  // GestureScrollBegin will be processed immediately.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[0]);

  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);

  // GestureScrollUpdate will be queued.
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(-20, static_cast<const blink::WebGestureEvent&>(
                     event_queue().front()->event())
                     .data.scroll_update.delta_y);
  EXPECT_EQ(1ul, event_queue().front()->coalesced_count());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -40);

  // GestureScrollUpdate will be coalesced.
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(-60, static_cast<const blink::WebGestureEvent&>(
                     event_queue().front()->event())
                     .data.scroll_update.delta_y);
  EXPECT_EQ(2ul, event_queue().front()->coalesced_count());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  // GestureScrollEnd will be queued.
  EXPECT_EQ(2ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));

  // Dispatch all queued events.
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_EQ(0ul, event_queue().size());
  // Should run callbacks for every original events.
  EXPECT_EQ(4ul, event_disposition_recorder_.size());
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[1]);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[2]);
  EXPECT_EQ(InputHandlerProxy::DID_HANDLE, event_disposition_recorder_[3]);
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  histogram_tester.ExpectUniqueSample(kCoalescedCountHistogram, 2, 1);
}

TEST_P(InputHandlerProxyEventQueueTest, VSyncAlignedGestureScrollPinchScroll) {
  base::HistogramTester histogram_tester;

  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  // Start scroll in the first frame.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);

  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  input_handler_proxy_->DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);

  // Continue scroll in the second frame, pinch, then start another scroll.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true)).Times(2);
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(mock_input_handler_, PinchGestureBegin());
  // Two |GesturePinchUpdate| will be coalesced.
  EXPECT_CALL(mock_input_handler_,
              PinchGestureUpdate(0.7f, gfx::Point(13, 17)));
  EXPECT_CALL(mock_input_handler_, PinchGestureEnd(gfx::Point(), false));

  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);
  HandleGestureEvent(WebInputEvent::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 1.4f, 13, 17);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 0.5f, 13, 17);
  HandleGestureEvent(WebInputEvent::kGesturePinchEnd);
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -70);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -5);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  EXPECT_EQ(8ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  input_handler_proxy_->DeliverInputForBeginFrame();

  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(12ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  histogram_tester.ExpectBucketCount(kCoalescedCountHistogram, 1, 2);
  histogram_tester.ExpectBucketCount(kCoalescedCountHistogram, 2, 2);
}

TEST_P(InputHandlerProxyEventQueueTest, VSyncAlignedQueueingTime) {
  base::HistogramTester histogram_tester;
  base::SimpleTestTickClock tick_clock;
  tick_clock.SetNowTicks(base::TimeTicks::Now());
  SetInputHandlerProxyTickClockForTesting(&tick_clock);

  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));

  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(10));
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(40));
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -40);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(20));
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(10));
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  // Dispatch all queued events.
  tick_clock.Advance(base::TimeDelta::FromMicroseconds(70));
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
  histogram_tester.ExpectUniqueSample(kContinuousHeadQueueingTimeHistogram, 140,
                                      1);
  histogram_tester.ExpectUniqueSample(kContinuousTailQueueingTimeHistogram, 80,
                                      1);
  histogram_tester.ExpectBucketCount(kNonContinuousQueueingTimeHistogram, 0, 1);
  histogram_tester.ExpectBucketCount(kNonContinuousQueueingTimeHistogram, 70,
                                     1);
}

TEST_P(InputHandlerProxyEventQueueTest, VSyncAlignedCoalesceScrollAndPinch) {
  // Start scroll in the first frame.
  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);

  // GSUs and GPUs in one sequence should be coalesced into 1 GSU and 1 GPU.
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -7);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 2.0f, 13, 10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -6);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);
  HandleGestureEvent(WebInputEvent::kGesturePinchBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 0.2f, 2, 20);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 0.25f, 3, 30);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::kGesturePinchEnd);

  // Only the first GSB was dispatched.
  EXPECT_EQ(7ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());

  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            event_queue()[0]->event().GetType());
  EXPECT_EQ(
      -35,
      ToWebGestureEvent(event_queue()[0]->event()).data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            event_queue()[1]->event().GetType());
  EXPECT_EQ(
      2.0f,
      ToWebGestureEvent(event_queue()[1]->event()).data.pinch_update.scale);
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            event_queue()[2]->event().GetType());
  EXPECT_EQ(WebInputEvent::kGesturePinchBegin,
            event_queue()[3]->event().GetType());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            event_queue()[4]->event().GetType());
  EXPECT_EQ(
      -85,
      ToWebGestureEvent(event_queue()[4]->event()).data.scroll_update.delta_y);
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            event_queue()[5]->event().GetType());
  EXPECT_EQ(
      0.5f,
      ToWebGestureEvent(event_queue()[5]->event()).data.pinch_update.scale);
  EXPECT_EQ(WebInputEvent::kGesturePinchEnd,
            event_queue()[6]->event().GetType());
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, OriginalEventsTracing) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .Times(::testing::AtLeast(1));

  trace_analyzer::Start("*");
  // Simulate scroll.
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  // Simulate scroll and pinch.
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 10.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -10);
  HandleGestureEvent(WebInputEvent::kGesturePinchUpdate, 2.0f, 1, 10);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);

  // Dispatch all events.
  input_handler_proxy_->DeliverInputForBeginFrame();

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

  EXPECT_EQ(5ul, begin_events.size());
  EXPECT_EQ(5ul, end_events.size());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            end_events[0]->GetKnownArgAsInt("type"));
  EXPECT_EQ(3, end_events[0]->GetKnownArgAsInt("coalesced_count"));
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            end_events[1]->GetKnownArgAsInt("type"));

  EXPECT_EQ(WebInputEvent::kGestureScrollBegin,
            end_events[2]->GetKnownArgAsInt("type"));
  // Original scroll and pinch updates will be stored in the coalesced
  // PinchUpdate of the <ScrollUpdate, PinchUpdate> pair.
  // The ScrollUpdate of the pair doesn't carry original events and won't be
  // traced.
  EXPECT_EQ(WebInputEvent::kGesturePinchUpdate,
            end_events[3]->GetKnownArgAsInt("type"));
  EXPECT_EQ(4, end_events[3]->GetKnownArgAsInt("coalesced_count"));
  EXPECT_EQ(WebInputEvent::kGestureScrollEnd,
            end_events[4]->GetKnownArgAsInt("type"));
  testing::Mock::VerifyAndClearExpectations(&mock_input_handler_);
}

TEST_P(InputHandlerProxyEventQueueTest, GestureScrollFlingOrder) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false))
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));

  // Simulate scroll.
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::kGestureFlingStart, -10);

  // ScrollUpdate and FlingStart should be queued.
  EXPECT_EQ(2ul, event_queue().size());
  EXPECT_EQ(1ul, event_disposition_recorder_.size());
  EXPECT_EQ(WebInputEvent::kGestureScrollUpdate,
            event_queue()[0]->event().GetType());
  EXPECT_EQ(WebInputEvent::kGestureFlingStart,
            event_queue()[1]->event().GetType());

  // Dispatch events.
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(4ul, event_disposition_recorder_.size());
  EXPECT_TRUE(
      input_handler_proxy_->gesture_scroll_on_impl_thread_for_testing());

  // Send FlingCancel to stop scrolling.
  HandleGestureEvent(WebInputEvent::kGestureFlingCancel);
  EXPECT_EQ(1ul, event_queue().size());
  EXPECT_EQ(WebInputEvent::kGestureFlingCancel,
            event_queue()[0]->event().GetType());
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(5ul, event_disposition_recorder_.size());
  // Should stop scrolling. Note that no ScrollEnd was sent.
  EXPECT_TRUE(
      !input_handler_proxy_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_P(InputHandlerProxyEventQueueTest, GestureScrollAfterFling) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, false))
      .Times(::testing::AtLeast(1));
  EXPECT_CALL(mock_input_handler_, FlingScrollBegin())
      .WillOnce(testing::Return(kImplThreadScrollState));

  // Simulate fling.
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureFlingStart, -10);
  HandleGestureEvent(WebInputEvent::kGestureFlingCancel);

  // Dispatch events.
  input_handler_proxy_->DeliverInputForBeginFrame();
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(4ul, event_disposition_recorder_.size());
  EXPECT_FALSE(
      input_handler_proxy_->gesture_scroll_on_impl_thread_for_testing());

  // New ScrollBegin should be dispatched immediately as there is no on-going
  // scroll, fling or pinch.
  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  EXPECT_EQ(0ul, event_queue().size());
}

TEST_P(InputHandlerProxyEventQueueTest, TouchpadGestureScrollEndFlushQueue) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillRepeatedly(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillRepeatedly(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true))
      .Times(::testing::AtLeast(1));

  // Simulate scroll.
  HandleGestureEventWithSourceDevice(WebInputEvent::kGestureScrollBegin,
                                     blink::kWebGestureDeviceTouchpad);
  HandleGestureEventWithSourceDevice(WebInputEvent::kGestureScrollUpdate,
                                     blink::kWebGestureDeviceTouchpad, -20);

  // Both GSB and the first GSU will be dispatched immediately since the first
  // GSU has blocking wheel event source.
  EXPECT_EQ(0ul, event_queue().size());
  EXPECT_EQ(2ul, event_disposition_recorder_.size());

  // When async_wheel_events_enabled_ the rest of the GSU events will get queued
  // since they have non-blocking wheel event source.
  if (async_wheel_events_enabled_) {
    EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput())
        .Times(::testing::AtLeast(1));
    HandleGestureEventWithSourceDevice(WebInputEvent::kGestureScrollUpdate,
                                       blink::kWebGestureDeviceTouchpad, -20);
    EXPECT_EQ(1ul, event_queue().size());
    EXPECT_EQ(2ul, event_disposition_recorder_.size());
  }

  // Touchpad GSE will flush the queue.
  HandleGestureEventWithSourceDevice(WebInputEvent::kGestureScrollEnd,
                                     blink::kWebGestureDeviceTouchpad);

  EXPECT_EQ(0ul, event_queue().size());
  if (async_wheel_events_enabled_) {
    // GSB, GSU(with blocking wheel source), GSU(with non-blocking wheel
    // source), and GSE are the sent events.
    EXPECT_EQ(4ul, event_disposition_recorder_.size());
  } else {
    // GSB, GSU(with blocking wheel source), and GSE are the sent events.
    EXPECT_EQ(3ul, event_disposition_recorder_.size());
  }

  EXPECT_FALSE(
      input_handler_proxy_->gesture_scroll_on_impl_thread_for_testing());
}

TEST_P(InputHandlerProxyEventQueueTest, CoalescedLatencyInfo) {
  // Handle scroll on compositor.
  cc::InputHandlerScrollResult scroll_result_did_scroll_;
  scroll_result_did_scroll_.did_scroll = true;

  EXPECT_CALL(mock_input_handler_, ScrollBegin(testing::_, testing::_))
      .WillOnce(testing::Return(kImplThreadScrollState));
  EXPECT_CALL(mock_input_handler_, SetNeedsAnimateInput()).Times(1);
  EXPECT_CALL(
      mock_input_handler_,
      ScrollBy(testing::Property(&cc::ScrollState::delta_y, testing::Gt(0))))
      .WillOnce(testing::Return(scroll_result_did_scroll_));
  EXPECT_CALL(mock_input_handler_, ScrollEnd(testing::_, true));

  HandleGestureEvent(WebInputEvent::kGestureScrollBegin);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -20);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -40);
  HandleGestureEvent(WebInputEvent::kGestureScrollUpdate, -30);
  HandleGestureEvent(WebInputEvent::kGestureScrollEnd);
  input_handler_proxy_->DeliverInputForBeginFrame();

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

INSTANTIATE_TEST_CASE_P(AnimateInput,
                        InputHandlerProxyTest,
                        testing::ValuesIn(test_types));

INSTANTIATE_TEST_CASE_P(AnimateInput,
                        InputHandlerProxyWithoutWheelScrollLatchingTest,
                        testing::ValuesIn(test_types));

INSTANTIATE_TEST_CASE_P(InputHandlerProxyEventQueueTests,
                        InputHandlerProxyEventQueueTest,
                        testing::Bool());

}  // namespace test
}  // namespace ui
