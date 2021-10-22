// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/projector_app/annotator_message_handler.h"
#include "ash/public/cpp/test/mock_projector_controller.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "chromeos/components/projector_app/annotator_tool.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"

namespace chromeos {

namespace {

const char kWebUIListenerCall[] = "cr.webUIListenerCallback";

}  // namespace

class AnnotatorMessageHandlerTest : public testing::Test {
 public:
  AnnotatorMessageHandlerTest() = default;
  AnnotatorMessageHandlerTest(const AnnotatorMessageHandlerTest&) = delete;
  AnnotatorMessageHandler& operator=(const AnnotatorMessageHandlerTest&) =
      delete;
  ~AnnotatorMessageHandlerTest() override = default;

  // testing::Test:
  void SetUp() override {
    message_handler_ = std::make_unique<AnnotatorMessageHandler>();
    message_handler_->set_web_ui_for_test(&web_ui());
    message_handler_->RegisterMessages();
  }

  void TearDown() override { message_handler_.reset(); }

  void ExpectCallToWebUI(const std::string& type,
                         const std::string& func_name,
                         size_t count) {
    EXPECT_EQ(web_ui().call_data().size(), count);
    const content::TestWebUI::CallData& call_data = *(web_ui().call_data()[0]);
    EXPECT_EQ(call_data.function_name(), type);
    EXPECT_EQ(call_data.arg1()->GetString(), func_name);
  }

  void SendUndoRedoAvailableChanged(bool undo_available, bool redo_available) {
    base::ListValue list_args;
    list_args.Append(base::Value(undo_available));
    list_args.Append(base::Value(redo_available));
    web_ui().HandleReceivedMessage("onUndoRedoAvailabilityChanged", &list_args);
  }

  content::TestWebUI& web_ui() { return web_ui_; }
  AnnotatorMessageHandler* handler() { return message_handler_.get(); }
  ash::MockProjectorController& controller() { return controller_; }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;

  std::unique_ptr<AnnotatorMessageHandler> message_handler_;
  content::TestWebUI web_ui_;
  ash::MockProjectorController controller_;
};

TEST_F(AnnotatorMessageHandlerTest, SetTool) {
  AnnotatorTool expected_tool;
  expected_tool.color = SkColorSetARGB(0xA1, 0xB2, 0xC3, 0xD4);
  expected_tool.size = 5;
  expected_tool.type = AnnotatorToolType::kPen;
  handler()->SetTool(expected_tool);

  // Let's check that the call has been made.
  ExpectCallToWebUI(kWebUIListenerCall, "setTool", /* call_count = */ 1u);
  const content::TestWebUI::CallData& call_data = *(web_ui().call_data()[0]);

  AnnotatorTool requested_tool = AnnotatorTool::FromValue(*call_data.arg2());
  EXPECT_EQ(requested_tool, expected_tool);

  // Now let's check that when the tool has been set, we notify the callback.
  EXPECT_CALL(controller(), OnToolSet(expected_tool));

  base::ListValue list_args;
  list_args.Append(expected_tool.ToValue());
  web_ui().HandleReceivedMessage("onToolSet", &list_args);
}

TEST_F(AnnotatorMessageHandlerTest, Undo) {
  handler()->Undo();
  ExpectCallToWebUI(kWebUIListenerCall, "undo", /* call_count = */ 1u);
}

TEST_F(AnnotatorMessageHandlerTest, Redo) {
  handler()->Redo();
  ExpectCallToWebUI(kWebUIListenerCall, "redo", /* call_count = */ 1u);
}

TEST_F(AnnotatorMessageHandlerTest, Clear) {
  handler()->Clear();
  ExpectCallToWebUI(kWebUIListenerCall, "clear", /* call_count = */ 1u);
}

TEST_F(AnnotatorMessageHandlerTest, UndoRedoAvailabilityChanged) {
  EXPECT_CALL(controller(), OnUndoRedoAvailabilityChanged(false, false));
  SendUndoRedoAvailableChanged(false, false);

  EXPECT_CALL(controller(), OnUndoRedoAvailabilityChanged(true, true));
  SendUndoRedoAvailableChanged(true, true);

  EXPECT_CALL(controller(), OnUndoRedoAvailabilityChanged(false, true));
  SendUndoRedoAvailableChanged(false, true);
}

}  // namespace chromeos
