// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/win/hwnd_message_handler.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace views {

namespace {

TOUCHINPUT GenerateTouchInput(int id, int x, int y, DWORD dwFlags) {
  TOUCHINPUT input;
  input.dwID = id;
  input.x = x;
  input.y = y;
  input.dwFlags = dwFlags;
  return input;
}

}  // namespace

TEST(HWNDMessageHandler, TestCorrectTouchInputList) {
  HWNDMessageHandler messageHandler(NULL);
  HWNDMessageHandler::TouchEvents touch_events1;

  // One finger down.
  scoped_ptr<TOUCHINPUT[]> input(new TOUCHINPUT[10]);
  input[0] = GenerateTouchInput(1, 10, 10, TOUCHEVENTF_DOWN);

  // Send out one touchpress event and set value to be
  // InPreviousMessage at index 0 in touch_id_list.
  messageHandler.PrepareTouchEventList(input.get(), 1, &touch_events1);
  EXPECT_EQ(1, touch_events1.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, touch_events1[0].type());
  EXPECT_EQ(0, touch_events1[0].touch_id());
  EXPECT_EQ(messageHandler.touch_id_list()[0].in_touch_list,
            HWNDMessageHandler::InTouchList::InPreviousMessage);
  EXPECT_EQ(touch_events1[0].location(),
            messageHandler.touch_id_list()[0].location);

  // One finger move and two fingers down.
  HWNDMessageHandler::TouchEvents touch_events2;
  input[0] = GenerateTouchInput(1, 20, 20, TOUCHEVENTF_MOVE);
  input[1] = GenerateTouchInput(2, 30, 30, TOUCHEVENTF_DOWN);
  input[2] = GenerateTouchInput(3, 40, 40, TOUCHEVENTF_DOWN);

  // Send out touchmove and two touchpress events and touch_id_list has
  // three InPreviousMessage at index 0, 1, 2.
  messageHandler.PrepareTouchEventList(input.get(), 3, &touch_events2);
  EXPECT_EQ(3, touch_events2.size());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, touch_events2[0].type());
  EXPECT_EQ(0, touch_events2[0].touch_id());
  EXPECT_EQ(messageHandler.touch_id_list()[0].in_touch_list,
            HWNDMessageHandler::InTouchList::InPreviousMessage);
  EXPECT_EQ(messageHandler.touch_id_list()[1].in_touch_list,
            HWNDMessageHandler::InTouchList::InPreviousMessage);
  EXPECT_EQ(messageHandler.touch_id_list()[2].in_touch_list,
            HWNDMessageHandler::InTouchList::InPreviousMessage);

  // Release one finger and two finger moves.
  HWNDMessageHandler::TouchEvents touch_events3;
  input[0] = GenerateTouchInput(1, 22, 22, TOUCHEVENTF_UP);
  input[1] = GenerateTouchInput(2, 33, 33, TOUCHEVENTF_MOVE);
  input[2] = GenerateTouchInput(3, 44, 44, TOUCHEVENTF_MOVE);

  // Send out one touchrelease, two touch move events and touch_id_list has
  // two InPreviousMessage at index 1, 2 and one NotPresent at index 0.
  messageHandler.PrepareTouchEventList(input.get(), 3, &touch_events3);
  EXPECT_EQ(3, touch_events3.size());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, touch_events3[0].type());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, touch_events3[1].type());
  EXPECT_EQ(ui::ET_TOUCH_MOVED, touch_events3[2].type());
  EXPECT_EQ(0, touch_events3[0].touch_id());
  EXPECT_EQ(1, touch_events3[1].touch_id());
  EXPECT_EQ(2, touch_events3[2].touch_id());
  EXPECT_EQ(messageHandler.touch_id_list()[0].in_touch_list,
            HWNDMessageHandler::InTouchList::NotPresent);
  EXPECT_EQ(messageHandler.touch_id_list()[1].in_touch_list,
            HWNDMessageHandler::InTouchList::InPreviousMessage);
  EXPECT_EQ(messageHandler.touch_id_list()[2].in_touch_list,
            HWNDMessageHandler::InTouchList::InPreviousMessage);
}

TEST(HWNDMessageHandler, TestCombineTouchMoveAndUp) {
    HWNDMessageHandler messageHandler(NULL);
    HWNDMessageHandler::TouchEvents touch_events1;

    // Two fingers down.
    scoped_ptr<TOUCHINPUT[]> input(new TOUCHINPUT[10]);
    input[0] = GenerateTouchInput(1, 10, 10, TOUCHEVENTF_DOWN);
    input[1] = GenerateTouchInput(2, 20, 20, TOUCHEVENTF_DOWN);

    // Send out two touchpress events and touch_id_list has two
    // InPreviousMessage.
    messageHandler.PrepareTouchEventList(input.get(), 2, &touch_events1);
    EXPECT_EQ(2, touch_events1.size());
    EXPECT_EQ(ui::ET_TOUCH_PRESSED, touch_events1[0].type());
    EXPECT_EQ(ui::ET_TOUCH_PRESSED, touch_events1[1].type());
    EXPECT_EQ(0, touch_events1[0].touch_id());
    EXPECT_EQ(1, touch_events1[1].touch_id());
    EXPECT_EQ(messageHandler.touch_id_list()[0].in_touch_list,
        HWNDMessageHandler::InTouchList::InPreviousMessage);
    EXPECT_EQ(messageHandler.touch_id_list()[1].in_touch_list,
        HWNDMessageHandler::InTouchList::InPreviousMessage);

    // One finger move and up and one fingers move.
    HWNDMessageHandler::TouchEvents touch_events2;
    input[0] = GenerateTouchInput(1, 20, 20,
        TOUCHEVENTF_MOVE | TOUCHEVENTF_UP);
    input[1] = GenerateTouchInput(2, 30, 30, TOUCHEVENTF_MOVE);

    // Send out a touchmove, touchrelease and a touchmove events and
    // touch_id_list has one InPreviousMessage at index 1 and one NotPresent
    // at index 0.
    messageHandler.PrepareTouchEventList(input.get(), 2, &touch_events2);
    EXPECT_EQ(3, touch_events2.size());
    EXPECT_EQ(ui::ET_TOUCH_MOVED, touch_events2[0].type());
    EXPECT_EQ(ui::ET_TOUCH_RELEASED, touch_events2[1].type());
    EXPECT_EQ(ui::ET_TOUCH_MOVED, touch_events2[2].type());
    EXPECT_EQ(0, touch_events2[0].touch_id());
    EXPECT_EQ(0, touch_events2[1].touch_id());
    EXPECT_EQ(1, touch_events2[2].touch_id());
    EXPECT_EQ(messageHandler.touch_id_list()[0].in_touch_list,
              HWNDMessageHandler::InTouchList::NotPresent);
    EXPECT_EQ(messageHandler.touch_id_list()[1].in_touch_list,
              HWNDMessageHandler::InTouchList::InPreviousMessage);
}

TEST(HWNDMessageHandler, TestMissingTouchRelease) {
  HWNDMessageHandler messageHandler(NULL);
  HWNDMessageHandler::TouchEvents touch_events1;

  // Two fingers down.
  scoped_ptr<TOUCHINPUT[]> input(new TOUCHINPUT[10]);
  input[0] = GenerateTouchInput(1, 10, 10, TOUCHEVENTF_DOWN);
  input[1] = GenerateTouchInput(2, 20, 20, TOUCHEVENTF_DOWN);

  // Send out two touchpress events and touch_id_list has two
  // InPreviousMessage.
  messageHandler.PrepareTouchEventList(input.get(), 2, &touch_events1);
  EXPECT_EQ(2, touch_events1.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, touch_events1[0].type());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, touch_events1[1].type());
  EXPECT_EQ(0, touch_events1[0].touch_id());
  EXPECT_EQ(1, touch_events1[1].touch_id());
  EXPECT_EQ(messageHandler.touch_id_list()[0].in_touch_list,
            HWNDMessageHandler::InTouchList::InPreviousMessage);
  EXPECT_EQ(messageHandler.touch_id_list()[1].in_touch_list,
            HWNDMessageHandler::InTouchList::InPreviousMessage);

  // Only one finger down, so we miss two touchrelease.
  HWNDMessageHandler::TouchEvents touch_events2;
  input[0] = GenerateTouchInput(3, 30, 30, TOUCHEVENTF_DOWN);

  // Send out one touchpress and two touchrelease events for the touch ids
  // which are not in this input list, and touch_id_list has one
  // InPreviousMessage at index 2.
  messageHandler.PrepareTouchEventList(input.get(), 1, &touch_events2);
  EXPECT_EQ(3, touch_events2.size());
  EXPECT_EQ(ui::ET_TOUCH_PRESSED, touch_events2[0].type());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, touch_events2[1].type());
  EXPECT_EQ(ui::ET_TOUCH_RELEASED, touch_events2[2].type());
  EXPECT_EQ(2, touch_events2[0].touch_id());
  EXPECT_EQ(0, touch_events2[1].touch_id());
  EXPECT_EQ(1, touch_events2[2].touch_id());
  EXPECT_EQ(messageHandler.touch_id_list()[0].in_touch_list,
            HWNDMessageHandler::InTouchList::NotPresent);
  EXPECT_EQ(messageHandler.touch_id_list()[1].in_touch_list,
            HWNDMessageHandler::InTouchList::NotPresent);
  EXPECT_EQ(messageHandler.touch_id_list()[2].in_touch_list,
            HWNDMessageHandler::InTouchList::InPreviousMessage);
}

}  // namespace views
