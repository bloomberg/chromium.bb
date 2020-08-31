// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "cc/trees/render_frame_metadata.h"
#include "components/viz/common/surfaces/local_surface_id.h"
#include "components/viz/common/surfaces/parent_local_surface_id_allocator.h"
#include "components/viz/test/begin_frame_args_test.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/input/mock_input_router.h"
#include "content/browser/renderer_host/input/touch_emulator.h"
#include "content/browser/renderer_host/mock_render_widget_host.h"
#include "content/browser/renderer_host/render_view_host_delegate_view.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_view_base.h"
#include "content/common/content_constants_internal.h"
#include "content/common/input/synthetic_web_input_event_builders.h"
#include "content/common/input_messages.h"
#include "content/common/render_frame_metadata.mojom.h"
#include "content/common/visual_properties.h"
#include "content/common/widget_messages.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_widget_impl.h"
#include "content/test/mock_widget_input_handler.h"
#include "content/test/stub_render_widget_host_owner_delegate.h"
#include "content/test/test_render_view_host.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/mojom/input/input_handler.mojom-shared.h"
#include "ui/display/screen.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/blink/blink_features.h"
#include "ui/events/blink/web_input_event_traits.h"
#include "ui/events/gesture_detection/gesture_provider_config_helper.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/canvas.h"

#if defined(OS_ANDROID)
#include "base/strings/utf_string_conversions.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "ui/android/screen_android.h"
#endif

#if defined(USE_AURA) || defined(OS_MACOSX)
#include "content/browser/compositor/test/test_image_transport_factory.h"
#endif

#if defined(USE_AURA)
#include "content/browser/renderer_host/render_widget_host_view_aura.h"
#include "content/browser/renderer_host/ui_events_helper.h"
#include "ui/aura/test/test_screen.h"
#include "ui/events/event.h"
#endif

using base::TimeDelta;
using blink::WebGestureDevice;
using blink::WebGestureEvent;
using blink::WebInputEvent;
using blink::WebKeyboardEvent;
using blink::WebMouseEvent;
using blink::WebMouseWheelEvent;
using blink::WebTouchEvent;
using blink::WebTouchPoint;

using testing::_;
using testing::Return;

namespace content {
namespace  {

// RenderWidgetHostProcess -----------------------------------------------------

class RenderWidgetHostProcess : public MockRenderProcessHost {
 public:
  explicit RenderWidgetHostProcess(BrowserContext* browser_context)
      : MockRenderProcessHost(browser_context) {
  }
  ~RenderWidgetHostProcess() override {}

  bool IsInitializedAndNotDead() override { return true; }

 private:
  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostProcess);
};

// TestView --------------------------------------------------------------------

// This test view allows us to specify the size, and keep track of acked
// touch-events.
class TestView : public TestRenderWidgetHostView {
 public:
  explicit TestView(RenderWidgetHostImpl* rwh)
      : TestRenderWidgetHostView(rwh),
        unhandled_wheel_event_count_(0),
        acked_event_count_(0),
        gesture_event_type_(WebInputEvent::Type::kUndefined),
        use_fake_compositor_viewport_pixel_size_(false),
        ack_result_(blink::mojom::InputEventResultState::kUnknown) {
    local_surface_id_allocator_.GenerateId();
  }

  // Sets the bounds returned by GetViewBounds.
  void SetBounds(const gfx::Rect& bounds) override {
    if (bounds_ == bounds)
      return;
    bounds_ = bounds;
    local_surface_id_allocator_.GenerateId();
  }

  void SetScreenInfo(const ScreenInfo& screen_info) {
    if (screen_info_ == screen_info)
      return;
    screen_info_ = screen_info;
    local_surface_id_allocator_.GenerateId();
  }

  void InvalidateLocalSurfaceId() { local_surface_id_allocator_.Invalidate(); }

  void GetScreenInfo(ScreenInfo* screen_info) override {
    *screen_info = screen_info_;
  }

  const WebTouchEvent& acked_event() const { return acked_event_; }
  int acked_event_count() const { return acked_event_count_; }
  void ClearAckedEvent() {
    acked_event_.SetType(blink::WebInputEvent::Type::kUndefined);
    acked_event_count_ = 0;
  }

  const WebMouseWheelEvent& unhandled_wheel_event() const {
    return unhandled_wheel_event_;
  }
  int unhandled_wheel_event_count() const {
    return unhandled_wheel_event_count_;
  }
  WebInputEvent::Type gesture_event_type() const { return gesture_event_type_; }
  blink::mojom::InputEventResultState ack_result() const { return ack_result_; }

  void SetMockCompositorViewportPixelSize(
      const gfx::Size& mock_compositor_viewport_pixel_size) {
    if (use_fake_compositor_viewport_pixel_size_ &&
        mock_compositor_viewport_pixel_size_ ==
            mock_compositor_viewport_pixel_size) {
      return;
    }
    use_fake_compositor_viewport_pixel_size_ = true;
    mock_compositor_viewport_pixel_size_ = mock_compositor_viewport_pixel_size;
    local_surface_id_allocator_.GenerateId();
  }
  void ClearMockCompositorViewportPixelSize() {
    if (!use_fake_compositor_viewport_pixel_size_)
      return;
    use_fake_compositor_viewport_pixel_size_ = false;
    local_surface_id_allocator_.GenerateId();
  }

  // RenderWidgetHostView override.
  gfx::Rect GetViewBounds() override { return bounds_; }
  const viz::LocalSurfaceIdAllocation& GetLocalSurfaceIdAllocation()
      const override {
    return local_surface_id_allocator_.GetCurrentLocalSurfaceIdAllocation();
  }

  void ProcessAckedTouchEvent(
      const TouchEventWithLatencyInfo& touch,
      blink::mojom::InputEventResultState ack_result) override {
    acked_event_ = touch.event;
    ++acked_event_count_;
  }
  void WheelEventAck(const WebMouseWheelEvent& event,
                     blink::mojom::InputEventResultState ack_result) override {
    if (ack_result == blink::mojom::InputEventResultState::kConsumed)
      return;
    unhandled_wheel_event_count_++;
    unhandled_wheel_event_ = event;
  }
  void GestureEventAck(
      const WebGestureEvent& event,
      blink::mojom::InputEventResultState ack_result) override {
    gesture_event_type_ = event.GetType();
    ack_result_ = ack_result;
  }
  gfx::Size GetCompositorViewportPixelSize() override {
    if (use_fake_compositor_viewport_pixel_size_)
      return mock_compositor_viewport_pixel_size_;
    return TestRenderWidgetHostView::GetCompositorViewportPixelSize();
  }

 protected:
  WebMouseWheelEvent unhandled_wheel_event_;
  int unhandled_wheel_event_count_;
  WebTouchEvent acked_event_;
  int acked_event_count_;
  WebInputEvent::Type gesture_event_type_;
  gfx::Rect bounds_;
  bool use_fake_compositor_viewport_pixel_size_;
  gfx::Size mock_compositor_viewport_pixel_size_;
  blink::mojom::InputEventResultState ack_result_;
  viz::ParentLocalSurfaceIdAllocator local_surface_id_allocator_;
  ScreenInfo screen_info_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestView);
};

// MockRenderViewHostDelegateView ------------------------------------------
class MockRenderViewHostDelegateView : public RenderViewHostDelegateView {
 public:
  MockRenderViewHostDelegateView() = default;
  ~MockRenderViewHostDelegateView() override = default;

  int start_dragging_count() const { return start_dragging_count_; }

  // RenderViewHostDelegateView:
  void StartDragging(const DropData& drop_data,
                     blink::WebDragOperationsMask allowed_ops,
                     const gfx::ImageSkia& image,
                     const gfx::Vector2d& image_offset,
                     const DragEventSourceInfo& event_info,
                     RenderWidgetHostImpl* source_rwh) override {
    ++start_dragging_count_;
  }

 private:
  int start_dragging_count_ = 0;

  DISALLOW_COPY_AND_ASSIGN(MockRenderViewHostDelegateView);
};

// FakeRenderFrameMetadataObserver -----------------------------------------

// Fake out the renderer side of mojom::RenderFrameMetadataObserver, allowing
// for RenderWidgetHostImpl to be created.
//
// All methods are no-opts, the provided mojo receiver and remote are held, but
// never bound.
class FakeRenderFrameMetadataObserver
    : public mojom::RenderFrameMetadataObserver {
 public:
  FakeRenderFrameMetadataObserver(
      mojo::PendingReceiver<mojom::RenderFrameMetadataObserver> receiver,
      mojo::PendingRemote<mojom::RenderFrameMetadataObserverClient>
          client_remote);
  ~FakeRenderFrameMetadataObserver() override {}

#if defined(OS_ANDROID)
  void ReportAllRootScrollsForAccessibility(bool enabled) override {}
#endif
  void ReportAllFrameSubmissionsForTesting(bool enabled) override {}

 private:
  mojo::PendingReceiver<mojom::RenderFrameMetadataObserver> receiver_;
  mojo::PendingRemote<mojom::RenderFrameMetadataObserverClient> client_remote_;
  DISALLOW_COPY_AND_ASSIGN(FakeRenderFrameMetadataObserver);
};

FakeRenderFrameMetadataObserver::FakeRenderFrameMetadataObserver(
    mojo::PendingReceiver<mojom::RenderFrameMetadataObserver> receiver,
    mojo::PendingRemote<mojom::RenderFrameMetadataObserverClient> client_remote)
    : receiver_(std::move(receiver)),
      client_remote_(std::move(client_remote)) {}

// MockInputEventObserver -------------------------------------------------
class MockInputEventObserver : public RenderWidgetHost::InputEventObserver {
 public:
  MOCK_METHOD1(OnInputEvent, void(const blink::WebInputEvent&));
#if defined(OS_ANDROID)
  MOCK_METHOD1(OnImeTextCommittedEvent, void(const base::string16& text_str));
  MOCK_METHOD1(OnImeSetComposingTextEvent,
               void(const base::string16& text_str));
  MOCK_METHOD0(OnImeFinishComposingTextEvent, void());
#endif
};

// MockRenderWidgetHostDelegate --------------------------------------------

class MockRenderWidgetHostDelegate : public RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate()
      : prehandle_keyboard_event_(false),
        prehandle_keyboard_event_is_shortcut_(false),
        prehandle_keyboard_event_called_(false),
        prehandle_keyboard_event_type_(WebInputEvent::Type::kUndefined),
        unhandled_keyboard_event_called_(false),
        unhandled_keyboard_event_type_(WebInputEvent::Type::kUndefined),
        handle_wheel_event_(false),
        handle_wheel_event_called_(false),
        unresponsive_timer_fired_(false),
        ignore_input_events_(false),
        render_view_host_delegate_view_(new MockRenderViewHostDelegateView()) {}
  ~MockRenderWidgetHostDelegate() override {}

  // Tests that make sure we ignore keyboard event acknowledgments to events we
  // didn't send work by making sure we didn't call UnhandledKeyboardEvent().
  bool unhandled_keyboard_event_called() const {
    return unhandled_keyboard_event_called_;
  }

  WebInputEvent::Type unhandled_keyboard_event_type() const {
    return unhandled_keyboard_event_type_;
  }

  bool prehandle_keyboard_event_called() const {
    return prehandle_keyboard_event_called_;
  }

  WebInputEvent::Type prehandle_keyboard_event_type() const {
    return prehandle_keyboard_event_type_;
  }

  void set_prehandle_keyboard_event(bool handle) {
    prehandle_keyboard_event_ = handle;
  }

  void set_handle_wheel_event(bool handle) {
    handle_wheel_event_ = handle;
  }

  void set_prehandle_keyboard_event_is_shortcut(bool is_shortcut) {
    prehandle_keyboard_event_is_shortcut_ = is_shortcut;
  }

  bool handle_wheel_event_called() const { return handle_wheel_event_called_; }

  bool unresponsive_timer_fired() const { return unresponsive_timer_fired_; }

  MockRenderViewHostDelegateView* mock_delegate_view() {
    return render_view_host_delegate_view_.get();
  }

  void SetZoomLevel(double zoom_level) { zoom_level_ = zoom_level; }

  double GetPendingPageZoomLevel() override { return zoom_level_; }

  void FocusOwningWebContents(
      RenderWidgetHostImpl* render_widget_host) override {
    focus_owning_web_contents_call_count++;
  }

  int GetFocusOwningWebContentsCallCount() const {
    return focus_owning_web_contents_call_count;
  }

  void OnVerticalScrollDirectionChanged(
      viz::VerticalScrollDirection scroll_direction) override {
    ++on_vertical_scroll_direction_changed_call_count_;
    last_vertical_scroll_direction_ = scroll_direction;
  }

  int GetOnVerticalScrollDirectionChangedCallCount() const {
    return on_vertical_scroll_direction_changed_call_count_;
  }

  viz::VerticalScrollDirection GetLastVerticalScrollDirection() const {
    return last_vertical_scroll_direction_;
  }

  RenderViewHostDelegateView* GetDelegateView() override {
    return mock_delegate_view();
  }

  void SetIgnoreInputEvents(bool ignore_input_events) {
    ignore_input_events_ = ignore_input_events;
  }

  bool IsFullscreenForCurrentTab() override { return is_fullscreen_for_tab_; }

  void set_is_fullscreen_for_current_tab(bool enabled) {
    is_fullscreen_for_tab_ = enabled;
  }

 protected:
  KeyboardEventProcessingResult PreHandleKeyboardEvent(
      const NativeWebKeyboardEvent& event) override {
    prehandle_keyboard_event_type_ = event.GetType();
    prehandle_keyboard_event_called_ = true;
    if (prehandle_keyboard_event_)
      return KeyboardEventProcessingResult::HANDLED;
    return prehandle_keyboard_event_is_shortcut_
               ? KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT
               : KeyboardEventProcessingResult::NOT_HANDLED;
  }

  bool HandleKeyboardEvent(const NativeWebKeyboardEvent& event) override {
    unhandled_keyboard_event_type_ = event.GetType();
    unhandled_keyboard_event_called_ = true;
    return true;
  }

  bool HandleWheelEvent(const blink::WebMouseWheelEvent& event) override {
    handle_wheel_event_called_ = true;
    return handle_wheel_event_;
  }

  void RendererUnresponsive(
      RenderWidgetHostImpl* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override {
    unresponsive_timer_fired_ = true;
  }

  bool ShouldIgnoreInputEvents() override { return ignore_input_events_; }

  void ExecuteEditCommand(
      const std::string& command,
      const base::Optional<base::string16>& value) override {}

  void Undo() override {}
  void Redo() override {}
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void PasteAndMatchStyle() override {}
  void SelectAll() override {}

 private:
  bool prehandle_keyboard_event_;
  bool prehandle_keyboard_event_is_shortcut_;
  bool prehandle_keyboard_event_called_;
  WebInputEvent::Type prehandle_keyboard_event_type_;

  bool unhandled_keyboard_event_called_;
  WebInputEvent::Type unhandled_keyboard_event_type_;

  bool handle_wheel_event_;
  bool handle_wheel_event_called_;

  bool unresponsive_timer_fired_;

  bool ignore_input_events_;

  std::unique_ptr<MockRenderViewHostDelegateView>
      render_view_host_delegate_view_;

  double zoom_level_ = 0;

  int focus_owning_web_contents_call_count = 0;

  int on_vertical_scroll_direction_changed_call_count_ = 0;
  viz::VerticalScrollDirection last_vertical_scroll_direction_ =
      viz::VerticalScrollDirection::kNull;

  bool is_fullscreen_for_tab_ = false;
};

class MockRenderWidgetHostOwnerDelegate
    : public StubRenderWidgetHostOwnerDelegate {
 public:
  MOCK_METHOD1(SetBackgroundOpaque, void(bool opaque));
  MOCK_METHOD1(UpdatePageVisualProperties,
               void(const VisualProperties& visual_properties));
  MOCK_METHOD0(IsMainFrameActive, bool());
};

// RenderWidgetHostTest --------------------------------------------------------

class RenderWidgetHostTest : public testing::Test {
 public:
  RenderWidgetHostTest()
      : process_(nullptr),
        handle_key_press_event_(false),
        handle_mouse_event_(false),
        last_simulated_event_time_(ui::EventTimeForNow()) {}
  ~RenderWidgetHostTest() override {}

  bool KeyPressEventCallback(const NativeWebKeyboardEvent& /* event */) {
    return handle_key_press_event_;
  }
  bool MouseEventCallback(const blink::WebMouseEvent& /* event */) {
    return handle_mouse_event_;
  }

 protected:
  // testing::Test
  void SetUp() override {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    command_line->AppendSwitch(switches::kValidateInputEventStream);
    browser_context_.reset(new TestBrowserContext());
    delegate_.reset(new MockRenderWidgetHostDelegate());
    process_ = new RenderWidgetHostProcess(browser_context_.get());
    sink_ = &process_->sink();
#if defined(USE_AURA) || defined(OS_MACOSX)
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
#endif
#if defined(OS_ANDROID)
    // calls display::Screen::SetScreenInstance().
    ui::SetScreenAndroid(false /* use_display_wide_color_gamut */);
#endif
#if defined(USE_AURA)
    screen_.reset(aura::TestScreen::Create(gfx::Size()));
    display::Screen::SetScreenInstance(screen_.get());
#endif
    host_.reset(MockRenderWidgetHost::Create(delegate_.get(), process_,
                                             process_->GetNextRoutingID()));
    // Set up the RenderWidgetHost as being for a main frame.
    host_->set_owner_delegate(&mock_owner_delegate_);
    // Act like there is no RenderWidget present in the renderer yet.
    EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
        .WillRepeatedly(Return(false));

    view_ = std::make_unique<TestView>(host_.get());
    ConfigureView(view_.get());
    host_->SetView(view_.get());
    host_->DisableGestureDebounce();
    // Act like we've created a RenderWidget.
    host_->GetInitialVisualProperties();
    EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
        .WillRepeatedly(Return(true));
    // Init() happens once the navigation completes.
    host_->Init();

    mojo::PendingRemote<mojom::RenderFrameMetadataObserver>
        renderer_render_frame_metadata_observer_remote;
    mojo::PendingRemote<mojom::RenderFrameMetadataObserverClient>
        render_frame_metadata_observer_remote;
    mojo::PendingReceiver<mojom::RenderFrameMetadataObserverClient>
        render_frame_metadata_observer_client_receiver =
            render_frame_metadata_observer_remote
                .InitWithNewPipeAndPassReceiver();
    renderer_render_frame_metadata_observer_ =
        std::make_unique<FakeRenderFrameMetadataObserver>(
            renderer_render_frame_metadata_observer_remote
                .InitWithNewPipeAndPassReceiver(),
            std::move(render_frame_metadata_observer_remote));

    host_->RegisterRenderFrameMetadataObserver(
        std::move(render_frame_metadata_observer_client_receiver),
        std::move(renderer_render_frame_metadata_observer_remote));
  }

  void TearDown() override {
    view_.reset();
    host_.reset();
    delegate_.reset();
    process_ = nullptr;
    browser_context_.reset();

#if defined(USE_AURA)
    display::Screen::SetScreenInstance(nullptr);
    screen_.reset();
#endif
#if defined(USE_AURA) || defined(OS_MACOSX)
    ImageTransportFactory::Terminate();
#endif
#if defined(OS_ANDROID)
    display::Screen::SetScreenInstance(nullptr);
#endif

    // Process all pending tasks to avoid leaks.
    base::RunLoop().RunUntilIdle();
  }

  virtual void ConfigureView(TestView* view) {
  }

  base::TimeTicks GetNextSimulatedEventTime() {
    last_simulated_event_time_ += simulated_event_time_delta_;
    return last_simulated_event_time_;
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type) {
    SimulateKeyboardEvent(type, 0);
  }

  void SimulateKeyboardEvent(WebInputEvent::Type type, int modifiers) {
    NativeWebKeyboardEvent native_event(type, modifiers,
                                        GetNextSimulatedEventTime());
    host_->ForwardKeyboardEvent(native_event);
  }

  void SimulateKeyboardEventWithCommands(WebInputEvent::Type type) {
    NativeWebKeyboardEvent native_event(type, 0, GetNextSimulatedEventTime());
    std::vector<blink::mojom::EditCommandPtr> edit_commands;
    edit_commands.push_back(blink::mojom::EditCommand::New("name", "value"));
    host_->ForwardKeyboardEventWithCommands(native_event, ui::LatencyInfo(),
                                            std::move(edit_commands), nullptr);
  }

  void SimulateMouseEvent(WebInputEvent::Type type) {
    host_->ForwardMouseEvent(SyntheticWebMouseEventBuilder::Build(type));
  }

  void SimulateMouseEventWithLatencyInfo(WebInputEvent::Type type,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardMouseEventWithLatencyInfo(
        SyntheticWebMouseEventBuilder::Build(type),
        ui_latency);
  }

  void SimulateWheelEvent(float dX, float dY, int modifiers, bool precise) {
    host_->ForwardWheelEvent(SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers,
        precise ? ui::ScrollGranularity::kScrollByPrecisePixel
                : ui::ScrollGranularity::kScrollByPixel));
  }

  void SimulateWheelEvent(float dX,
                          float dY,
                          int modifiers,
                          bool precise,
                          WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers,
        precise ? ui::ScrollGranularity::kScrollByPrecisePixel
                : ui::ScrollGranularity::kScrollByPixel);
    wheel_event.phase = phase;
    host_->ForwardWheelEvent(wheel_event);
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency) {
    host_->ForwardWheelEventWithLatencyInfo(
        SyntheticWebMouseWheelEventBuilder::Build(
            0, 0, dX, dY, modifiers,
            precise ? ui::ScrollGranularity::kScrollByPrecisePixel
                    : ui::ScrollGranularity::kScrollByPixel),
        ui_latency);
  }

  void SimulateWheelEventWithLatencyInfo(float dX,
                                         float dY,
                                         int modifiers,
                                         bool precise,
                                         const ui::LatencyInfo& ui_latency,
                                         WebMouseWheelEvent::Phase phase) {
    WebMouseWheelEvent wheel_event = SyntheticWebMouseWheelEventBuilder::Build(
        0, 0, dX, dY, modifiers,
        precise ? ui::ScrollGranularity::kScrollByPrecisePixel
                : ui::ScrollGranularity::kScrollByPixel);
    wheel_event.phase = phase;
    host_->ForwardWheelEventWithLatencyInfo(wheel_event, ui_latency);
  }

  void SimulateMouseMove(int x, int y, int modifiers) {
    SimulateMouseEvent(WebInputEvent::Type::kMouseMove, x, y, modifiers, false);
  }

  void SimulateMouseEvent(
      WebInputEvent::Type type, int x, int y, int modifiers, bool pressed) {
    WebMouseEvent event =
        SyntheticWebMouseEventBuilder::Build(type, x, y, modifiers);
    if (pressed)
      event.button = WebMouseEvent::Button::kLeft;
    event.SetTimeStamp(GetNextSimulatedEventTime());
    host_->ForwardMouseEvent(event);
  }

  // Inject simple synthetic WebGestureEvent instances.
  void SimulateGestureEvent(WebInputEvent::Type type,
                            WebGestureDevice sourceDevice) {
    host_->ForwardGestureEvent(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice));
  }

  void SimulateGestureEventWithLatencyInfo(WebInputEvent::Type type,
                                           WebGestureDevice sourceDevice,
                                           const ui::LatencyInfo& ui_latency) {
    host_->ForwardGestureEventWithLatencyInfo(
        SyntheticWebGestureEventBuilder::Build(type, sourceDevice), ui_latency);
  }

  // Set the timestamp for the touch-event.
  void SetTouchTimestamp(base::TimeTicks timestamp) {
    touch_event_.SetTimestamp(timestamp);
  }

  // Sends a touch event (irrespective of whether the page has a touch-event
  // handler or not).
  uint32_t SendTouchEvent() {
    uint32_t touch_event_id = touch_event_.unique_touch_event_id;
    host_->ForwardTouchEventWithLatencyInfo(touch_event_, ui::LatencyInfo());

    touch_event_.ResetPoints();
    return touch_event_id;
  }

  int PressTouchPoint(int x, int y) {
    return touch_event_.PressPoint(x, y);
  }

  void MoveTouchPoint(int index, int x, int y) {
    touch_event_.MovePoint(index, x, y);
  }

  void ReleaseTouchPoint(int index) {
    touch_event_.ReleasePoint(index);
  }

  const WebInputEvent* GetInputEventFromMessage(const IPC::Message& message) {
    base::PickleIterator iter(message);
    const char* data;
    int data_length;
    if (!iter.ReadData(&data, &data_length))
      return nullptr;
    return reinterpret_cast<const WebInputEvent*>(data);
  }

  BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<TestBrowserContext> browser_context_;
  RenderWidgetHostProcess* process_;  // Deleted automatically by the widget.
  std::unique_ptr<MockRenderWidgetHostDelegate> delegate_;
  testing::NiceMock<MockRenderWidgetHostOwnerDelegate> mock_owner_delegate_;
  std::unique_ptr<MockRenderWidgetHost> host_;
  std::unique_ptr<TestView> view_;
  std::unique_ptr<display::Screen> screen_;
  bool handle_key_press_event_;
  bool handle_mouse_event_;
  base::TimeTicks last_simulated_event_time_;
  base::TimeDelta simulated_event_time_delta_;
  IPC::TestSink* sink_;
  std::unique_ptr<FakeRenderFrameMetadataObserver>
      renderer_render_frame_metadata_observer_;

 private:
  SyntheticWebTouchEvent touch_event_;

  DISALLOW_COPY_AND_ASSIGN(RenderWidgetHostTest);
};

// RenderWidgetHostWithSourceTest ----------------------------------------------

// This is for tests that are to be run for all source devices.
class RenderWidgetHostWithSourceTest
    : public RenderWidgetHostTest,
      public testing::WithParamInterface<WebGestureDevice> {};

}  // namespace

// -----------------------------------------------------------------------------

TEST_F(RenderWidgetHostTest, SynchronizeVisualProperties) {
  sink_->ClearMessages();

  // The initial zoom is 0 so host should not send a sync message
  delegate_->SetZoomLevel(0);
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(0u, sink_->message_count());

  sink_->ClearMessages();

  // The zoom has changed so host should send out a sync message.
  double new_zoom_level = blink::PageZoomFactorToZoomLevel(0.25);
  delegate_->SetZoomLevel(new_zoom_level);
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_NEAR(new_zoom_level, host_->old_visual_properties_->zoom_level, 0.01);
  EXPECT_EQ(1u, sink_->message_count());

  sink_->ClearMessages();

  // The initial bounds is the empty rect, so setting it to the same thing
  // shouldn't send the resize message.
  view_->SetBounds(gfx::Rect());
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(0u, sink_->message_count());

  sink_->ClearMessages();

  // No visual properties ACK if the physical backing gets set, but the view
  // bounds are zero.
  view_->SetMockCompositorViewportPixelSize(gfx::Size(200, 200));
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(1u, sink_->message_count());

  sink_->ClearMessages();

  // Setting the view bounds to nonzero should send out the notification.
  // but should not expect ack for empty physical backing size.
  gfx::Rect original_size(0, 0, 100, 100);
  view_->SetBounds(original_size);
  view_->SetMockCompositorViewportPixelSize(gfx::Size());
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_visual_properties_->new_size);
  EXPECT_EQ(1u, sink_->message_count());

  sink_->ClearMessages();

  // Setting the bounds and physical backing size to nonzero should send out
  // the notification and expect an ack.
  view_->ClearMockCompositorViewportPixelSize();
  host_->SynchronizeVisualProperties();
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(original_size.size(), host_->old_visual_properties_->new_size);
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = original_size.size();
  metadata.local_surface_id_allocation = base::nullopt;
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(1u, sink_->message_count());

  sink_->ClearMessages();

  gfx::Rect second_size(0, 0, 110, 110);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  view_->SetBounds(second_size);
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  EXPECT_TRUE(host_->visual_properties_ack_pending_);

  sink_->ClearMessages();

  // Sending out a new notification should NOT send out a new IPC message since
  // a visual properties ACK is pending.
  gfx::Rect third_size(0, 0, 120, 120);
  process_->sink().ClearMessages();
  view_->SetBounds(third_size);
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(0u, sink_->message_count());

  sink_->ClearMessages();

  // Send a update that's a visual properties ACK, but for the original_size we
  // sent. Since this isn't the second_size, the message handler should
  // immediately send a new resize message for the new size to the renderer.
  metadata.viewport_size_in_pixels = original_size.size();
  metadata.local_surface_id_allocation = base::nullopt;
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_visual_properties_->new_size);
  EXPECT_EQ(1u, sink_->message_count());

  sink_->ClearMessages();

  // Send the visual properties ACK for the latest size.
  metadata.viewport_size_in_pixels = third_size.size();
  metadata.local_surface_id_allocation = base::nullopt;
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(third_size.size(), host_->old_visual_properties_->new_size);
  EXPECT_EQ(0u, sink_->message_count());

  sink_->ClearMessages();

  // Now clearing the bounds should send out a notification but we shouldn't
  // expect a visual properties ACK (since the renderer won't ack empty sizes).
  // The message should contain the new size (0x0) and not the previous one that
  // we skipped.
  view_->SetBounds(gfx::Rect());
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(), host_->old_visual_properties_->new_size);
  EXPECT_EQ(1u, sink_->message_count());

  sink_->ClearMessages();

  // Send a rect that has no area but has either width or height set.
  view_->SetBounds(gfx::Rect(0, 0, 0, 30));
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_visual_properties_->new_size);
  EXPECT_EQ(1u, sink_->message_count());

  sink_->ClearMessages();

  // Set the same size again. It should not be sent again.
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 30), host_->old_visual_properties_->new_size);
  EXPECT_EQ(0u, sink_->message_count());

  sink_->ClearMessages();

  // A different size should be sent again, however.
  view_->SetBounds(gfx::Rect(0, 0, 0, 31));
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(0, 31), host_->old_visual_properties_->new_size);
  EXPECT_EQ(1u, sink_->message_count());

  sink_->ClearMessages();

  // An invalid LocalSurfaceId should result in no change to the
  // |visual_properties_ack_pending_| bit.
  view_->SetBounds(gfx::Rect(25, 25));
  view_->InvalidateLocalSurfaceId();
  host_->SynchronizeVisualProperties();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
  EXPECT_EQ(gfx::Size(25, 25), host_->old_visual_properties_->new_size);
  EXPECT_EQ(1u, sink_->message_count());
}

// Test that a resize event is sent if SynchronizeVisualProperties() is called
// after a ScreenInfo change.
TEST_F(RenderWidgetHostTest, ResizeScreenInfo) {
  ScreenInfo screen_info;
  screen_info.device_scale_factor = 1.f;
  screen_info.rect = blink::WebRect(0, 0, 800, 600);
  screen_info.available_rect = blink::WebRect(0, 0, 800, 600);
  screen_info.orientation_angle = 0;
  screen_info.orientation_type = SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;

  sink_->ClearMessages();
  view_->SetScreenInfo(screen_info);
  EXPECT_EQ(0u, sink_->message_count());
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  // WidgetMsg_UpdateVisualProperties sent to the renderer.
  ASSERT_EQ(1u, sink_->message_count());
  EXPECT_EQ(WidgetMsg_UpdateVisualProperties::ID,
            sink_->GetMessageAt(0)->type());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);

  screen_info.orientation_angle = 180;
  screen_info.orientation_type = SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY;

  sink_->ClearMessages();
  view_->SetScreenInfo(screen_info);
  EXPECT_EQ(0u, sink_->message_count());
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  // WidgetMsg_UpdateVisualProperties sent to the renderer.
  ASSERT_EQ(1u, sink_->message_count());
  EXPECT_EQ(WidgetMsg_UpdateVisualProperties::ID,
            sink_->GetMessageAt(0)->type());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);

  screen_info.device_scale_factor = 2.f;

  sink_->ClearMessages();
  view_->SetScreenInfo(screen_info);
  EXPECT_EQ(0u, sink_->message_count());
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  // WidgetMsg_UpdateVisualProperties sent to the renderer.
  ASSERT_EQ(1u, sink_->message_count());
  EXPECT_EQ(WidgetMsg_UpdateVisualProperties::ID,
            sink_->GetMessageAt(0)->type());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);

  // No screen change.
  sink_->ClearMessages();
  view_->SetScreenInfo(screen_info);
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_EQ(0u, sink_->message_count());
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
}

// Tests that a resize event is sent when entering fullscreen mode, and the
// screen_info rects are overridden to match the view bounds.
TEST_F(RenderWidgetHostTest, OverrideScreenInfoDuringFullscreenMode) {
  const gfx::Rect kScreenBounds(0, 0, 800, 600);
  const gfx::Rect kViewBounds(55, 66, 600, 500);

  ScreenInfo screen_info;
  screen_info.device_scale_factor = 1.f;
  screen_info.rect = kScreenBounds;
  screen_info.available_rect = kScreenBounds;
  screen_info.orientation_angle = 0;
  screen_info.orientation_type = SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY;
  view_->SetScreenInfo(screen_info);

  sink_->ClearMessages();

  // Do initial VisualProperties sync while not fullscreened.
  view_->SetBounds(kViewBounds);
  ASSERT_FALSE(delegate_->IsFullscreenForCurrentTab());
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  // WidgetMsg_UpdateVisualProperties sent to the renderer.
  ASSERT_EQ(1u, sink_->message_count());
  WidgetMsg_UpdateVisualProperties::Param param;
  ASSERT_TRUE(
      WidgetMsg_UpdateVisualProperties::Read(sink_->GetMessageAt(0), &param));
  VisualProperties props = std::get<0>(param);
  EXPECT_EQ(kScreenBounds, props.screen_info.rect);
  EXPECT_EQ(kScreenBounds, props.screen_info.available_rect);

  // Enter fullscreen and do another VisualProperties sync.
  delegate_->set_is_fullscreen_for_current_tab(true);
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  // WidgetMsg_UpdateVisualProperties sent to the renderer.
  ASSERT_EQ(2u, sink_->message_count());
  ASSERT_TRUE(
      WidgetMsg_UpdateVisualProperties::Read(sink_->GetMessageAt(1), &param));
  props = std::get<0>(param);
  EXPECT_EQ(kViewBounds.size(), props.screen_info.rect.size());
  EXPECT_EQ(kViewBounds.size(), props.screen_info.available_rect.size());

  // Exit fullscreen and do another VisualProperties sync.
  delegate_->set_is_fullscreen_for_current_tab(false);
  host_->SynchronizeVisualPropertiesIgnoringPendingAck();
  // WidgetMsg_UpdateVisualProperties sent to the renderer.
  ASSERT_EQ(3u, sink_->message_count());
  ASSERT_TRUE(
      WidgetMsg_UpdateVisualProperties::Read(sink_->GetMessageAt(2), &param));
  props = std::get<0>(param);
  EXPECT_EQ(kScreenBounds, props.screen_info.rect);
  EXPECT_EQ(kScreenBounds, props.screen_info.available_rect);
}

TEST_F(RenderWidgetHostTest, ReceiveFrameTokenFromCrashedRenderer) {
  // The Renderer sends a monotonically increasing frame token.
  host_->DidProcessFrame(2);

  // Simulate a renderer crash.
  host_->SetView(nullptr);
  host_->RendererExited();

  // Receive an in-flight frame token (it needs to monotonically increase)
  // while the RenderWidget is gone.
  host_->DidProcessFrame(3);

  // The renderer is recreated.
  host_->SetView(view_.get());
  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  host_->Init();

  // The new RenderWidget sends a frame token, which is lower than what the
  // previous RenderWidget sent. This should be okay, as the expected token has
  // been reset.
  host_->DidProcessFrame(1);
}

TEST_F(RenderWidgetHostTest, ReceiveFrameTokenFromDeletedRenderWidget) {
  // The RenderWidget sends a monotonically increasing frame token.
  host_->DidProcessFrame(2);

  // The RenderWidget is destroyed in the renderer process as the main frame
  // is removed from this RenderWidgetHost's RenderWidgetView, but the
  // RenderWidgetView is still kept around for another RenderFrame.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(false));

  // Receive an in-flight frame token (it needs to monotonically increase)
  // while the RenderWidget is gone.
  host_->DidProcessFrame(3);

  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(true));

  // The new RenderWidget sends a frame token, which is lower than what the
  // previous RenderWidget sent. This should be okay, as the expected token has
  // been reset.
  host_->DidProcessFrame(1);
}

// Unable to include render_widget_host_view_mac.h and compile.
#if !defined(OS_MACOSX)
// Tests setting background transparency.
TEST_F(RenderWidgetHostTest, Background) {
  std::unique_ptr<RenderWidgetHostViewBase> view;
#if defined(USE_AURA)
  view.reset(new RenderWidgetHostViewAura(host_.get()));
  // TODO(derat): Call this on all platforms: http://crbug.com/102450.
  view->InitAsChild(nullptr);
#elif defined(OS_ANDROID)
  view.reset(new RenderWidgetHostViewAndroid(host_.get(), NULL));
#endif
  host_->SetView(view.get());

  ASSERT_FALSE(view->GetBackgroundColor());

  {
    // The background is assumed opaque by default, so choosing opaque won't
    // do anything if it's not set to transparent first.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(_)).Times(0);
    view->SetBackgroundColor(SK_ColorRED);
    EXPECT_EQ(unsigned{SK_ColorRED}, *view->GetBackgroundColor());
  }
  {
    // Another opaque color doesn't inform the view of any change.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(_)).Times(0);
    view->SetBackgroundColor(SK_ColorBLUE);
    EXPECT_EQ(unsigned{SK_ColorBLUE}, *view->GetBackgroundColor());
  }
  {
    // The owner delegate will be called to pass it over IPC to the RenderView.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(false));
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
    // The browser side will represent the background color as transparent
    // immediately.
    EXPECT_EQ(unsigned{SK_ColorTRANSPARENT}, *view->GetBackgroundColor());
  }
  {
    // Setting back an opaque color informs the view.
    EXPECT_CALL(mock_owner_delegate_, SetBackgroundOpaque(true));
    view->SetBackgroundColor(SK_ColorBLUE);
    EXPECT_EQ(unsigned{SK_ColorBLUE}, *view->GetBackgroundColor());
  }

  host_->SetView(nullptr);
  static_cast<RenderWidgetHostViewBase*>(view.release())->Destroy();
}
#endif

// Test that the RenderWidgetHost tells the renderer when it is hidden and
// shown, and can accept a racey update from the renderer after hiding.
TEST_F(RenderWidgetHostTest, HideShowMessages) {
  // Hide the widget, it should have sent out a message to the renderer.
  EXPECT_FALSE(host_->is_hidden_);
  host_->WasHidden();
  EXPECT_TRUE(host_->is_hidden_);
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(WidgetMsg_WasHidden::ID));

  // Send it an update as from the renderer.
  process_->sink().ClearMessages();
  cc::RenderFrameMetadata metadata;
  metadata.viewport_size_in_pixels = gfx::Size(100, 100);
  metadata.local_surface_id_allocation = base::nullopt;
  static_cast<RenderFrameMetadataProvider::Observer&>(*host_)
      .OnLocalSurfaceIdChanged(metadata);

  // Now unhide.
  process_->sink().ClearMessages();
  host_->WasShown(base::nullopt /* record_tab_switch_time_request */);
  EXPECT_FALSE(host_->is_hidden_);

  // It should have sent out a restored message.
  EXPECT_TRUE(
      process_->sink().GetUniqueMessageMatching(WidgetMsg_WasShown::ID));
}

TEST_F(RenderWidgetHostTest, IgnoreKeyEventsHandledByRenderer) {
  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  // Make sure we sent the input event to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
  EXPECT_FALSE(delegate_->unhandled_keyboard_event_called());
}

TEST_F(RenderWidgetHostTest, SendEditCommandsBeforeKeyEvent) {
  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::Type::kRawKeyDown);

  // Make sure we sent commands and key event to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(2u, dispatched_events.size());

  ASSERT_TRUE(dispatched_events[0]->ToEditCommand());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  // Send the simulated response from the renderer back.
  dispatched_events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
}

TEST_F(RenderWidgetHostTest, PreHandleRawKeyDownEvent) {
  // Simulate the situation that the browser handled the key down event during
  // pre-handle phrase.
  delegate_->set_prehandle_keyboard_event(true);

  // Simulate a keyboard event.
  SimulateKeyboardEventWithCommands(WebInputEvent::Type::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the commands and key event are not sent to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);

  // Make sure the Char event is suppressed.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::Type::kKeyUp);

  // Make sure the KeyUp event is suppressed.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());

  // Simulate a new RawKeyDown event.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  EXPECT_TRUE(delegate_->unhandled_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, RawKeyDownShortcutEvent) {
  // Simulate the situation that the browser marks the key down as a keyboard
  // shortcut, but doesn't consume it in the pre-handle phase.
  delegate_->set_prehandle_keyboard_event_is_shortcut(true);

  // Simulate a keyboard event.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  EXPECT_TRUE(delegate_->prehandle_keyboard_event_called());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            delegate_->prehandle_keyboard_event_type());

  // Make sure the RawKeyDown event is sent to the renderer.
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(WebInputEvent::Type::kRawKeyDown,
            delegate_->unhandled_keyboard_event_type());

  // The browser won't pre-handle a Char event.
  delegate_->set_prehandle_keyboard_event_is_shortcut(false);

  // Forward the Char event.
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);

  // The Char event is not suppressed; the renderer will ignore it
  // if the preceding RawKeyDown shortcut goes unhandled.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kChar,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(WebInputEvent::Type::kChar,
            delegate_->unhandled_keyboard_event_type());

  // Forward the KeyUp event.
  SimulateKeyboardEvent(WebInputEvent::Type::kKeyUp);

  // Make sure only KeyUp was sent to the renderer.
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kKeyUp,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);
  EXPECT_EQ(WebInputEvent::Type::kKeyUp,
            delegate_->unhandled_keyboard_event_type());
}

TEST_F(RenderWidgetHostTest, UnhandledWheelEvent) {
  SimulateWheelEvent(-5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  EXPECT_TRUE(delegate_->handle_wheel_event_called());
  EXPECT_EQ(1, view_->unhandled_wheel_event_count());
  EXPECT_EQ(-5, view_->unhandled_wheel_event().delta_x);
}

TEST_F(RenderWidgetHostTest, HandleWheelEvent) {
  // Indicate that we're going to handle this wheel event
  delegate_->set_handle_wheel_event(true);

  SimulateWheelEvent(-5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kMouseWheel,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  // ensure the wheel event handler was invoked
  EXPECT_TRUE(delegate_->handle_wheel_event_called());

  // and that it suppressed the unhandled wheel event handler.
  EXPECT_EQ(0, view_->unhandled_wheel_event_count());
}

TEST_F(RenderWidgetHostTest, EventsCausingFocus) {
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  EXPECT_EQ(1, delegate_->GetFocusOwningWebContentsCallCount());

  PressTouchPoint(0, 1);
  SendTouchEvent();
  EXPECT_EQ(2, delegate_->GetFocusOwningWebContentsCallCount());

  ReleaseTouchPoint(0);
  SendTouchEvent();
  EXPECT_EQ(2, delegate_->GetFocusOwningWebContentsCallCount());

  SimulateGestureEvent(WebInputEvent::Type::kGestureTapDown,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(2, delegate_->GetFocusOwningWebContentsCallCount());

  SimulateGestureEvent(WebInputEvent::Type::kGestureTap,
                       blink::WebGestureDevice::kTouchscreen);
  EXPECT_EQ(3, delegate_->GetFocusOwningWebContentsCallCount());
}

TEST_F(RenderWidgetHostTest, UnhandledGestureEvent) {
  SimulateGestureEvent(WebInputEvent::Type::kGestureTwoFingerTap,
                       blink::WebGestureDevice::kTouchscreen);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kGestureTwoFingerTap,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  EXPECT_EQ(WebInputEvent::Type::kGestureTwoFingerTap,
            view_->gesture_event_type());
  EXPECT_EQ(blink::mojom::InputEventResultState::kNotConsumed,
            view_->ack_result());
}

// Test that the hang monitor timer expires properly if a new timer is started
// while one is in progress (see crbug.com/11007).
TEST_F(RenderWidgetHostTest, DontPostponeInputEventAckTimeout) {
  base::TimeDelta delay =
      base::TimeDelta::FromMilliseconds(kHungRendererDelayMs);

  // Start a timeout.
  host_->StartInputEventAckTimeout();

  task_environment_.FastForwardBy(delay / 2);

  // Add another timeout.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartInputEventAckTimeout();

  // Wait long enough for first timeout and see if it fired.
  task_environment_.FastForwardBy(delay);
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer expires properly if it is started, stopped,
// and then started again.
TEST_F(RenderWidgetHostTest, StopAndStartInputEventAckTimeout) {
  // Start a timeout, then stop it.
  host_->StartInputEventAckTimeout();
  host_->StopInputEventAckTimeout();

  // Start it again to ensure it still works.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  host_->StartInputEventAckTimeout();

  // Wait long enough for first timeout and see if it fired.
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(kHungRendererDelayMs + 10));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor timer is effectively disabled when the widget is
// hidden.
TEST_F(RenderWidgetHostTest, InputEventAckTimeoutDisabledForInputWhenHidden) {
  SimulateMouseEvent(WebInputEvent::Type::kMouseMove, 10, 10, 0, false);

  // Hiding the widget should deactivate the timeout.
  host_->WasHidden();

  // The timeout should not fire.
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(kHungRendererDelayMs + 10));
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // The timeout should never reactivate while hidden.
  SimulateMouseEvent(WebInputEvent::Type::kMouseMove, 10, 10, 0, false);
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(kHungRendererDelayMs + 10));
  EXPECT_FALSE(delegate_->unresponsive_timer_fired());

  // Showing the widget should restore the timeout, as the events have
  // not yet been ack'ed.
  host_->WasShown(base::nullopt /* record_tab_switch_time_request */);
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(kHungRendererDelayMs + 10));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

// Test that the hang monitor catches two input events but only one ack.
// This can happen if the second input event causes the renderer to hang.
// This test will catch a regression of crbug.com/111185.
TEST_F(RenderWidgetHostTest, MultipleInputEvents) {
  // Send two events but only one ack.
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(kHungRendererDelayMs / 2));
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(2u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  // Send the simulated response from the renderer back.
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Wait long enough for second timeout and see if it fired.
  task_environment_.FastForwardBy(
      base::TimeDelta::FromMilliseconds(kHungRendererDelayMs + 10));
  EXPECT_TRUE(delegate_->unresponsive_timer_fired());
}

TEST_F(RenderWidgetHostTest, IgnoreInputEvent) {
  host_->SetupForInputRouterTest();

  delegate_->SetIgnoreInputEvents(true);

  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  SimulateMouseEvent(WebInputEvent::Type::kMouseMove);
  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  SimulateWheelEvent(0, 100, 0, true);
  EXPECT_FALSE(host_->mock_input_router()->sent_wheel_event_);

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchpad);
  EXPECT_FALSE(host_->mock_input_router()->sent_gesture_event_);

  PressTouchPoint(100, 100);
  SendTouchEvent();
  EXPECT_FALSE(host_->mock_input_router()->send_touch_event_not_cancelled_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerIgnoresEvent) {
  host_->SetupForInputRouterTest();
  host_->AddKeyPressEventCallback(base::BindRepeating(
      &RenderWidgetHostTest::KeyPressEventCallback, base::Unretained(this)));
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, KeyboardListenerSuppressFollowingEvents) {
  host_->SetupForInputRouterTest();

  host_->AddKeyPressEventCallback(base::BindRepeating(
      &RenderWidgetHostTest::KeyPressEventCallback, base::Unretained(this)));

  // The callback handles the first event
  handle_key_press_event_ = true;
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Following Char events should be suppressed
  handle_key_press_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);
  EXPECT_FALSE(host_->mock_input_router()->sent_keyboard_event_);

  // Sending RawKeyDown event should stop suppression
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);

  host_->mock_input_router()->sent_keyboard_event_ = false;
  SimulateKeyboardEvent(WebInputEvent::Type::kChar);
  EXPECT_TRUE(host_->mock_input_router()->sent_keyboard_event_);
}

TEST_F(RenderWidgetHostTest, MouseEventCallbackCanHandleEvent) {
  host_->SetupForInputRouterTest();

  host_->AddMouseEventCallback(base::BindRepeating(
      &RenderWidgetHostTest::MouseEventCallback, base::Unretained(this)));

  handle_mouse_event_ = true;
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);

  EXPECT_FALSE(host_->mock_input_router()->sent_mouse_event_);

  handle_mouse_event_ = false;
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);

  EXPECT_TRUE(host_->mock_input_router()->sent_mouse_event_);
}

TEST_F(RenderWidgetHostTest, InputRouterReceivesHasTouchEventHandlers) {
  host_->SetupForInputRouterTest();

  ASSERT_FALSE(host_->mock_input_router()->has_handlers_);

  host_->SetHasTouchEventHandlers(true);
  EXPECT_TRUE(host_->mock_input_router()->has_handlers_);
}

void CheckLatencyInfoComponentInMessage(
    MockWidgetInputHandler::MessageVector& dispatched_events,
    WebInputEvent::Type expected_type) {
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());

  EXPECT_TRUE(dispatched_events[0]->ToEvent()->Event()->web_event->GetType() ==
              expected_type);
  EXPECT_TRUE(
      dispatched_events[0]->ToEvent()->Event()->latency_info.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
}

void CheckLatencyInfoComponentInGestureScrollUpdate(
    MockWidgetInputHandler::MessageVector& dispatched_events) {
  ASSERT_EQ(2u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  ASSERT_TRUE(dispatched_events[1]->ToEvent());
  EXPECT_EQ(WebInputEvent::Type::kTouchScrollStarted,
            dispatched_events[0]->ToEvent()->Event()->web_event->GetType());

  EXPECT_EQ(WebInputEvent::Type::kGestureScrollUpdate,
            dispatched_events[1]->ToEvent()->Event()->web_event->GetType());
  EXPECT_TRUE(
      dispatched_events[1]->ToEvent()->Event()->latency_info.FindLatency(
          ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT, nullptr));
  dispatched_events[1]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);
}

// Tests that after input event passes through RWHI through ForwardXXXEvent()
// or ForwardXXXEventWithLatencyInfo(), LatencyInfo component
// ui::INPUT_EVENT_LATENCY_BEGIN_RWH_COMPONENT will always present in the
// event's LatencyInfo.
TEST_F(RenderWidgetHostTest, InputEventRWHLatencyComponent) {
  host_->SetHasTouchEventHandlers(true);

  // Tests RWHI::ForwardWheelEvent().
  SimulateWheelEvent(-5, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kMouseWheel);

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo(),
                                    WebMouseWheelEvent::kPhaseChanged);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kMouseWheel);

  // Tests RWHI::ForwardMouseEvent().
  SimulateMouseEvent(WebInputEvent::Type::kMouseMove);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kMouseMove);

  // Tests RWHI::ForwardMouseEventWithLatencyInfo().
  SimulateMouseEventWithLatencyInfo(WebInputEvent::Type::kMouseMove,
                                    ui::LatencyInfo());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kMouseMove);

  // Tests RWHI::ForwardGestureEvent().
  PressTouchPoint(0, 1);
  SendTouchEvent();
  host_->input_router()->OnSetTouchAction(cc::TouchAction::kAuto);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kTouchStart);

  SimulateGestureEvent(WebInputEvent::Type::kGestureScrollBegin,
                       blink::WebGestureDevice::kTouchscreen);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kGestureScrollBegin);

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::Type::kGestureScrollUpdate,
                                      blink::WebGestureDevice::kTouchscreen,
                                      ui::LatencyInfo());
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInGestureScrollUpdate(dispatched_events);

  ReleaseTouchPoint(0);
  SendTouchEvent();
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();

  // Tests RWHI::ForwardTouchEventWithLatencyInfo().
  PressTouchPoint(0, 1);
  SendTouchEvent();
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  CheckLatencyInfoComponentInMessage(dispatched_events,
                                     WebInputEvent::Type::kTouchStart);
}

TEST_F(RenderWidgetHostTest, RendererExitedResetsInputRouter) {
  EXPECT_EQ(0u, host_->in_flight_event_count());
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(1u, host_->in_flight_event_count());

  EXPECT_FALSE(host_->input_router()->HasPendingEvents());
  blink::WebMouseWheelEvent event;
  event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  host_->input_router()->SendWheelEvent(MouseWheelEventWithLatencyInfo(event));
  EXPECT_TRUE(host_->input_router()->HasPendingEvents());

  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->RendererExited();

  // The renderer is recreated.
  host_->SetView(view_.get());
  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  host_->Init();

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
  // There should be no in flight events. https://crbug.com/615090#152.
  EXPECT_EQ(0u, host_->in_flight_event_count());
}

TEST_F(RenderWidgetHostTest, DestroyingRenderWidgetResetsInputRouter) {
  EXPECT_EQ(0u, host_->in_flight_event_count());
  SimulateKeyboardEvent(WebInputEvent::Type::kRawKeyDown);
  EXPECT_EQ(1u, host_->in_flight_event_count());

  EXPECT_FALSE(host_->input_router()->HasPendingEvents());
  blink::WebMouseWheelEvent event;
  event.phase = blink::WebMouseWheelEvent::kPhaseBegan;
  host_->input_router()->SendWheelEvent(MouseWheelEventWithLatencyInfo(event));
  EXPECT_TRUE(host_->input_router()->HasPendingEvents());

  // The RenderWidget is destroyed in the renderer process as the main frame
  // is removed from this RenderWidgetHost's RenderWidgetView, but the
  // RenderWidgetView is still kept around for another RenderFrame.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(false));

  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(true));

  // Make sure the input router is in a fresh state.
  EXPECT_FALSE(host_->input_router()->HasPendingEvents());
  // There should be no in flight events. https://crbug.com/615090#152.
  EXPECT_EQ(0u, host_->in_flight_event_count());
}

TEST_F(RenderWidgetHostTest, RendererExitedResetsScreenRectsAck) {
  auto count_screen_rect_messages = [&]() {
    int count = 0;
    for (uint32_t i = 0; i < sink_->message_count(); ++i) {
      if (sink_->GetMessageAt(i)->type() == WidgetMsg_UpdateScreenRects::ID)
        ++count;
    }
    return count;
  };

  // Screen rects are sent during initialization, but we are waiting for an ack.
  EXPECT_EQ(1, count_screen_rect_messages());
  // Waiting for the ack prevents further sending.
  host_->SendScreenRects();
  host_->SendScreenRects();
  EXPECT_EQ(1, count_screen_rect_messages());

  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->RendererExited();

  // Still can't send until the RenderWidget is replaced.
  host_->SendScreenRects();
  EXPECT_EQ(1, count_screen_rect_messages());

  // The renderer is recreated.
  host_->SetView(view_.get());
  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  host_->Init();

  // The RenderWidget is shown when navigation completes. This sends screen
  // rects again. The IPC is sent as it's not waiting for an ack.
  host_->WasShown(base::nullopt);
  EXPECT_EQ(2, count_screen_rect_messages());
}

TEST_F(RenderWidgetHostTest, DestroyingRenderWidgetResetsScreenRectsAck) {
  auto count_screen_rect_messages = [&]() {
    int count = 0;
    for (uint32_t i = 0; i < sink_->message_count(); ++i) {
      if (sink_->GetMessageAt(i)->type() == WidgetMsg_UpdateScreenRects::ID)
        ++count;
    }
    return count;
  };

  // Screen rects are sent during initialization, but we are waiting for an ack.
  EXPECT_EQ(1, count_screen_rect_messages());
  // Waiting for the ack prevents further sending.
  host_->SendScreenRects();
  host_->SendScreenRects();
  EXPECT_EQ(1, count_screen_rect_messages());

  // The RenderWidget has been destroyed in the renderer.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(false));

  // Still can't send until the RenderWidget is replaced.
  host_->SendScreenRects();
  EXPECT_EQ(1, count_screen_rect_messages());

  // Make a new RenderWidget when the renderer is recreated and inform that a
  // RenderWidget is being created.
  VisualProperties props = host_->GetInitialVisualProperties();
  // The RenderWidget is recreated with the initial VisualProperties.
  EXPECT_CALL(mock_owner_delegate_, IsMainFrameActive())
      .WillRepeatedly(Return(true));

  // We are able to send screen rects again. The IPC is sent as it's not waiting
  // for an ack.
  host_->SendScreenRects();
  EXPECT_EQ(2, count_screen_rect_messages());
}

// Regression test for http://crbug.com/401859 and http://crbug.com/522795.
TEST_F(RenderWidgetHostTest, RendererExitedResetsIsHidden) {
  // RendererExited will delete the view.
  host_->SetView(new TestView(host_.get()));
  host_->WasShown(base::nullopt /* record_tab_switch_time_request */);

  ASSERT_FALSE(host_->is_hidden());
  host_->RendererExited();
  ASSERT_TRUE(host_->is_hidden());

  // Make sure the input router is in a fresh state.
  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

TEST_F(RenderWidgetHostTest, VisualProperties) {
  gfx::Rect bounds(0, 0, 100, 100);
  gfx::Rect compositor_viewport_pixel_rect(40, 50);
  view_->SetBounds(bounds);
  view_->SetMockCompositorViewportPixelSize(
      compositor_viewport_pixel_rect.size());

  VisualProperties visual_properties = host_->GetVisualProperties();
  EXPECT_EQ(bounds.size(), visual_properties.new_size);
  EXPECT_EQ(compositor_viewport_pixel_rect,
            visual_properties.compositor_viewport_pixel_rect);
}

// Make sure no dragging occurs after renderer exited. See crbug.com/704832.
TEST_F(RenderWidgetHostTest, RendererExitedNoDrag) {
  host_->SetView(new TestView(host_.get()));

  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 0);

  GURL http_url = GURL("http://www.domain.com/index.html");
  DropData drop_data;
  drop_data.url = http_url;
  drop_data.html_base_url = http_url;
  blink::WebDragOperationsMask drag_operation = blink::kWebDragOperationEvery;
  DragEventSourceInfo event_info;
  host_->OnStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Vector2d(),
                         event_info);
  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 1);

  // Simulate that renderer exited due navigation to the next page.
  host_->RendererExited();
  EXPECT_FALSE(host_->GetView());
  host_->OnStartDragging(drop_data, drag_operation, SkBitmap(), gfx::Vector2d(),
                         event_info);
  EXPECT_EQ(delegate_->mock_delegate_view()->start_dragging_count(), 1);
}

class RenderWidgetHostInitialSizeTest : public RenderWidgetHostTest {
 public:
  RenderWidgetHostInitialSizeTest()
      : RenderWidgetHostTest(), initial_size_(200, 100) {}

  void ConfigureView(TestView* view) override {
    view->SetBounds(gfx::Rect(initial_size_));
  }

 protected:
  gfx::Size initial_size_;
};

TEST_F(RenderWidgetHostInitialSizeTest, InitialSize) {
  // Having an initial size set means that the size information had been sent
  // with the reqiest to new up the RenderView and so subsequent
  // SynchronizeVisualProperties calls should not result in new IPC (unless the
  // size has actually changed).
  EXPECT_CALL(mock_owner_delegate_, UpdatePageVisualProperties(_)).Times(0);
  EXPECT_FALSE(host_->SynchronizeVisualProperties());
  EXPECT_EQ(initial_size_, host_->old_visual_properties_->new_size);
  EXPECT_TRUE(host_->visual_properties_ack_pending_);
}

TEST_F(RenderWidgetHostTest, HideUnthrottlesResize) {
  sink_->ClearMessages();
  view_->SetBounds(gfx::Rect(100, 100));
  EXPECT_TRUE(host_->SynchronizeVisualProperties());
  // WidgetMsg_UpdateVisualProperties is sent to the renderer.
  ASSERT_EQ(1u, sink_->message_count());
  {
    const IPC::Message* msg = sink_->GetMessageAt(0);
    ASSERT_EQ(WidgetMsg_UpdateVisualProperties::ID, msg->type());
    WidgetMsg_UpdateVisualProperties::Param params;
    WidgetMsg_UpdateVisualProperties::Read(msg, &params);
    VisualProperties visual_properties = std::get<0>(params);
    // Size sent to the renderer.
    EXPECT_EQ(gfx::Size(100, 100), visual_properties.new_size);
  }
  // An ack is pending, throttling further updates.
  EXPECT_TRUE(host_->visual_properties_ack_pending_);

  // Hiding the widget should unthrottle resize.
  host_->WasHidden();
  EXPECT_FALSE(host_->visual_properties_ack_pending_);
}

// Tests that event dispatch after the delegate has been detached doesn't cause
// a crash. See crbug.com/563237.
TEST_F(RenderWidgetHostTest, EventDispatchPostDetach) {
  host_->SetHasTouchEventHandlers(true);
  process_->sink().ClearMessages();

  host_->DetachDelegate();

  // Tests RWHI::ForwardGestureEventWithLatencyInfo().
  SimulateGestureEventWithLatencyInfo(WebInputEvent::Type::kGestureScrollUpdate,
                                      blink::WebGestureDevice::kTouchscreen,
                                      ui::LatencyInfo());

  // Tests RWHI::ForwardWheelEventWithLatencyInfo().
  SimulateWheelEventWithLatencyInfo(-5, 0, 0, true, ui::LatencyInfo());

  ASSERT_FALSE(host_->input_router()->HasPendingEvents());
}

// Check that if messages of a frame arrive earlier than the frame itself, we
// queue the messages until the frame arrives and then process them.
TEST_F(RenderWidgetHostTest, FrameToken_MessageThenFrame) {
  constexpr uint32_t frame_token = 1;
  std::vector<IPC::Message> messages;
  messages.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token, messages));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  view_->OnFrameTokenChanged(frame_token);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());
}

// Check that if a frame arrives earlier than its messages, we process the
// messages immedtiately.
TEST_F(RenderWidgetHostTest, FrameToken_FrameThenMessage) {
  constexpr uint32_t frame_token = 1;
  std::vector<IPC::Message> messages;
  messages.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  view_->OnFrameTokenChanged(frame_token);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token, messages));
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());
}

// Check that if messages of multiple frames arrive before the frames, we
// process each message once it frame arrives.
TEST_F(RenderWidgetHostTest, FrameToken_MultipleMessagesThenTokens) {
  constexpr uint32_t frame_token1 = 1;
  constexpr uint32_t frame_token2 = 2;
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages2;
  messages1.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages2.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token2, messages2));
  EXPECT_EQ(2u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  view_->OnFrameTokenChanged(frame_token1);
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());
  view_->OnFrameTokenChanged(frame_token2);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(2u, host_->processed_frame_messages_count());
}

// Check that if multiple frames arrive before their messages, each message is
// processed immediately as soon as it arrives.
TEST_F(RenderWidgetHostTest, FrameToken_MultipleTokensThenMessages) {
  constexpr uint32_t frame_token1 = 1;
  constexpr uint32_t frame_token2 = 2;
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages2;
  messages1.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages2.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  view_->OnFrameTokenChanged(frame_token1);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  view_->OnFrameTokenChanged(frame_token2);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(1u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token2, messages2));
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(2u, host_->processed_frame_messages_count());
}

// Check that if one frame is lost but its messages arrive, we process the
// messages on the arrival of the next frame.
TEST_F(RenderWidgetHostTest, FrameToken_DroppedFrame) {
  constexpr uint32_t frame_token1 = 1;
  constexpr uint32_t frame_token2 = 2;
  std::vector<IPC::Message> messages1;
  std::vector<IPC::Message> messages2;
  messages1.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(5));
  messages2.push_back(WidgetHostMsg_DidFirstVisuallyNonEmptyPaint(6));

  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token1, messages1));
  EXPECT_EQ(1u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  host_->OnMessageReceived(
      WidgetHostMsg_FrameSwapMessages(0, frame_token2, messages2));
  EXPECT_EQ(2u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(0u, host_->processed_frame_messages_count());

  view_->OnFrameTokenChanged(frame_token2);
  EXPECT_EQ(0u, host_->frame_token_message_queue_->size());
  EXPECT_EQ(2u, host_->processed_frame_messages_count());
}

TEST_F(RenderWidgetHostTest, ForceEnableZoomShouldUpdateAfterRebind) {
  SCOPED_TRACE("force_enable_zoom is false at start.");
  host_->ExpectForceEnableZoom(false);

  // Set force_enable_zoom true.
  host_->SetForceEnableZoom(true);

  SCOPED_TRACE("force_enable_zoom is true after set.");
  host_->ExpectForceEnableZoom(true);

  // Rebind should also update to the latest force_enable_zoom state.
  mojo::PendingRemote<mojom::Widget> widget;
  std::unique_ptr<MockWidgetImpl> widget_impl =
      std::make_unique<MockWidgetImpl>(widget.InitWithNewPipeAndPassReceiver());
  host_->SetWidget(std::move(widget));

  SCOPED_TRACE("force_enable_zoom is true after rebind.");
  host_->ExpectForceEnableZoom(true);
}

// If a navigation happens while the widget is hidden, we shouldn't show
// contents of the previous page when we become visible.
TEST_F(RenderWidgetHostTest, NavigateInBackgroundShowsBlank) {
  // When visible, navigation does not immediately call into
  // ClearDisplayedGraphics.
  host_->WasShown(base::nullopt /* record_tab_switch_time_request */);
  host_->DidNavigate();
  EXPECT_FALSE(host_->new_content_rendering_timeout_fired());

  // Hide then show. ClearDisplayedGraphics must be called.
  host_->WasHidden();
  host_->WasShown(base::nullopt /* record_tab_switch_time_request */);
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
  host_->reset_new_content_rendering_timeout_fired();

  // Hide, navigate, then show. ClearDisplayedGraphics must be called.
  host_->WasHidden();
  host_->DidNavigate();
  host_->WasShown(base::nullopt /* record_tab_switch_time_request */);
  EXPECT_TRUE(host_->new_content_rendering_timeout_fired());
}

TEST_F(RenderWidgetHostTest, RendererHangRecordsMetrics) {
  base::SimpleTestTickClock clock;
  host_->set_clock_for_testing(&clock);
  base::HistogramTester tester;

  // RenderWidgetHost makes private the methods it overrides from
  // InputRouterClient. Call them through the base class.
  InputRouterClient* input_router_client = host_.get();

  // Do a 3s hang. This shouldn't affect metrics.
  input_router_client->IncrementInFlightEventCount();
  clock.Advance(base::TimeDelta::FromSeconds(3));
  input_router_client->DecrementInFlightEventCount(
      blink::mojom::InputEventResultSource::kUnknown);
  tester.ExpectTotalCount("Renderer.Hung.Duration", 0u);

  // Do a 17s hang. This should affect metrics.
  input_router_client->IncrementInFlightEventCount();
  clock.Advance(base::TimeDelta::FromSeconds(17));
  input_router_client->DecrementInFlightEventCount(
      blink::mojom::InputEventResultSource::kUnknown);
  tester.ExpectTotalCount("Renderer.Hung.Duration", 1u);
  tester.ExpectUniqueSample("Renderer.Hung.Duration", 17000, 1);
}

TEST_F(RenderWidgetHostTest, PendingUserActivationTimeout) {
  base::test::ScopedFeatureList scoped_feature_list_;
  scoped_feature_list_.InitWithFeatures(
      {features::kBrowserVerifiedUserActivationMouse,
       features::kBrowserVerifiedUserActivationKeyboard},
      {});

  // One event allows one activation notification.
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  EXPECT_TRUE(host_->RemovePendingUserActivationIfAvailable());
  EXPECT_FALSE(host_->RemovePendingUserActivationIfAvailable());

  // Mouse move and up does not increase pending user activation counter.
  SimulateMouseEvent(WebInputEvent::Type::kMouseMove);
  SimulateMouseEvent(WebInputEvent::Type::kMouseUp);
  EXPECT_FALSE(host_->RemovePendingUserActivationIfAvailable());

  // 2 events allow 2 activation notifications.
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  SimulateKeyboardEvent(WebInputEvent::Type::kKeyDown);
  EXPECT_TRUE(host_->RemovePendingUserActivationIfAvailable());
  EXPECT_TRUE(host_->RemovePendingUserActivationIfAvailable());
  EXPECT_FALSE(host_->RemovePendingUserActivationIfAvailable());

  // Timer reset the pending activation.
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  SimulateMouseEvent(WebInputEvent::Type::kMouseDown);
  task_environment_.FastForwardBy(
      RenderWidgetHostImpl::kActivationNotificationExpireTime);
  EXPECT_FALSE(host_->RemovePendingUserActivationIfAvailable());
}

// Tests that fling events are not dispatched when the wheel event is consumed.
TEST_F(RenderWidgetHostTest, NoFlingEventsWhenLastScrollEventConsumed) {
  // Simulate a consumed wheel event.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // A GestureFlingStart event following a consumed scroll event should not be
  // dispatched.
  SimulateGestureEvent(blink::WebInputEvent::Type::kGestureFlingStart,
                       blink::WebGestureDevice::kTouchpad);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_EQ(0u, dispatched_events.size());
}

// Tests that fling events are dispatched when some, but not all, scroll events
// were consumed.
TEST_F(RenderWidgetHostTest, FlingEventsWhenSomeScrollEventsConsumed) {
  // Simulate a consumed wheel event.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseBegan);
  MockWidgetInputHandler::MessageVector dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kConsumed);

  // Followed by a not consumed wheel event.
  SimulateWheelEvent(10, 0, 0, true, WebMouseWheelEvent::kPhaseChanged);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  ASSERT_EQ(1u, dispatched_events.size());
  ASSERT_TRUE(dispatched_events[0]->ToEvent());
  dispatched_events[0]->ToEvent()->CallCallback(
      blink::mojom::InputEventResultState::kNotConsumed);

  // A GestureFlingStart event following the scroll events should be dispatched.
  SimulateGestureEvent(blink::WebInputEvent::Type::kGestureFlingStart,
                       blink::WebGestureDevice::kTouchpad);
  dispatched_events =
      host_->mock_widget_input_handler_.GetAndResetDispatchedMessages();
  EXPECT_NE(0u, dispatched_events.size());
}

TEST_F(RenderWidgetHostTest, AddAndRemoveInputEventObserver) {
  MockInputEventObserver observer;

  // Add InputEventObserver.
  host_->AddInputEventObserver(&observer);

  // Confirm OnInputEvent is triggered.
  NativeWebKeyboardEvent native_event(WebInputEvent::Type::kChar, 0,
                                      GetNextSimulatedEventTime());
  ui::LatencyInfo latency_info = ui::LatencyInfo();
  EXPECT_CALL(observer, OnInputEvent(_)).Times(1);
  host_->DispatchInputEventWithLatencyInfo(native_event, &latency_info);

  // Remove InputEventObserver.
  host_->RemoveInputEventObserver(&observer);

  // Confirm InputEventObserver is removed.
  EXPECT_CALL(observer, OnInputEvent(_)).Times(0);
  latency_info = ui::LatencyInfo();
  host_->DispatchInputEventWithLatencyInfo(native_event, &latency_info);
}

#if defined(OS_ANDROID)
TEST_F(RenderWidgetHostTest, AddAndRemoveImeInputEventObserver) {
  MockInputEventObserver observer;

  // Add ImeInputEventObserver.
  host_->AddImeInputEventObserver(&observer);

  // Confirm ImeFinishComposingTextEvent is triggered.
  EXPECT_CALL(observer, OnImeFinishComposingTextEvent()).Times(1);
  host_->ImeFinishComposingText(true);

  // Remove ImeInputEventObserver.
  host_->RemoveImeInputEventObserver(&observer);

  // Confirm ImeInputEventObserver is removed.
  EXPECT_CALL(observer, OnImeFinishComposingTextEvent()).Times(0);
  host_->ImeFinishComposingText(true);
}
#endif

// Tests that vertical scroll direction changes are propagated to the delegate.
TEST_F(RenderWidgetHostTest, OnVerticalScrollDirectionChanged) {
  const auto NotifyVerticalScrollDirectionChanged =
      [this](viz::VerticalScrollDirection scroll_direction) {
        static uint32_t frame_token = 1u;
        host_->frame_token_message_queue_->DidProcessFrame(frame_token);

        cc::RenderFrameMetadata metadata;
        metadata.new_vertical_scroll_direction = scroll_direction;
        static_cast<mojom::RenderFrameMetadataObserverClient*>(
            host_->render_frame_metadata_provider())
            ->OnRenderFrameMetadataChanged(frame_token++, metadata);
      };

  // Verify initial state.
  EXPECT_EQ(0, delegate_->GetOnVerticalScrollDirectionChangedCallCount());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            delegate_->GetLastVerticalScrollDirection());

  // Verify that we will *not* propagate a vertical scroll of |kNull| which is
  // only used to indicate the absence of a change in vertical scroll direction.
  NotifyVerticalScrollDirectionChanged(viz::VerticalScrollDirection::kNull);
  EXPECT_EQ(0, delegate_->GetOnVerticalScrollDirectionChangedCallCount());
  EXPECT_EQ(viz::VerticalScrollDirection::kNull,
            delegate_->GetLastVerticalScrollDirection());

  // Verify that we will propagate a vertical scroll |kUp|.
  NotifyVerticalScrollDirectionChanged(viz::VerticalScrollDirection::kUp);
  EXPECT_EQ(1, delegate_->GetOnVerticalScrollDirectionChangedCallCount());
  EXPECT_EQ(viz::VerticalScrollDirection::kUp,
            delegate_->GetLastVerticalScrollDirection());

  // Verify that we will propagate a vertical scroll |kDown|.
  NotifyVerticalScrollDirectionChanged(viz::VerticalScrollDirection::kDown);
  EXPECT_EQ(2, delegate_->GetOnVerticalScrollDirectionChangedCallCount());
  EXPECT_EQ(viz::VerticalScrollDirection::kDown,
            delegate_->GetLastVerticalScrollDirection());
}

}  // namespace content
