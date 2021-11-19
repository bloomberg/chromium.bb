// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdf_view_web_plugin.h"

#include <memory>
#include <string>
#include <utility>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/strings/string_piece.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "cc/paint/paint_canvas.h"
#include "cc/test/pixel_comparator.h"
#include "cc/test/pixel_test_utils.h"
#include "pdf/ppapi_migration/bitmap.h"
#include "pdf/test/test_helpers.h"
#include "pdf/test/test_pdfium_engine.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_coalesced_input_event.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_keyboard_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/blink/public/common/input/web_pointer_properties.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_text_input_type.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_associated_url_loader.h"
#include "third_party/blink/public/web/web_plugin_container.h"
#include "third_party/blink/public/web/web_plugin_params.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/cursor/cursor.h"
#include "ui/events/blink/blink_event_util.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/skia_conversions.h"
#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/latency/latency_info.h"

namespace chrome_pdf {

namespace {

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Pointwise;
using ::testing::Return;

// `kCanvasSize` needs to be big enough to hold plugin's snapshots during
// testing.
constexpr gfx::Size kCanvasSize(100, 100);

// A common device scale for high DPI displays.
constexpr float kDeviceScale = 2.0f;

// Note: Make sure `kDefaultColor` is different from `kPaintColor` and the
// plugin's background color. This will help identify bitmap changes after
// painting.
constexpr SkColor kDefaultColor = SK_ColorGREEN;

constexpr SkColor kPaintColor = SK_ColorRED;

struct PaintParams {
  // The plugin container's device scale.
  float device_scale;

  // The window area in CSS pixels.
  gfx::Rect window_rect;

  // The target painting area on the canvas in CSS pixels.
  gfx::Rect paint_rect;

  // The expected clipped area to be filled with paint color. The clipped area
  // should be the intersection of `paint_rect` and `window_rect`.
  gfx::Rect expected_clipped_rect;
};

MATCHER(SearchStringResultEq, "") {
  PDFEngine::Client::SearchStringResult l = std::get<0>(arg);
  PDFEngine::Client::SearchStringResult r = std::get<1>(arg);
  return l.start_index == r.start_index && l.length == r.length;
}

MATCHER_P(IsExpectedImeKeyEvent, expected_text, "") {
  if (arg.GetType() != blink::WebInputEvent::Type::kChar)
    return false;

  const auto& event = static_cast<const blink::WebKeyboardEvent&>(arg);
  return event.GetModifiers() == blink::WebInputEvent::kNoModifiers &&
         event.windows_key_code == expected_text[0] &&
         event.native_key_code == expected_text[0] &&
         event.dom_code == static_cast<int>(ui::DomCode::NONE) &&
         event.dom_key == ui::DomKey::NONE && !event.is_system_key &&
         !event.is_browser_shortcut && event.text == expected_text &&
         event.unmodified_text == expected_text;
}

// Generates the expected `SkBitmap` with `paint_color` filled in the expected
// clipped area and `kDefaultColor` as the background color.
SkBitmap GenerateExpectedBitmapForPaint(const gfx::Rect& expected_clipped_rect,
                                        SkColor paint_color) {
  SkBitmap expected_bitmap =
      CreateN32PremulSkBitmap(gfx::SizeToSkISize(kCanvasSize));
  expected_bitmap.eraseColor(kDefaultColor);
  expected_bitmap.erase(paint_color, gfx::RectToSkIRect(expected_clipped_rect));
  return expected_bitmap;
}

blink::WebMouseEvent CreateDefaultMouseDownEvent() {
  blink::WebMouseEvent web_event(
      blink::WebInputEvent::Type::kMouseDown,
      /*position=*/gfx::PointF(),
      /*global_position=*/gfx::PointF(),
      blink::WebPointerProperties::Button::kLeft,
      /*click_count_param=*/1, blink::WebInputEvent::Modifiers::kLeftButtonDown,
      blink::WebInputEvent::GetStaticTimeStampForTests());
  web_event.SetFrameScale(1);
  return web_event;
}

class FakeContainerWrapper : public PdfViewWebPlugin::ContainerWrapper {
 public:
  explicit FakeContainerWrapper(PdfViewWebPlugin* web_plugin)
      : web_plugin_(web_plugin) {
    ON_CALL(*this, UpdateTextInputState)
        .WillByDefault(Invoke(
            this, &FakeContainerWrapper::UpdateTextInputStateFromPlugin));

    UpdateTextInputStateFromPlugin();
  }

  FakeContainerWrapper(const FakeContainerWrapper&) = delete;
  FakeContainerWrapper& operator=(const FakeContainerWrapper&) = delete;
  ~FakeContainerWrapper() override = default;

  // PdfViewWebPlugin::ContainerWrapper:
  void Invalidate() override {}

  MOCK_METHOD(void,
              RequestTouchEventType,
              (blink::WebPluginContainer::TouchEventRequestType),
              (override));

  MOCK_METHOD(void, ReportFindInPageMatchCount, (int, int, bool), (override));

  MOCK_METHOD(void, ReportFindInPageSelection, (int, int), (override));

  float DeviceScaleFactor() override { return device_scale_; }

  MOCK_METHOD(void,
              SetReferrerForRequest,
              (blink::WebURLRequest&, const blink::WebURL&),
              (override));

  MOCK_METHOD(void, Alert, (const blink::WebString&), (override));

  MOCK_METHOD(bool, Confirm, (const blink::WebString&), (override));

  MOCK_METHOD(blink::WebString,
              Prompt,
              (const blink::WebString&, const blink::WebString&),
              (override));

  MOCK_METHOD(void,
              TextSelectionChanged,
              (const blink::WebString&, uint32_t, const gfx::Range&),
              (override));

  MOCK_METHOD(std::unique_ptr<blink::WebAssociatedURLLoader>,
              CreateAssociatedURLLoader,
              (const blink::WebAssociatedURLLoaderOptions&),
              (override));

  MOCK_METHOD(void, UpdateTextInputState, (), (override));

  MOCK_METHOD(void, UpdateSelectionBounds, (), (override));

  std::string GetEmbedderOriginString() override {
    return "chrome-extension://mhjfbmdgcfjbbpaeojofohoefgiehjai/";
  }

  blink::WebLocalFrame* GetFrame() override { return nullptr; }

  blink::WebLocalFrameClient* GetWebLocalFrameClient() override {
    return nullptr;
  }

  // TODO(https://crbug.com/1207575): Container() should not be used for testing
  // since it doesn't have a valid blink::WebPluginContainer. Make this method
  // fail once ContainerWrapper instead of blink::WebPluginContainer is used for
  // initializing `PostMessageSender`.
  blink::WebPluginContainer* Container() override { return nullptr; }

  blink::WebTextInputType widget_text_input_type() const {
    return widget_text_input_type_;
  }

  void set_device_scale(float device_scale) { device_scale_ = device_scale; }

 private:
  void UpdateTextInputStateFromPlugin() {
    widget_text_input_type_ = web_plugin_->GetPluginTextInputType();
  }

  float device_scale_ = 1.0f;

  // Represents the frame widget's text input type.
  blink::WebTextInputType widget_text_input_type_;

  PdfViewWebPlugin* web_plugin_;
};

class FakePdfViewWebPluginClient : public PdfViewWebPlugin::Client {
 public:
  FakePdfViewWebPluginClient() = default;
  FakePdfViewWebPluginClient(const FakePdfViewWebPluginClient&) = delete;
  FakePdfViewWebPluginClient& operator=(const FakePdfViewWebPluginClient&) =
      delete;
  ~FakePdfViewWebPluginClient() override = default;

  // PdfViewWebPlugin::Client:
  MOCK_METHOD(bool, IsUseZoomForDSFEnabled, (), (const, override));
};

}  // namespace

class PdfViewWebPluginWithoutInitializeTest : public testing::Test {
 public:
  PdfViewWebPluginWithoutInitializeTest(
      const PdfViewWebPluginWithoutInitializeTest&) = delete;
  PdfViewWebPluginWithoutInitializeTest& operator=(
      const PdfViewWebPluginWithoutInitializeTest&) = delete;

 protected:
  // Custom deleter for `plugin_`. PdfViewWebPlugin must be destroyed by
  // PdfViewWebPlugin::Destroy() instead of its destructor.
  struct PluginDeleter {
    void operator()(PdfViewWebPlugin* ptr) { ptr->Destroy(); }
  };

  PdfViewWebPluginWithoutInitializeTest() = default;
  ~PdfViewWebPluginWithoutInitializeTest() override = default;

  void SetUp() override {
    // Set a dummy URL for initializing the plugin.
    blink::WebPluginParams params;
    params.attribute_names.push_back(blink::WebString("src"));
    params.attribute_values.push_back(blink::WebString("dummy.pdf"));

    auto client = std::make_unique<NiceMock<FakePdfViewWebPluginClient>>();
    client_ptr_ = client.get();

    mojo::AssociatedRemote<pdf::mojom::PdfService> unbound_remote;
    plugin_ =
        std::unique_ptr<PdfViewWebPlugin, PluginDeleter>(new PdfViewWebPlugin(
            std::move(client), std::move(unbound_remote), params));
  }

  void TearDown() override { plugin_.reset(); }

  FakePdfViewWebPluginClient* client_ptr_;
  std::unique_ptr<PdfViewWebPlugin, PluginDeleter> plugin_;
};

class PdfViewWebPluginTest : public PdfViewWebPluginWithoutInitializeTest {
 protected:
  void SetUp() override {
    PdfViewWebPluginWithoutInitializeTest::SetUp();

    auto wrapper =
        std::make_unique<NiceMock<FakeContainerWrapper>>(plugin_.get());
    wrapper_ptr_ = wrapper.get();
    auto engine = CreateEngine();
    engine_ptr_ = engine.get();
    EXPECT_TRUE(
        plugin_->InitializeForTesting(std::move(wrapper), std::move(engine)));
  }

  void TearDown() override {
    wrapper_ptr_ = nullptr;

    PdfViewWebPluginWithoutInitializeTest::TearDown();
  }

  // Allow derived test classes to create their own custom TestPDFiumEngine.
  virtual std::unique_ptr<TestPDFiumEngine> CreateEngine() {
    return std::make_unique<NiceMock<TestPDFiumEngine>>(plugin_.get());
  }

  void UpdatePluginGeometry(float device_scale, const gfx::Rect& window_rect) {
    UpdatePluginGeometryWithoutWaiting(device_scale, window_rect);

    // Waits for main thread callback scheduled by `PaintManager`.
    base::RunLoop run_loop;
    plugin_->ScheduleTaskOnMainThread(
        FROM_HERE, base::BindLambdaForTesting([&run_loop](int32_t /*result*/) {
          run_loop.Quit();
        }),
        /*result=*/0, base::TimeDelta());
    run_loop.Run();
  }

  void UpdatePluginGeometryWithoutWaiting(float device_scale,
                                          const gfx::Rect& window_rect) {
    // The plugin container's device scale must be set before calling
    // UpdateGeometry().
    ASSERT_TRUE(wrapper_ptr_);
    wrapper_ptr_->set_device_scale(device_scale);
    plugin_->UpdateGeometry(window_rect, window_rect, window_rect,
                            /*is_visible=*/true);
  }

  void TestUpdateGeometrySetsPluginRect(float device_scale,
                                        const gfx::Rect& window_rect,
                                        float expected_device_scale,
                                        const gfx::Rect& expected_plugin_rect) {
    UpdatePluginGeometryWithoutWaiting(device_scale, window_rect);
    EXPECT_EQ(expected_device_scale, plugin_->GetDeviceScaleForTesting())
        << "Device scale comparison failure at device scale of "
        << device_scale;
    EXPECT_EQ(expected_plugin_rect, plugin_->GetPluginRectForTesting())
        << "Plugin rect comparison failure at device scale of " << device_scale
        << ", window rect of " << window_rect.ToString();
  }

  void TestPaintEmptySnapshots(float device_scale,
                               const gfx::Rect& window_rect,
                               const gfx::Rect& paint_rect,
                               const gfx::Rect& expected_clipped_rect) {
    UpdatePluginGeometryWithoutWaiting(device_scale, window_rect);
    canvas_.DrawColor(kDefaultColor);

    plugin_->Paint(canvas_.sk_canvas(), paint_rect);

    // Expect the clipped area on canvas to be filled with plugin's background
    // color.
    SkBitmap expected_bitmap = GenerateExpectedBitmapForPaint(
        expected_clipped_rect, plugin_->GetBackgroundColor());
    EXPECT_TRUE(
        cc::MatchesBitmap(canvas_.GetBitmap(), expected_bitmap,
                          cc::ExactPixelComparator(/*discard_alpha=*/false)))
        << "Failure at device scale of " << device_scale << ", window rect of "
        << window_rect.ToString();
  }

  void TestPaintSnapshots(float device_scale,
                          const gfx::Rect& window_rect,
                          const gfx::Rect& paint_rect,
                          const gfx::Rect& expected_clipped_rect) {
    UpdatePluginGeometry(device_scale, window_rect);
    canvas_.DrawColor(kDefaultColor);

    // Fill the graphics device with `kPaintColor` and update the plugin's
    // snapshot.
    const gfx::Rect& plugin_rect = plugin_->GetPluginRectForTesting();
    std::unique_ptr<Graphics> graphics =
        plugin_->CreatePaintGraphics(plugin_rect.size());
    graphics->PaintImage(
        CreateSkiaImageForTesting(plugin_rect.size(), kPaintColor),
        gfx::Rect(plugin_rect.width(), plugin_rect.height()));
    graphics->Flush(base::DoNothing());

    plugin_->Paint(canvas_.sk_canvas(), paint_rect);

    // Expect the clipped area on canvas to be filled with `kPaintColor`.
    SkBitmap expected_bitmap =
        GenerateExpectedBitmapForPaint(expected_clipped_rect, kPaintColor);
    EXPECT_TRUE(
        cc::MatchesBitmap(canvas_.GetBitmap(), expected_bitmap,
                          cc::ExactPixelComparator(/*discard_alpha=*/false)))
        << "Failure at device scale of " << device_scale << ", window rect of "
        << window_rect.ToString();
  }

  TestPDFiumEngine* engine_ptr_;
  FakeContainerWrapper* wrapper_ptr_;

  // Provides the cc::PaintCanvas for painting.
  gfx::Canvas canvas_{kCanvasSize, /*image_scale=*/1.0f, /*is_opaque=*/true};
};

TEST_F(PdfViewWebPluginWithoutInitializeTest, Initialize) {
  auto wrapper =
      std::make_unique<NiceMock<FakeContainerWrapper>>(plugin_.get());
  auto engine = std::make_unique<NiceMock<TestPDFiumEngine>>(plugin_.get());
  EXPECT_CALL(*wrapper,
              RequestTouchEventType(
                  blink::WebPluginContainer::kTouchEventRequestTypeRaw));

  EXPECT_TRUE(
      plugin_->InitializeForTesting(std::move(wrapper), std::move(engine)));
}

TEST_F(PdfViewWebPluginTest, UpdateGeometrySetsPluginRectUseZoomForDSFEnabled) {
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, ZoomUpdated(2.0f));
  TestUpdateGeometrySetsPluginRect(
      /*device_scale=*/2.0f, /*window_rect=*/gfx::Rect(4, 4, 12, 12),
      /*expected_device_scale=*/2.0f,
      /*expected_plugin_rect=*/gfx::Rect(4, 4, 12, 12));
}

TEST_F(PdfViewWebPluginTest,
       UpdateGeometrySetsPluginRectUseZoomForDSFDisabled) {
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, ZoomUpdated(2.0f));
  TestUpdateGeometrySetsPluginRect(
      /*device_scale=*/2.0f, /*window_rect=*/gfx::Rect(4, 4, 12, 12),
      /*expected_device_scale=*/2.0f,
      /*expected_plugin_rect=*/gfx::Rect(8, 8, 24, 24));
}

TEST_F(PdfViewWebPluginTest,
       UpdateGeometrySetsPluginRectOnVariousDeviceScales) {
  struct UpdateGeometryParams {
    // The plugin container's device scale.
    float device_scale;

    //  The window rect in CSS pixels.
    gfx::Rect window_rect;

    // The expected plugin device scale.
    float expected_device_scale;

    // The expected plugin rect in device pixels.
    gfx::Rect expected_plugin_rect;
  };

  // Keep the using zoom for DSF setting consistent within the test.
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(true));

  static constexpr UpdateGeometryParams kUpdateGeometryParams[] = {
      {1.0f, gfx::Rect(3, 4, 5, 6), 1.0f, gfx::Rect(3, 4, 5, 6)},
      {2.0f, gfx::Rect(3, 4, 5, 6), 2.0f, gfx::Rect(3, 4, 5, 6)},
  };

  for (const auto& params : kUpdateGeometryParams) {
    TestUpdateGeometrySetsPluginRect(params.device_scale, params.window_rect,
                                     params.expected_device_scale,
                                     params.expected_plugin_rect);
  }
}

class PdfViewWebPluginTestUseZoomForDSF
    : public PdfViewWebPluginTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    PdfViewWebPluginTest::SetUp();
    ON_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
        .WillByDefault(Return(GetParam()));
  }
};

TEST_P(PdfViewWebPluginTestUseZoomForDSF,
       UpdateGeometrySetsPluginRectWithEmptyWindow) {
  EXPECT_CALL(*engine_ptr_, ZoomUpdated).Times(0);
  TestUpdateGeometrySetsPluginRect(
      /*device_scale=*/2.0f, /*window_rect=*/gfx::Rect(2, 2, 0, 0),
      /*expected_device_scale=*/1.0f, /*expected_plugin_rect=*/gfx::Rect());
}

TEST_P(PdfViewWebPluginTestUseZoomForDSF, PaintEmptySnapshots) {
  TestPaintEmptySnapshots(/*device_scale=*/4.0f,
                          /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                          /*paint_rect=*/gfx::Rect(5, 5, 15, 15),
                          /*expected_clipped_rect=*/gfx::Rect(10, 10, 10, 10));
}

TEST_P(PdfViewWebPluginTestUseZoomForDSF, PaintSnapshots) {
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(5, 5, 15, 15),
                     /*expected_clipped_rect=*/gfx::Rect(10, 10, 10, 10));
}

TEST_P(PdfViewWebPluginTestUseZoomForDSF,
       PaintSnapshotsWithVariousDeviceScales) {
  static constexpr PaintParams kPaintWithVariousScalesParams[] = {
      {0.4f, gfx::Rect(8, 8, 30, 30), gfx::Rect(10, 10, 30, 30),
       gfx::Rect(10, 10, 28, 28)},
      {1.0f, gfx::Rect(8, 8, 30, 30), gfx::Rect(10, 10, 30, 30),
       gfx::Rect(10, 10, 28, 28)},
      {4.0f, gfx::Rect(8, 8, 30, 30), gfx::Rect(10, 10, 30, 30),
       gfx::Rect(10, 10, 28, 28)},
  };

  for (const auto& params : kPaintWithVariousScalesParams) {
    TestPaintSnapshots(params.device_scale, params.window_rect,
                       params.paint_rect, params.expected_clipped_rect);
  }
}

TEST_P(PdfViewWebPluginTestUseZoomForDSF,
       PaintSnapshotsWithVariousRectPositions) {
  static constexpr PaintParams kPaintWithVariousPositionsParams[] = {
      // The window origin falls outside the `paint_rect` area.
      {4.0f, gfx::Rect(10, 10, 20, 20), gfx::Rect(5, 5, 15, 15),
       gfx::Rect(10, 10, 10, 10)},
      // The window origin falls within the `paint_rect` area.
      {4.0f, gfx::Rect(4, 4, 20, 20), gfx::Rect(8, 8, 15, 15),
       gfx::Rect(8, 8, 15, 15)},
  };

  for (const auto& params : kPaintWithVariousPositionsParams) {
    TestPaintSnapshots(params.device_scale, params.window_rect,
                       params.paint_rect, params.expected_clipped_rect);
  }
}

TEST_P(PdfViewWebPluginTestUseZoomForDSF, UpdateLayerTransformWithIdentity) {
  plugin_->UpdateLayerTransform(1.0f, gfx::Vector2dF());
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 10, 20, 20));
}

TEST_P(PdfViewWebPluginTestUseZoomForDSF, UpdateLayerTransformWithScale) {
  plugin_->UpdateLayerTransform(0.5f, gfx::Vector2dF());
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 10, 10, 10));
}

TEST_F(PdfViewWebPluginTest,
       UpdateLayerTransformWithTranslateUseZoomForDSFDisabled) {
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(false));

  plugin_->UpdateLayerTransform(1.0f, gfx::Vector2dF(-5, 5));
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 15, 15, 15));
}

TEST_F(PdfViewWebPluginTest,
       UpdateLayerTransformWithTranslateUseZoomForDSFEnabled) {
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(true));

  plugin_->UpdateLayerTransform(1.0f, gfx::Vector2dF(-1.25, 1.25));
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 15, 15, 15));
}

TEST_F(PdfViewWebPluginTest,
       UpdateLayerTransformWithScaleAndTranslateUseZoomForDSFDisabled) {
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(false));

  plugin_->UpdateLayerTransform(0.5f, gfx::Vector2dF(-5, 5));
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 15, 5, 10));
}

TEST_F(PdfViewWebPluginTest,
       UpdateLayerTransformWithScaleAndTranslateUseZoomForDSFEnabled) {
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(true));

  plugin_->UpdateLayerTransform(0.5f, gfx::Vector2dF(-1.25, 1.25));
  TestPaintSnapshots(/*device_scale=*/4.0f,
                     /*window_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*paint_rect=*/gfx::Rect(10, 10, 20, 20),
                     /*expected_clipped_rect=*/gfx::Rect(10, 15, 5, 10));
}

INSTANTIATE_TEST_SUITE_P(All,
                         PdfViewWebPluginTestUseZoomForDSF,
                         testing::Bool());

class PdfViewWebPluginMouseEventsTest : public PdfViewWebPluginTest {
 public:
  class TestPDFiumEngineForMouseEvents : public TestPDFiumEngine {
   public:
    explicit TestPDFiumEngineForMouseEvents(PDFEngine::Client* client)
        : TestPDFiumEngine(client) {}

    // TestPDFiumEngine:
    bool HandleInputEvent(const blink::WebInputEvent& event) override {
      // Since blink::WebInputEvent is an abstract class, we cannot use equal
      // matcher to verify its value. Here we test with blink::WebMouseEvent
      // specifically.
      if (!blink::WebInputEvent::IsMouseEventType(event.GetType()))
        return false;

      scaled_mouse_event_ = std::make_unique<blink::WebMouseEvent>();
      *scaled_mouse_event_ = static_cast<const blink::WebMouseEvent&>(event);
      return true;
    }

    const blink::WebMouseEvent* GetScaledMouseEvent() const {
      return scaled_mouse_event_.get();
    }

   private:
    std::unique_ptr<blink::WebMouseEvent> scaled_mouse_event_;
  };

  std::unique_ptr<TestPDFiumEngine> CreateEngine() override {
    return std::make_unique<NiceMock<TestPDFiumEngineForMouseEvents>>(
        plugin_.get());
  }

  TestPDFiumEngineForMouseEvents* engine() {
    return static_cast<TestPDFiumEngineForMouseEvents*>(engine_ptr_);
  }
};

TEST_F(PdfViewWebPluginMouseEventsTest,
       HandleInputEventWithUseZoomForDSFEnabled) {
  // Test when using zoom for DSF is enabled.
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(true));
  wrapper_ptr_->set_device_scale(kDeviceScale);
  UpdatePluginGeometry(kDeviceScale, gfx::Rect(20, 20));

  ui::Cursor dummy_cursor;
  plugin_->HandleInputEvent(
      blink::WebCoalescedInputEvent(CreateDefaultMouseDownEvent(),
                                    ui::LatencyInfo()),
      &dummy_cursor);

  const blink::WebMouseEvent* event = engine()->GetScaledMouseEvent();
  ASSERT_TRUE(event);
  EXPECT_EQ(gfx::PointF(-10.0f, 0.0f), event->PositionInWidget());
}

TEST_F(PdfViewWebPluginMouseEventsTest,
       HandleInputEventWithUseZoomForDSFDisabled) {
  // Test when using zoom for DSF is disabled.
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(false));
  wrapper_ptr_->set_device_scale(kDeviceScale);
  UpdatePluginGeometry(kDeviceScale, gfx::Rect(20, 20));

  ui::Cursor dummy_cursor;
  plugin_->HandleInputEvent(
      blink::WebCoalescedInputEvent(CreateDefaultMouseDownEvent(),
                                    ui::LatencyInfo()),
      &dummy_cursor);

  const blink::WebMouseEvent* event = engine()->GetScaledMouseEvent();
  ASSERT_TRUE(event);
  EXPECT_EQ(gfx::PointF(-20.0f, 0.0f), event->PositionInWidget());
}

class PdfViewWebPluginImeTest : public PdfViewWebPluginTest {
 public:
  class TestPDFiumEngineForIme : public TestPDFiumEngine {
   public:
    explicit TestPDFiumEngineForIme(PDFEngine::Client* client)
        : TestPDFiumEngine(client) {}

    // TestPDFiumEngine:
    MOCK_METHOD(bool,
                HandleInputEvent,
                (const blink::WebInputEvent&),
                (override));
  };

  std::unique_ptr<TestPDFiumEngine> CreateEngine() override {
    return std::make_unique<NiceMock<TestPDFiumEngineForIme>>(plugin_.get());
  }

  TestPDFiumEngineForIme* engine() {
    return static_cast<TestPDFiumEngineForIme*>(engine_ptr_);
  }

  void TestImeSetCompositionForPlugin(const blink::WebString& text) {
    EXPECT_CALL(*engine(), HandleInputEvent).Times(0);
    plugin_->ImeSetCompositionForPlugin(text, std::vector<ui::ImeTextSpan>(),
                                        gfx::Range(),
                                        /*selection_start=*/0,
                                        /*selection_end=*/0);
  }

  void TestImeFinishComposingTextForPlugin(
      const blink::WebString& expected_text) {
    InSequence sequence;
    std::u16string expected_text16 = expected_text.Utf16();
    if (expected_text16.size()) {
      for (const auto& c : expected_text16) {
        base::StringPiece16 expected_key(&c, 1);
        EXPECT_CALL(*engine(),
                    HandleInputEvent(IsExpectedImeKeyEvent(expected_key)))
            .WillOnce(Return(true));
      }
    } else {
      EXPECT_CALL(*engine(), HandleInputEvent).Times(0);
    }
    plugin_->ImeFinishComposingTextForPlugin(false);
  }

  void TestImeCommitTextForPlugin(const blink::WebString& text) {
    InSequence sequence;
    std::u16string expected_text16 = text.Utf16();
    if (expected_text16.size()) {
      for (const auto& c : expected_text16) {
        base::StringPiece16 event(&c, 1);
        EXPECT_CALL(*engine(), HandleInputEvent(IsExpectedImeKeyEvent(event)))
            .WillOnce(Return(true));
      }
    } else {
      EXPECT_CALL(*engine(), HandleInputEvent).Times(0);
    }
    plugin_->ImeCommitTextForPlugin(text, std::vector<ui::ImeTextSpan>(),
                                    gfx::Range(),
                                    /*relative_cursor_pos=*/0);
  }
};

TEST_F(PdfViewWebPluginImeTest, ImeSetCompositionAndFinishAscii) {
  const blink::WebString text = blink::WebString::FromASCII("input");
  TestImeSetCompositionForPlugin(text);
  TestImeFinishComposingTextForPlugin(text);
}

TEST_F(PdfViewWebPluginImeTest, ImeSetCompositionAndFinishUnicode) {
  const blink::WebString text = blink::WebString::FromUTF16(u"你好");
  TestImeSetCompositionForPlugin(text);
  TestImeFinishComposingTextForPlugin(text);
  // Calling ImeFinishComposingTextForPlugin() again is a no-op.
  TestImeFinishComposingTextForPlugin("");
}

TEST_F(PdfViewWebPluginImeTest, ImeSetCompositionAndFinishEmpty) {
  const blink::WebString text;
  TestImeSetCompositionForPlugin(text);
  TestImeFinishComposingTextForPlugin(text);
}

TEST_F(PdfViewWebPluginImeTest, ImeCommitTextForPluginAscii) {
  const blink::WebString text = blink::WebString::FromASCII("a b");
  TestImeCommitTextForPlugin(text);
}

TEST_F(PdfViewWebPluginImeTest, ImeCommitTextForPluginUnicode) {
  const blink::WebString text = blink::WebString::FromUTF16(u"さようなら");
  TestImeCommitTextForPlugin(text);
}

TEST_F(PdfViewWebPluginImeTest, ImeCommitTextForPluginEmpty) {
  const blink::WebString text;
  TestImeCommitTextForPlugin(text);
}

TEST_F(PdfViewWebPluginTest, ChangeTextSelection) {
  ASSERT_FALSE(plugin_->HasSelection());
  ASSERT_TRUE(plugin_->SelectionAsText().IsEmpty());
  ASSERT_TRUE(plugin_->SelectionAsMarkup().IsEmpty());

  static constexpr char kSelectedText[] = "1234";
  EXPECT_CALL(*wrapper_ptr_,
              TextSelectionChanged(blink::WebString::FromUTF8(kSelectedText), 0,
                                   gfx::Range(0, 4)));

  plugin_->SetSelectedText(kSelectedText);
  EXPECT_TRUE(plugin_->HasSelection());
  EXPECT_EQ(kSelectedText, plugin_->SelectionAsText().Utf8());
  EXPECT_EQ(kSelectedText, plugin_->SelectionAsMarkup().Utf8());

  static constexpr char kEmptyText[] = "";
  EXPECT_CALL(*wrapper_ptr_,
              TextSelectionChanged(blink::WebString::FromUTF8(kEmptyText), 0,
                                   gfx::Range(0, 0)));
  plugin_->SetSelectedText(kEmptyText);
  EXPECT_FALSE(plugin_->HasSelection());
  EXPECT_TRUE(plugin_->SelectionAsText().IsEmpty());
  EXPECT_TRUE(plugin_->SelectionAsMarkup().IsEmpty());
}

TEST_F(PdfViewWebPluginTest, FormTextFieldFocusChangeUpdatesTextInputType) {
  ASSERT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            wrapper_ptr_->widget_text_input_type());

  MockFunction<void()> checkpoint;
  {
    InSequence sequence;
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
    EXPECT_CALL(checkpoint, Call);
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
    EXPECT_CALL(checkpoint, Call);
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
    EXPECT_CALL(checkpoint, Call);
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
  }

  plugin_->FormFieldFocusChange(PDFEngine::FocusFieldType::kText);
  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeText,
            wrapper_ptr_->widget_text_input_type());

  checkpoint.Call();

  plugin_->FormFieldFocusChange(PDFEngine::FocusFieldType::kNoFocus);
  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            wrapper_ptr_->widget_text_input_type());

  checkpoint.Call();

  plugin_->FormFieldFocusChange(PDFEngine::FocusFieldType::kText);
  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeText,
            wrapper_ptr_->widget_text_input_type());

  checkpoint.Call();

  plugin_->FormFieldFocusChange(PDFEngine::FocusFieldType::kNonText);
  EXPECT_EQ(blink::WebTextInputType::kWebTextInputTypeNone,
            wrapper_ptr_->widget_text_input_type());
}

TEST_F(PdfViewWebPluginTest, SearchString) {
  static constexpr char16_t kPattern[] = u"fox";
  static constexpr char16_t kTarget[] =
      u"The quick brown fox jumped over the lazy Fox";

  {
    static constexpr PDFEngine::Client::SearchStringResult kExpectation[] = {
        {16, 3}};
    EXPECT_THAT(
        plugin_->SearchString(kTarget, kPattern, /*case_sensitive=*/true),
        Pointwise(SearchStringResultEq(), kExpectation));
  }
  {
    static constexpr PDFEngine::Client::SearchStringResult kExpectation[] = {
        {16, 3}, {41, 3}};
    EXPECT_THAT(
        plugin_->SearchString(kTarget, kPattern, /*case_sensitive=*/false),
        Pointwise(SearchStringResultEq(), kExpectation));
  }
}

TEST_F(PdfViewWebPluginTest, UpdateFocus) {
  MockFunction<void(int checkpoint_num)> checkpoint;

  {
    InSequence sequence;

    // Focus false -> true: Triggers updates.
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
    EXPECT_CALL(*wrapper_ptr_, UpdateSelectionBounds);
    EXPECT_CALL(checkpoint, Call(1));

    // Focus true -> true: No updates.
    EXPECT_CALL(checkpoint, Call(2));

    // Focus true -> false: Triggers updates. `UpdateTextInputState` is called
    // twice because it also gets called due to
    // `PDFiumEngine::UpdateFocus(false)`.
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState).Times(2);
    EXPECT_CALL(*wrapper_ptr_, UpdateSelectionBounds);
    EXPECT_CALL(checkpoint, Call(3));

    // Focus false -> false: No updates.
    EXPECT_CALL(checkpoint, Call(4));

    // Focus false -> true: Triggers updates.
    EXPECT_CALL(*wrapper_ptr_, UpdateTextInputState);
    EXPECT_CALL(*wrapper_ptr_, UpdateSelectionBounds);
  }

  // The focus type does not matter in this test.
  plugin_->UpdateFocus(/*focused=*/true, blink::mojom::FocusType::kNone);
  checkpoint.Call(1);
  plugin_->UpdateFocus(/*focused=*/true, blink::mojom::FocusType::kNone);
  checkpoint.Call(2);
  plugin_->UpdateFocus(/*focused=*/false, blink::mojom::FocusType::kNone);
  checkpoint.Call(3);
  plugin_->UpdateFocus(/*focused=*/false, blink::mojom::FocusType::kNone);
  checkpoint.Call(4);
  plugin_->UpdateFocus(/*focused=*/true, blink::mojom::FocusType::kNone);
}

TEST_F(PdfViewWebPluginTest, ShouldDispatchImeEventsToPlugin) {
  ASSERT_TRUE(plugin_->ShouldDispatchImeEventsToPlugin());
}

TEST_F(PdfViewWebPluginTest, CaretChangeUseZoomForDSFEnabled) {
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*engine_ptr_, ZoomUpdated(2.0f));
  UpdatePluginGeometry(
      /*device_scale=*/2.0f, /*window_rect=*/gfx::Rect(12, 24, 36, 48));
  plugin_->CaretChanged(gfx::Rect(10, 20, 30, 40));
  EXPECT_EQ(gfx::Rect(28, 20, 30, 40), plugin_->GetPluginCaretBounds());
}

TEST_F(PdfViewWebPluginTest, CaretChangeUseZoomForDSFDisabled) {
  EXPECT_CALL(*client_ptr_, IsUseZoomForDSFEnabled)
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*engine_ptr_, ZoomUpdated(2.0f));
  UpdatePluginGeometry(
      /*device_scale=*/2.0f, /*window_rect=*/gfx::Rect(12, 24, 36, 48));
  plugin_->CaretChanged(gfx::Rect(10, 20, 30, 40));
  EXPECT_EQ(gfx::Rect(23, 10, 15, 20), plugin_->GetPluginCaretBounds());
}

}  // namespace chrome_pdf
