// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_resource_array.h"

#include "ppapi/cpp/dev/resource_array_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(ResourceArray);

namespace {

pp::InputEvent CreateMouseEvent(pp::Instance* instance,
                                PP_InputEvent_Type type,
                                PP_InputEvent_MouseButton buttons) {
  return pp::MouseInputEvent(
      instance,
      type,
      100,  // time_stamp
      0,  // modifiers
      buttons,
      pp::Point(),  // position
      1,  // click count
      pp::Point());  // movement
}

pp::ImageData CreateImageData(pp::Instance* instance) {
  return pp::ImageData(
      instance,
      PP_IMAGEDATAFORMAT_RGBA_PREMUL,
      pp::Size(1, 1),
      true);
}

}  // namespace

TestResourceArray::TestResourceArray(TestingInstance* instance)
    : TestCase(instance) {
}

TestResourceArray::~TestResourceArray() {
}

void TestResourceArray::RunTests(const std::string& filter) {
  RUN_TEST(Basics, filter);
  RUN_TEST(OutOfRangeAccess, filter);
  RUN_TEST(EmptyArray, filter);
  RUN_TEST(InvalidElement, filter);
}

std::string TestResourceArray::TestBasics() {
  pp::InputEvent mouse_event_1 = CreateMouseEvent(
      instance_, PP_INPUTEVENT_TYPE_MOUSEDOWN, PP_INPUTEVENT_MOUSEBUTTON_LEFT);
  pp::InputEvent mouse_event_2 = CreateMouseEvent(
      instance_, PP_INPUTEVENT_TYPE_MOUSEUP, PP_INPUTEVENT_MOUSEBUTTON_RIGHT);
  pp::ImageData image_data = CreateImageData(instance_);

  PP_Resource elements[] = {
    mouse_event_1.pp_resource(),
    mouse_event_2.pp_resource(),
    image_data.pp_resource()
  };
  size_t size = sizeof(elements) / sizeof(elements[0]);

  pp::ResourceArray_Dev resource_array(instance_, elements, size);

  ASSERT_EQ(size, resource_array.size());
  for (uint32_t index = 0; index < size; ++index)
    ASSERT_EQ(elements[index], resource_array[index]);

  PASS();
}

std::string TestResourceArray::TestOutOfRangeAccess() {
  pp::InputEvent mouse_event_1 = CreateMouseEvent(
      instance_, PP_INPUTEVENT_TYPE_MOUSEDOWN, PP_INPUTEVENT_MOUSEBUTTON_LEFT);
  pp::InputEvent mouse_event_2 = CreateMouseEvent(
      instance_, PP_INPUTEVENT_TYPE_MOUSEUP, PP_INPUTEVENT_MOUSEBUTTON_RIGHT);
  pp::ImageData image_data = CreateImageData(instance_);

  PP_Resource elements[] = {
    mouse_event_1.pp_resource(),
    mouse_event_2.pp_resource(),
    image_data.pp_resource()
  };
  size_t size = sizeof(elements) / sizeof(elements[0]);

  pp::ResourceArray_Dev resource_array(instance_, elements, size);
  ASSERT_EQ(0, resource_array[size]);
  ASSERT_EQ(0, resource_array[size + 1]);

  PASS();
}

std::string TestResourceArray::TestEmptyArray() {
  pp::ResourceArray_Dev resource_array(instance_, NULL, 0);
  ASSERT_EQ(0, resource_array.size());
  PASS();
}

std::string TestResourceArray::TestInvalidElement() {
  pp::InputEvent mouse_event = CreateMouseEvent(
      instance_, PP_INPUTEVENT_TYPE_MOUSEDOWN, PP_INPUTEVENT_MOUSEBUTTON_LEFT);
  pp::ImageData image_data = CreateImageData(instance_);

  PP_Resource elements[] = {
    mouse_event.pp_resource(),
    0,
    image_data.pp_resource()
  };
  size_t size = sizeof(elements) / sizeof(elements[0]);

  pp::ResourceArray_Dev resource_array(instance_, elements, size);

  ASSERT_EQ(size, resource_array.size());
  for (uint32_t index = 0; index < size; ++index)
    ASSERT_EQ(elements[index], resource_array[index]);

  PASS();
}
