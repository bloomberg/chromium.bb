// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "services/ui/display/platform_screen.h"
#include "services/ui/display/platform_screen_ozone.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/chromeos/display_configurator.h"
#include "ui/display/display_switches.h"
#include "ui/display/fake_display_snapshot.h"
#include "ui/display/types/display_constants.h"
#include "ui/display/types/display_mode.h"
#include "ui/display/types/display_snapshot.h"
#include "ui/ozone/public/ozone_platform.h"

namespace display {

using ui::DisplayConfigurator;
using ui::DisplayMode;
using ui::DisplaySnapshot;
using ui::DisplaySnapshotVirtual;
using testing::IsEmpty;
using testing::SizeIs;

namespace {

// The ID of default "display" that gets added when running off device.
const int64_t kDefaultDisplayId = 1;

// Holds info about the display state we want to test.
struct DisplayState {
  int64_t id;
  gfx::Rect bounds;
  gfx::Size size;
  float device_scale_factor;
};

// Matchers that operate on DisplayState.
MATCHER_P(DisplayId, display_id, "") {
  *result_listener << "has id " << arg.id;
  return arg.id == display_id;
}

MATCHER_P(DisplaySize, size_string, "") {
  *result_listener << "has size " << arg.bounds.size().ToString();
  return arg.bounds.size().ToString() == size_string;
}

MATCHER_P(DisplayOrigin, origin_string, "") {
  *result_listener << "has origin " << arg.bounds.origin().ToString();
  return arg.bounds.origin().ToString() == origin_string;
}

// Make a DisplaySnapshot with specified id and size.
std::unique_ptr<DisplaySnapshot> MakeSnapshot(int64_t id,
                                              const gfx::Size& size) {
  return FakeDisplaySnapshot::Builder()
      .SetId(id)
      .SetNativeMode(size)
      .SetCurrentMode(size)
      .Build();
}

// Test delegate to track what functions calls the delegate receives.
class TestPlatformScreenDelegate : public PlatformScreenDelegate {
 public:
  TestPlatformScreenDelegate() {}
  ~TestPlatformScreenDelegate() override {}

  std::vector<DisplayState> added() { return added_; }
  std::vector<DisplayState> removed() { return removed_; }
  std::vector<DisplayState> modified() { return modified_; }

  void Reset() {
    added_.clear();
    removed_.clear();
    modified_.clear();
  }

 private:
  void OnDisplayAdded(int64_t id,
                      const gfx::Rect& bounds,
                      const gfx::Size& pixel_size,
                      float device_scale_factor) override {
    added_.push_back({id, bounds, pixel_size, device_scale_factor});
  }

  void OnDisplayRemoved(int64_t id) override {
    removed_.push_back({id, gfx::Rect(), gfx::Size(), 1.0f});
  }

  void OnDisplayModified(int64_t id,
                         const gfx::Rect& bounds,
                         const gfx::Size& pixel_size,
                         float device_scale_factor) override {
    modified_.push_back({id, bounds, pixel_size, device_scale_factor});
  }

  std::vector<DisplayState> added_;
  std::vector<DisplayState> removed_;
  std::vector<DisplayState> modified_;

  DISALLOW_COPY_AND_ASSIGN(TestPlatformScreenDelegate);
};

// Test fixture with helpers to act like ui::DisplayConfigurator and send
// OnDisplayModeChanged() to PlatformScreenOzone.
class PlatformScreenOzoneTest : public testing::Test {
 public:
  PlatformScreenOzoneTest() {}
  ~PlatformScreenOzoneTest() override {}

  PlatformScreen* platform_screen() { return platform_screen_.get(); }
  TestPlatformScreenDelegate* delegate() { return &delegate_; }

  // Adds a display snapshot with specified ID and default size.
  void AddDisplay(int64_t id) { return AddDisplay(id, gfx::Size(1024, 768)); }

  // Adds a display snapshot with specified ID and size to list of snapshots.
  void AddDisplay(int64_t id, const gfx::Size& size) {
    snapshots_.push_back(MakeSnapshot(id, size));
  }

  // Removes display snapshot with specified ID.
  void RemoveDisplay(int64_t id) {
    snapshots_.erase(
        std::remove_if(snapshots_.begin(), snapshots_.end(),
                       [id](std::unique_ptr<DisplaySnapshot>& snapshot) {
                         return snapshot->display_id() == id;
                       }));
  }

  // Modify the size of the display snapshot with specified ID.
  void ModifyDisplay(int64_t id, const gfx::Size& size) {
    DisplaySnapshot* snapshot = GetSnapshotById(id);

    const DisplayMode* new_mode = nullptr;
    for (auto& mode : snapshot->modes()) {
      if (mode->size() == size) {
        new_mode = mode.get();
        break;
      }
    }

    if (!new_mode) {
      snapshot->add_mode(new DisplayMode(size, false, 30.0f));
      new_mode = snapshot->modes().back().get();
    }

    snapshot->set_current_mode(new_mode);
  }

  // Calls OnDisplayModeChanged with our list of display snapshots.
  void TriggerOnDisplayModeChanged() {
    std::vector<DisplaySnapshot*> snapshots_ptrs;
    for (auto& snapshot : snapshots_) {
      snapshots_ptrs.push_back(snapshot.get());
    }
    static_cast<DisplayConfigurator::Observer*>(platform_screen_.get())
        ->OnDisplayModeChanged(snapshots_ptrs);
  }

 private:
  DisplaySnapshot* GetSnapshotById(int64_t id) {
    for (auto& snapshot : snapshots_) {
      if (snapshot->display_id() == id)
        return snapshot.get();
    }
    return nullptr;
  }

  // testing::Test:
  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitchNative(
        switches::kScreenConfig, "none");

    testing::Test::SetUp();
    ui::OzonePlatform::InitializeForUI();
    platform_screen_ = base::MakeUnique<PlatformScreenOzone>();
    platform_screen_->Init(&delegate_);

    // Have all tests start with a 1024x768 display by default.
    AddDisplay(kDefaultDisplayId, gfx::Size(1024, 768));
    TriggerOnDisplayModeChanged();

    // Double check the expected display exists and clear counters.
    ASSERT_THAT(delegate()->added(), SizeIs(1));
    ASSERT_THAT(delegate_.added()[0], DisplayId(kDefaultDisplayId));
    ASSERT_THAT(delegate_.added()[0], DisplayOrigin("0,0"));
    ASSERT_THAT(delegate_.added()[0], DisplaySize("1024x768"));
    delegate_.Reset();
  }

  void TearDown() override {
    snapshots_.clear();
    delegate_.Reset();
    platform_screen_.reset();
  }

  TestPlatformScreenDelegate delegate_;
  std::unique_ptr<PlatformScreenOzone> platform_screen_;
  std::vector<std::unique_ptr<DisplaySnapshot>> snapshots_;
};

}  // namespace

TEST_F(PlatformScreenOzoneTest, AddDisplay) {
  AddDisplay(2);
  TriggerOnDisplayModeChanged();

  // Check that display 2 was added.
  ASSERT_THAT(delegate()->added(), SizeIs(1));
  EXPECT_THAT(delegate()->added()[0], DisplayId(2));
  EXPECT_THAT(delegate()->removed(), IsEmpty());
  EXPECT_THAT(delegate()->modified(), IsEmpty());
}

TEST_F(PlatformScreenOzoneTest, RemoveDisplay) {
  AddDisplay(2);
  TriggerOnDisplayModeChanged();
  delegate()->Reset();

  RemoveDisplay(2);
  TriggerOnDisplayModeChanged();

  // Check that display 2 was removed.
  ASSERT_THAT(delegate()->removed(), SizeIs(1));
  EXPECT_THAT(delegate()->removed()[0], DisplayId(2));
  EXPECT_THAT(delegate()->added(), IsEmpty());
  EXPECT_THAT(delegate()->modified(), IsEmpty());
}

TEST_F(PlatformScreenOzoneTest, RemoveFirstDisplay) {
  AddDisplay(2);
  TriggerOnDisplayModeChanged();
  delegate()->Reset();

  RemoveDisplay(kDefaultDisplayId);
  TriggerOnDisplayModeChanged();

  // Check that the default display was removed and display 2 was modified due
  // to the origin changing.
  EXPECT_THAT(delegate()->added(), IsEmpty());
  ASSERT_THAT(delegate()->removed(), SizeIs(1));
  EXPECT_THAT(delegate()->removed()[0], DisplayId(kDefaultDisplayId));

  ASSERT_THAT(delegate()->modified(), SizeIs(1));
  EXPECT_THAT(delegate()->modified()[0], DisplayId(2));
  EXPECT_THAT(delegate()->modified()[0], DisplayOrigin("0,0"));
}

TEST_F(PlatformScreenOzoneTest, RemovePrimaryDisplay) {
  EXPECT_EQ(kDefaultDisplayId, platform_screen()->GetPrimaryDisplayId());

  AddDisplay(2);
  RemoveDisplay(kDefaultDisplayId);
  TriggerOnDisplayModeChanged();

  // Check the primary display changed because the old primary was removed.
  EXPECT_EQ(2, platform_screen()->GetPrimaryDisplayId());
}

TEST_F(PlatformScreenOzoneTest, RemoveMultipleDisplay) {
  AddDisplay(2);
  AddDisplay(3);
  TriggerOnDisplayModeChanged();
  delegate()->Reset();

  RemoveDisplay(2);
  TriggerOnDisplayModeChanged();

  // Check that display 2 was removed.
  ASSERT_THAT(delegate()->removed(), SizeIs(1));
  EXPECT_THAT(delegate()->removed()[0], DisplayId(2));

  delegate()->Reset();
  RemoveDisplay(3);
  TriggerOnDisplayModeChanged();

  // Check that display 3 was removed.
  ASSERT_THAT(delegate()->removed(), SizeIs(1));
  EXPECT_THAT(delegate()->removed()[0], DisplayId(3));
}

TEST_F(PlatformScreenOzoneTest, ModifyDisplaySize) {
  const gfx::Size size1(1920, 1200);
  const gfx::Size size2(1680, 1050);

  AddDisplay(2, size1);
  TriggerOnDisplayModeChanged();

  // Check that display 2 was added with expected size.
  ASSERT_THAT(delegate()->added(), SizeIs(1));
  EXPECT_THAT(delegate()->added()[0], DisplayId(2));
  EXPECT_THAT(delegate()->added()[0], DisplaySize(size1.ToString()));
  delegate()->Reset();

  ModifyDisplay(2, size2);
  TriggerOnDisplayModeChanged();

  // Check that display 2 was modified to have the new expected size.
  ASSERT_THAT(delegate()->modified(), SizeIs(1));
  EXPECT_THAT(delegate()->modified()[0], DisplayId(2));
  EXPECT_THAT(delegate()->modified()[0], DisplaySize(size2.ToString()));
}

TEST_F(PlatformScreenOzoneTest, ModifyFirstDisplaySize) {
  const gfx::Size size(1920, 1200);

  AddDisplay(2, size);
  TriggerOnDisplayModeChanged();

  // Check that display two has the expected initial origin.
  ASSERT_THAT(delegate()->added(), SizeIs(1));
  EXPECT_THAT(delegate()->added()[0], DisplayOrigin("1024,0"));
  delegate()->Reset();

  ModifyDisplay(kDefaultDisplayId, size);
  TriggerOnDisplayModeChanged();

  // Check that the default display was modified with a new size and display 2
  // was modified with a new origin.
  ASSERT_THAT(delegate()->modified(), SizeIs(2));
  EXPECT_THAT(delegate()->modified()[0], DisplayId(kDefaultDisplayId));
  EXPECT_THAT(delegate()->modified()[0], DisplaySize(size.ToString()));
  EXPECT_THAT(delegate()->modified()[1], DisplayId(2));
  EXPECT_THAT(delegate()->modified()[1], DisplayOrigin("1920,0"));
}

}  // namespace display
