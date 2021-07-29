// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/input/virtualkeyboard/cpp/fidl.h>
#include <fuchsia/ui/input3/cpp/fidl.h>
#include <lib/fit/function.h>

#include "base/callback.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/koid.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/test_component_context_for_process.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "content/public/test/browser_test.h"
#include "fuchsia/base/test/frame_test_util.h"
#include "fuchsia/base/test/scoped_connection_checker.h"
#include "fuchsia/base/test/test_navigation_listener.h"
#include "fuchsia/engine/browser/context_impl.h"
#include "fuchsia/engine/browser/frame_impl.h"
#include "fuchsia/engine/browser/mock_virtual_keyboard.h"
#include "fuchsia/engine/features.h"
#include "fuchsia/engine/test/frame_for_test.h"
#include "fuchsia/engine/test/scenic_test_helper.h"
#include "fuchsia/engine/test/test_data.h"
#include "fuchsia/engine/test/web_engine_browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace virtualkeyboard = fuchsia::input::virtualkeyboard;

namespace {

const gfx::Point kNoTarget = {999, 999};

constexpr char kInputFieldText[] = "input-text";
constexpr char kInputFieldModeTel[] = "input-mode-tel";
constexpr char kInputFieldModeNumeric[] = "input-mode-numeric";
constexpr char kInputFieldModeUrl[] = "input-mode-url";
constexpr char kInputFieldModeEmail[] = "input-mode-email";
constexpr char kInputFieldModeDecimal[] = "input-mode-decimal";
constexpr char kInputFieldModeSearch[] = "input-mode-search";
constexpr char kInputFieldTypeTel[] = "input-type-tel";
constexpr char kInputFieldTypeNumber[] = "input-type-number";
constexpr char kInputFieldTypePassword[] = "input-type-password";

class VirtualKeyboardTest : public cr_fuchsia::WebEngineBrowserTest {
 public:
  VirtualKeyboardTest() {
    set_test_server_root(base::FilePath(cr_fuchsia::kTestServerRoot));
  }
  ~VirtualKeyboardTest() override = default;

  void SetUp() override {
    scoped_feature_list_.InitWithFeatures(
        {features::kVirtualKeyboard, features::kKeyboardInput}, {});
    cr_fuchsia::WebEngineBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    cr_fuchsia::WebEngineBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    fuchsia::web::CreateFrameParams params;
    frame_for_test_ =
        cr_fuchsia::FrameForTest::Create(context(), std::move(params));

    component_context_.emplace(
        base::TestComponentContextForProcess::InitialState::kCloneAll);
    controller_creator_.emplace(&*component_context_);

    controller_ = controller_creator_->CreateController();

    // Ensure that the fuchsia.ui.input3.Keyboard service is connected.
    component_context_->additional_services()
        ->RemovePublicService<fuchsia::ui::input3::Keyboard>();
    keyboard_input_checker_.emplace(component_context_->additional_services());

    fuchsia::web::NavigationControllerPtr controller;
    frame_for_test_.ptr()->GetNavigationController(controller.NewRequest());
    const GURL test_url(embedded_test_server()->GetURL("/input_fields.html"));
    EXPECT_TRUE(cr_fuchsia::LoadUrlAndExpectResponse(
        controller.get(), fuchsia::web::LoadUrlParams(), test_url.spec()));
    frame_for_test_.navigation_listener().RunUntilUrlEquals(test_url);

    fuchsia::web::FramePtr* frame_ptr = &(frame_for_test_.ptr());
    web_contents_ =
        context_impl()->GetFrameImplForTest(frame_ptr)->web_contents();
    scenic_test_helper_.CreateScenicView(
        context_impl()->GetFrameImplForTest(frame_ptr), frame_for_test_.ptr());
    scenic_test_helper_.SetUpViewForInteraction(web_contents_);

    controller_->AwaitWatchAndRespondWith(false);
    ASSERT_EQ(
        base::GetKoid(controller_->view_ref().reference).value(),
        base::GetKoid(scenic_test_helper_.CloneViewRef().reference).value());
  }

  gfx::Point GetCoordinatesOfInputField(base::StringPiece id) {
    // Distance to click from the top/left extents of an input field.
    constexpr int kInputFieldClickInset = 8;

    absl::optional<base::Value> result = cr_fuchsia::ExecuteJavaScript(
        frame_for_test_.ptr().get(),
        base::StringPrintf("getPointInsideText('%s')", id.data()));
    CHECK(result);

    // Note that coordinates are floating point and must be retrieved as such
    // from the Value, but we can cast them to integers and disregard the
    // fractional value with no major consequences.
    return gfx::Point(*result->FindDoublePath("x") + kInputFieldClickInset,
                      *result->FindDoublePath("y") + kInputFieldClickInset);
  }

 protected:
  cr_fuchsia::FrameForTest frame_for_test_;
  cr_fuchsia::ScenicTestHelper scenic_test_helper_;
  base::test::ScopedFeatureList scoped_feature_list_;

  absl::optional<EnsureConnectedChecker<fuchsia::ui::input3::Keyboard>>
      keyboard_input_checker_;

  // Fake virtual keyboard services for the InputMethod to use.
  absl::optional<base::TestComponentContextForProcess> component_context_;
  absl::optional<MockVirtualKeyboardControllerCreator> controller_creator_;
  std::unique_ptr<MockVirtualKeyboardController> controller_;

  content::WebContents* web_contents_ = nullptr;
};

// Verifies that RequestShow() is not called redundantly if the virtual
// keyboard is reported as visible.
// TODO(https://crbug.com/1226757): Flaky on Fuchsia-x64.
IN_PROC_BROWSER_TEST_F(VirtualKeyboardTest, DISABLED_ShowAndHideWithVisibility) {
  testing::InSequence s;

  // Alphanumeric field click.
  base::RunLoop on_show_run_loop;
  EXPECT_CALL(*controller_, RequestShow())
      .WillOnce(testing::InvokeWithoutArgs(
          [&on_show_run_loop]() { on_show_run_loop.Quit(); }))
      .RetiresOnSaturation();

  // Numeric field click.
  base::RunLoop click_numeric_run_loop;
  EXPECT_CALL(*controller_, RequestHide()).RetiresOnSaturation();
  EXPECT_CALL(*controller_, SetTextType(virtualkeyboard::TextType::NUMERIC))
      .RetiresOnSaturation();
  EXPECT_CALL(*controller_, RequestShow())
      .WillOnce(testing::InvokeWithoutArgs(
          [&click_numeric_run_loop]() { click_numeric_run_loop.Quit(); }))
      .RetiresOnSaturation();

  // Input blur click.
  base::RunLoop on_hide_run_loop;
  EXPECT_CALL(*controller_, RequestHide())
      .WillOnce(testing::InvokeWithoutArgs(
          [&on_hide_run_loop]() { on_hide_run_loop.Quit(); }))
      .RetiresOnSaturation();

  // Give focus to an alphanumeric input field, which will result in
  // RequestShow() being called.
  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldText));
  on_show_run_loop.Run();
  EXPECT_EQ(controller_->text_type(), virtualkeyboard::TextType::ALPHANUMERIC);

  // Indicate that the virtual keyboard is now visible.
  controller_->AwaitWatchAndRespondWith(true);
  base::RunLoop().RunUntilIdle();

  // Tap on another text field. RequestShow should not be called a second time
  // since the keyboard is already onscreen.
  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldModeNumeric));
  click_numeric_run_loop.Run();

  // Trigger input blur by clicking outside any input element.
  content::SimulateTapAt(web_contents_, kNoTarget);
  on_hide_run_loop.Run();
}

// Gives focus to a sequence of HTML <input> nodes with different InputModes,
// and verifies that the InputMode's FIDL equivalent is sent via SetTextType().
IN_PROC_BROWSER_TEST_F(VirtualKeyboardTest, InputModeMappings) {
  // Note that the service will elide type updates if there is no change,
  // so the array is ordered to produce an update on each entry.
  const std::vector<std::pair<base::StringPiece, virtualkeyboard::TextType>>
      kInputTypeMappings = {
          {kInputFieldModeTel, virtualkeyboard::TextType::PHONE},
          {kInputFieldModeSearch, virtualkeyboard::TextType::ALPHANUMERIC},
          {kInputFieldModeNumeric, virtualkeyboard::TextType::NUMERIC},
          {kInputFieldModeUrl, virtualkeyboard::TextType::ALPHANUMERIC},
          {kInputFieldModeDecimal, virtualkeyboard::TextType::NUMERIC},
          {kInputFieldModeEmail, virtualkeyboard::TextType::ALPHANUMERIC},
          {kInputFieldTypeTel, virtualkeyboard::TextType::PHONE},
          {kInputFieldTypeNumber, virtualkeyboard::TextType::NUMERIC},
          {kInputFieldTypePassword, virtualkeyboard::TextType::ALPHANUMERIC},
      };

  // GMock expectations must be set upfront, hence the redundant for-each loop.
  testing::InSequence s;
  virtualkeyboard::TextType previous_text_type =
      virtualkeyboard::TextType::ALPHANUMERIC;
  std::vector<base::RunLoop> set_type_loops(base::size(kInputTypeMappings));
  for (size_t i = 0; i < base::size(kInputTypeMappings); ++i) {
    const auto& field_type_pair = kInputTypeMappings[i];
    DCHECK_NE(field_type_pair.second, previous_text_type);

    EXPECT_CALL(*controller_, SetTextType(field_type_pair.second))
        .WillOnce(testing::InvokeWithoutArgs(
            [run_loop = &set_type_loops[i]]() mutable { run_loop->Quit(); }))
        .RetiresOnSaturation();
    previous_text_type = field_type_pair.second;
  }

  controller_->AwaitWatchAndRespondWith(false);

  for (size_t i = 0; i < base::size(kInputTypeMappings); ++i) {
    content::SimulateTapAt(
        web_contents_, GetCoordinatesOfInputField(kInputTypeMappings[i].first));

    // Spin the runloop until we've received the type update.
    set_type_loops[i].Run();
  }
}

IN_PROC_BROWSER_TEST_F(VirtualKeyboardTest, Disconnection) {
  testing::InSequence s;
  base::RunLoop on_show_run_loop;
  EXPECT_CALL(*controller_, RequestShow())
      .WillOnce(testing::InvokeWithoutArgs(
          [&on_show_run_loop]() { on_show_run_loop.Quit(); }));

  // Tapping inside the text field should show the IME and signal RequestShow.
  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldText));
  on_show_run_loop.Run();

  controller_->AwaitWatchAndRespondWith(true);
  base::RunLoop().RunUntilIdle();

  // Disconnect the FIDL service.
  controller_.reset();
  base::RunLoop().RunUntilIdle();

  // Focus on another text field, then defocus. Nothing should crash.
  content::SimulateTapAt(web_contents_,
                         GetCoordinatesOfInputField(kInputFieldModeNumeric));
  content::SimulateTapAt(web_contents_, kNoTarget);
}

}  // namespace
