// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display.h"
#include "ui/display/display_layout.h"
#include "ui/display/mojo/display_struct_traits_test.mojom.h"
#include "ui/display/types/display_mode.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace display {
namespace {

constexpr int64_t kDisplayId1 = 123;
constexpr int64_t kDisplayId2 = 456;
constexpr int64_t kDisplayId3 = 789;

class DisplayStructTraitsTest : public testing::Test,
                                public mojom::DisplayStructTraitsTest {
 public:
  DisplayStructTraitsTest() {}

 protected:
  mojom::DisplayStructTraitsTestPtr GetTraitsTestProxy() {
    return traits_test_bindings_.CreateInterfacePtrAndBind(this);
  }

 private:
  // mojom::DisplayStructTraitsTest:
  void EchoDisplay(const Display& in,
                   const EchoDisplayCallback& callback) override {
    callback.Run(in);
  }

  void EchoDisplayMode(std::unique_ptr<DisplayMode> in,
                       const EchoDisplayModeCallback& callback) override {
    callback.Run(std::move(in));
  }

  void EchoDisplayPlacement(
      const DisplayPlacement& in,
      const EchoDisplayPlacementCallback& callback) override {
    callback.Run(in);
  }

  void EchoDisplayLayout(std::unique_ptr<display::DisplayLayout> in,
                         const EchoDisplayLayoutCallback& callback) override {
    callback.Run(std::move(in));
  }

  base::MessageLoop loop_;  // A MessageLoop is needed for Mojo IPC to work.
  mojo::BindingSet<mojom::DisplayStructTraitsTest> traits_test_bindings_;

  DISALLOW_COPY_AND_ASSIGN(DisplayStructTraitsTest);
};

void CheckDisplaysEqual(const Display& input, const Display& output) {
  EXPECT_NE(&input, &output);  // Make sure they aren't the same object.
  EXPECT_EQ(input.id(), output.id());
  EXPECT_EQ(input.bounds(), output.bounds());
  EXPECT_EQ(input.work_area(), output.work_area());
  EXPECT_EQ(input.device_scale_factor(), output.device_scale_factor());
  EXPECT_EQ(input.rotation(), output.rotation());
  EXPECT_EQ(input.touch_support(), output.touch_support());
  EXPECT_EQ(input.maximum_cursor_size(), output.maximum_cursor_size());
}

void CheckDisplayLayoutsEqual(const DisplayLayout& input,
                              const DisplayLayout& output) {
  EXPECT_NE(&input, &output);  // Make sure they aren't the same object.
  EXPECT_EQ(input.placement_list, output.placement_list);
  EXPECT_EQ(input.mirrored, output.mirrored);
  EXPECT_EQ(input.default_unified, output.default_unified);
  EXPECT_EQ(input.primary_id, output.primary_id);
}

}  // namespace

TEST_F(DisplayStructTraitsTest, DefaultDisplayValues) {
  Display input(5);

  Display output;
  GetTraitsTestProxy()->EchoDisplay(input, &output);

  CheckDisplaysEqual(input, output);
}

TEST_F(DisplayStructTraitsTest, SetAllDisplayValues) {
  const gfx::Rect bounds(100, 200, 500, 600);
  const gfx::Rect work_area(150, 250, 400, 500);
  const gfx::Size maximum_cursor_size(64, 64);

  Display input(246345234, bounds);
  input.set_work_area(work_area);
  input.set_device_scale_factor(2.0f);
  input.set_rotation(Display::ROTATE_270);
  input.set_touch_support(Display::TOUCH_SUPPORT_AVAILABLE);
  input.set_maximum_cursor_size(maximum_cursor_size);

  Display output;
  GetTraitsTestProxy()->EchoDisplay(input, &output);

  CheckDisplaysEqual(input, output);
}

TEST_F(DisplayStructTraitsTest, DefaultDisplayMode) {
  std::unique_ptr<DisplayMode> input =
      base::MakeUnique<DisplayMode>(gfx::Size(1024, 768), true, 61.0);

  mojom::DisplayStructTraitsTestPtr proxy = GetTraitsTestProxy();
  std::unique_ptr<DisplayMode> output;

  proxy->EchoDisplayMode(input->Clone(), &output);

  // We want to test each component individually to make sure each data member
  // was correctly serialized and deserialized.
  EXPECT_EQ(input->size(), output->size());
  EXPECT_EQ(input->is_interlaced(), output->is_interlaced());
  EXPECT_EQ(input->refresh_rate(), output->refresh_rate());
}

TEST_F(DisplayStructTraitsTest, DisplayPlacementFlushAtTop) {
  DisplayPlacement input;
  input.display_id = kDisplayId1;
  input.parent_display_id = kDisplayId2;
  input.position = DisplayPlacement::TOP;
  input.offset = 0;
  input.offset_reference = DisplayPlacement::TOP_LEFT;

  DisplayPlacement output;
  GetTraitsTestProxy()->EchoDisplayPlacement(input, &output);

  EXPECT_EQ(input, output);
}

TEST_F(DisplayStructTraitsTest, DisplayPlacementWithOffset) {
  DisplayPlacement input;
  input.display_id = kDisplayId1;
  input.parent_display_id = kDisplayId2;
  input.position = DisplayPlacement::BOTTOM;
  input.offset = -100;
  input.offset_reference = DisplayPlacement::BOTTOM_RIGHT;

  DisplayPlacement output;
  GetTraitsTestProxy()->EchoDisplayPlacement(input, &output);

  EXPECT_EQ(input, output);
}

TEST_F(DisplayStructTraitsTest, DisplayLayoutTwoExtended) {
  DisplayPlacement placement;
  placement.display_id = kDisplayId1;
  placement.parent_display_id = kDisplayId2;
  placement.position = DisplayPlacement::RIGHT;
  placement.offset = 0;
  placement.offset_reference = DisplayPlacement::TOP_LEFT;

  auto input = base::MakeUnique<DisplayLayout>();
  input->placement_list.push_back(placement);
  input->primary_id = kDisplayId2;
  input->mirrored = false;
  input->default_unified = true;

  std::unique_ptr<DisplayLayout> output;
  GetTraitsTestProxy()->EchoDisplayLayout(input->Copy(), &output);

  CheckDisplayLayoutsEqual(*input, *output);
}

TEST_F(DisplayStructTraitsTest, DisplayLayoutThreeExtended) {
  DisplayPlacement placement1;
  placement1.display_id = kDisplayId2;
  placement1.parent_display_id = kDisplayId1;
  placement1.position = DisplayPlacement::LEFT;
  placement1.offset = 0;
  placement1.offset_reference = DisplayPlacement::TOP_LEFT;

  DisplayPlacement placement2;
  placement2.display_id = kDisplayId3;
  placement2.parent_display_id = kDisplayId1;
  placement2.position = DisplayPlacement::RIGHT;
  placement2.offset = -100;
  placement2.offset_reference = DisplayPlacement::BOTTOM_RIGHT;

  auto input = base::MakeUnique<DisplayLayout>();
  input->placement_list.push_back(placement1);
  input->placement_list.push_back(placement2);
  input->primary_id = kDisplayId1;
  input->mirrored = false;
  input->default_unified = false;

  std::unique_ptr<DisplayLayout> output;
  GetTraitsTestProxy()->EchoDisplayLayout(input->Copy(), &output);

  CheckDisplayLayoutsEqual(*input, *output);
}

TEST_F(DisplayStructTraitsTest, DisplayLayoutTwoMirrored) {
  DisplayPlacement placement;
  placement.display_id = kDisplayId1;
  placement.parent_display_id = kDisplayId2;
  placement.position = DisplayPlacement::RIGHT;
  placement.offset = 0;
  placement.offset_reference = DisplayPlacement::TOP_LEFT;

  auto input = base::MakeUnique<DisplayLayout>();
  input->placement_list.push_back(placement);
  input->primary_id = kDisplayId2;
  input->mirrored = true;
  input->default_unified = true;

  std::unique_ptr<DisplayLayout> output;
  GetTraitsTestProxy()->EchoDisplayLayout(input->Copy(), &output);

  CheckDisplayLayoutsEqual(*input, *output);
}

}  // namespace display
