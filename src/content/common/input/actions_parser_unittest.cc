// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/input/actions_parser.h"

#include "base/json/json_reader.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

TEST(ActionsParserTest, ParseMousePointerActionSequence) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 2, "y": 3,
                         "button": 0},
                        {"type": "pointerUp", "x": 2, "y": 3,
                         "button": 0}],
            "parameters": {"pointerType": "mouse"},
            "id": "1"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_TRUE(actions_parser.ParsePointerActionSequence());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.gesture_params();
  EXPECT_EQ(SyntheticGestureParams::MOUSE_INPUT,
            action_list_params.gesture_source_type);
  EXPECT_EQ(2U, action_list_params.params.size());
  EXPECT_EQ(1U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(2, 3), action_list_params.params[0][0].position());
  EXPECT_EQ(SyntheticPointerActionParams::Button::LEFT,
            action_list_params.params[0][0].button());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::RELEASE,
            action_list_params.params[1][0].pointer_action_type());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequence) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 3, "y": 5},
                        {"type": "pointerMove", "x": 30, "y": 30},
                        {"type": "pointerUp" }],
            "parameters": {"pointerType": "touch"},
            "id": "pointer1"},
           {"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                        {"type": "pointerMove", "x": 50, "y": 50},
                        {"type": "pointerUp" }],
            "parameters": {"pointerType": "touch"},
            "id": "pointer2"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_TRUE(actions_parser.ParsePointerActionSequence());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.gesture_params();
  EXPECT_EQ(SyntheticGestureParams::TOUCH_INPUT,
            action_list_params.gesture_source_type);
  EXPECT_EQ(3U, action_list_params.params.size());
  EXPECT_EQ(2U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(3, 5), action_list_params.params[0][0].position());
  EXPECT_EQ(0U, action_list_params.params[0][0].pointer_id());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][1].pointer_action_type());
  EXPECT_EQ(gfx::PointF(10, 10), action_list_params.params[0][1].position());
  EXPECT_EQ(1U, action_list_params.params[0][1].pointer_id());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceIdNotString) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 0, "y": 0},
                        {"type": "pointerMove", "x": 30, "y": 30},
                        {"type": "pointerUp"}],
            "parameters": {"pointerType": "touch"},
            "id": 1},
           {"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                        {"type": "pointerMove", "x": 50, "y": 50},
                        {"type": "pointerUp"}],
            "parameters": {"pointerType": "touch"},
            "id": 2}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer name is missing or not a string",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceDuplicateId) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 0, "y": 0},
                        {"type": "pointerMove", "x": 30, "y": 30},
                        {"type": "pointerUp"}],
            "parameters": {"pointerType": "touch"},
            "id": "pointer1"},
           {"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                        {"type": "pointerMove", "x": 50, "y": 50},
                        {"type": "pointerUp"}],
            "parameters": {"pointerType": "touch"},
            "id": "pointer1"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer name already exists", actions_parser.error_message());
}

TEST(ActionsParserTest, ParseMousePointerActionSequenceNoParameters) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 2, "y": 3,
                         "button": 0},
                        {"type": "pointerUp", "x": 2, "y": 3,
                         "button": 0}],
            "id": "pointer1"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("action sequence parameters is missing for pointer type",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseMousePointerActionSequenceNoPointerType) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 2, "y": 3,
                         "button": 0},
                        {"type": "pointerUp", "x": 2, "y": 3,
                         "button": 0}],
            "parameters": {},
            "id": "pointer1"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("action sequence pointer type is missing or not a string",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseMousePointerActionSequenceNoAction) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer", "parameters": {"pointerType": "mouse"},
            "id": "pointer1"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer[0].actions is missing or not a list",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseMousePointerActionSequenceUnsupportedButton) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 2, "y": 3,
                         "button": -1},
                        {"type": "pointerUp", "x": 2, "y": 3,
                         "button": 0}],
            "parameters": {"pointerType": "mouse"},
            "id": "1"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("actions[0].actions.button is an unsupported button",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceMultiActionsType) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "key",
            "actions": [{"type":"keyDown","value":"p"},
                        {"type":"keyUp","value":"p"},
                        {"type":"keyDown","value":"a"},
                        {"type":"keyUp","value":"a"}],
            "id": "1"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("we only support action sequence type of pointer",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceMultiPointerType) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 3, "y": 5},
                        {"type": "pointerMove", "x": 30, "y": 30},
                        {"type": "pointerUp" }],
            "parameters": {"pointerType": "touch"},
            "id": "pointer1"},
           {"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                        {"type": "pointerMove", "x": 50, "y": 50},
                        {"type": "pointerUp" }],
            "parameters": {"pointerType": "mouse"},
            "id": "1"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("currently multiple action sequence pointer type are not supported",
            actions_parser.error_message());
}

TEST(ActionsParserTest, ParseTouchPointerActionSequenceMultiMouse) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 3, "y": 5},
                        {"type": "pointerMove", "x": 30, "y": 30},
                        {"type": "pointerUp" }],
            "parameters": {"pointerType": "mouse"},
            "id": "1"},
           {"type": "pointer",
            "actions": [{"type": "pointerDown", "x": 10, "y": 10},
                        {"type": "pointerMove", "x": 50, "y": 50},
                        {"type": "pointerUp" }],
            "parameters": {"pointerType": "mouse"},
            "id": "2"}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("for input type of mouse and pen, we only support one device",
            actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseMousePointerActionSequence) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "mouse", "id": 0,
            "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                         "button": 0},
                        {"name": "pointerUp", "x": 2, "y": 3,
                         "button": 0}]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_TRUE(actions_parser.ParsePointerActionSequence());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.gesture_params();
  EXPECT_EQ(SyntheticGestureParams::MOUSE_INPUT,
            action_list_params.gesture_source_type);
  EXPECT_EQ(2U, action_list_params.params.size());
  EXPECT_EQ(1U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(2, 3), action_list_params.params[0][0].position());
  EXPECT_EQ(SyntheticPointerActionParams::Button::LEFT,
            action_list_params.params[0][0].button());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::RELEASE,
            action_list_params.params[1][0].pointer_action_type());
}

TEST(ActionsParserTest, OldParseTouchPointerActionSequence1) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 1,
            "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": 2,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_TRUE(actions_parser.ParsePointerActionSequence());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.gesture_params();
  EXPECT_EQ(SyntheticGestureParams::TOUCH_INPUT,
            action_list_params.gesture_source_type);
  EXPECT_EQ(3U, action_list_params.params.size());
  EXPECT_EQ(2U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(3, 5), action_list_params.params[0][0].position());
  EXPECT_EQ(1U, action_list_params.params[0][0].pointer_id());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][1].pointer_action_type());
  EXPECT_EQ(gfx::PointF(10, 10), action_list_params.params[0][1].position());
  EXPECT_EQ(2U, action_list_params.params[0][1].pointer_id());
}

TEST(ActionsParserTest, OldParseTouchPointerActionSequenceWithoutId) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": 1,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_TRUE(actions_parser.ParsePointerActionSequence());
  SyntheticPointerActionListParams action_list_params =
      actions_parser.gesture_params();
  EXPECT_EQ(SyntheticGestureParams::TOUCH_INPUT,
            action_list_params.gesture_source_type);
  EXPECT_EQ(3U, action_list_params.params.size());
  EXPECT_EQ(2U, action_list_params.params[0].size());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][0].pointer_action_type());
  EXPECT_EQ(gfx::PointF(3, 5), action_list_params.params[0][0].position());
  EXPECT_EQ(0U, action_list_params.params[0][0].pointer_id());
  EXPECT_EQ(SyntheticPointerActionParams::PointerActionType::PRESS,
            action_list_params.params[0][1].pointer_action_type());
  EXPECT_EQ(gfx::PointF(10, 10), action_list_params.params[0][1].position());
  EXPECT_EQ(1U, action_list_params.params[0][1].pointer_id());
}

TEST(ActionsParserTest, OldParseTouchPointerActionSequenceIdNotInt) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": "0",
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": "1",
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer id is not an integer", actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseTouchPointerActionSequenceIdNegative) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": -1,
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": -2,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer id can not be negative", actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseTouchPointerActionSequenceDuplicateId) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer id already exists", actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseTouchPointerActionSequenceNoId) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch",
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("this pointer does not have a pointer id",
            actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseTouchPointerActionSequenceMissingId) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch",
            "actions": [{"name": "pointerDown", "x": 0, "y": 0},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "touch", "id": 0,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("some pointers do not have a pointer id",
            actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseMousePointerActionSequenceNoSource) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"id": 0, "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                                  "button": 0},
                                 {"name": "pointerUp", "x": 2, "y": 3,
                                  "button": 0}]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("source type is missing or not a string",
            actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseMousePointerActionSequenceNoAction) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "mouse", "id": 0}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("pointer[0].actions is missing or not a list",
            actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseMousePointerActionSequenceUnsupportedButton) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "mouse", "id": 0,
            "actions": [{"name": "pointerDown", "x": 2, "y": 3,
                         "button": -1},
                        {"name": "pointerUp", "x": 2, "y": 3,
                         "button": 0}]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("actions[0].actions.button is an unsupported button",
            actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseTouchPointerActionSequenceMultiSource) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "touch", "id": 1,
            "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "mouse", "id": 2,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ("currently multiple input sources are not not supported",
            actions_parser.error_message());
}

TEST(ActionsParserTest, OldParseTouchPointerActionSequenceMultiMouse) {
  std::unique_ptr<base::Value> value = base::JSONReader::Read(
      R"( [{"source": "mouse", "id": 1,
            "actions": [{"name": "pointerDown", "x": 3, "y": 5},
                        {"name": "pointerMove", "x": 30, "y": 30},
                        {"name": "pointerUp" } ]},
           {"source": "mouse", "id": 2,
            "actions": [{"name": "pointerDown", "x": 10, "y": 10},
                        {"name": "pointerMove", "x": 50, "y": 50},
                        {"name": "pointerUp" } ]}] )");

  ActionsParser actions_parser(value.get());
  EXPECT_FALSE(actions_parser.ParsePointerActionSequence());
  EXPECT_EQ(
      "for input source type of mouse and pen, we only support one device in "
      "one sequence",
      actions_parser.error_message());
}

}  // namespace content
