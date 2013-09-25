// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_aura/desktop_screen_x11.h"

#include "base/message_loop/message_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/display_observer.h"

namespace views {

const int64 kFirstDisplay = 5321829;
const int64 kSecondDisplay = 928310;

class DesktopScreenX11Test : public testing::Test,
                             public gfx::DisplayObserver {
 public:
  DesktopScreenX11Test() {}
  virtual ~DesktopScreenX11Test() {}

  // Overridden from testing::Test:
  virtual void SetUp() OVERRIDE {
    // Initialize the world to the single monitor case.
    std::vector<gfx::Display> displays;
    displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
    screen_.reset(new DesktopScreenX11(displays));
    screen_->AddObserver(this);
  }
  virtual void TearDown() OVERRIDE {
    screen_.reset();
    message_loop_.RunUntilIdle();
  }

 protected:
  std::vector<gfx::Display> changed_display_;
  std::vector<gfx::Display> added_display_;
  std::vector<gfx::Display> removed_display_;

  DesktopScreenX11* screen() { return screen_.get(); }

  void ResetDisplayChanges() {
    changed_display_.clear();
    added_display_.clear();
    removed_display_.clear();
  }

 private:
  // Overridden from gfx::DisplayObserver:
  virtual void OnDisplayBoundsChanged(const gfx::Display& display) OVERRIDE {
    changed_display_.push_back(display);
  }

  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE {
    added_display_.push_back(new_display);
  }

  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE {
    removed_display_.push_back(old_display);
  }

  base::MessageLoopForUI message_loop_;
  scoped_ptr<DesktopScreenX11> screen_;

  DISALLOW_COPY_AND_ASSIGN(DesktopScreenX11Test);
};

TEST_F(DesktopScreenX11Test, BoundsChangeSingleMonitor) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(0u, added_display_.size());
  EXPECT_EQ(0u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, AddMonitorToTheRight) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(0u, changed_display_.size());
  EXPECT_EQ(1u, added_display_.size());
  EXPECT_EQ(0u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, AddMonitorToTheLeft) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kSecondDisplay, gfx::Rect(0, 0, 1024, 768)));
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(1024, 0, 640, 480)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(1u, added_display_.size());
  EXPECT_EQ(0u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, RemoveMonitorOnRight) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  ResetDisplayChanges();

  displays.clear();
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(0u, changed_display_.size());
  EXPECT_EQ(0u, added_display_.size());
  EXPECT_EQ(1u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, RemoveMonitorOnLeft) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  ResetDisplayChanges();

  displays.clear();
  displays.push_back(gfx::Display(kSecondDisplay, gfx::Rect(0, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(1u, changed_display_.size());
  EXPECT_EQ(0u, added_display_.size());
  EXPECT_EQ(1u, removed_display_.size());
}

TEST_F(DesktopScreenX11Test, GetDisplayNearestPoint) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(650, 10)).id());
  EXPECT_EQ(kFirstDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(10, 10)).id());
  EXPECT_EQ(kFirstDisplay,
            screen()->GetDisplayNearestPoint(gfx::Point(10000, 10000)).id());
}

TEST_F(DesktopScreenX11Test, GetDisplayMatchingBasic) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayMatching(gfx::Rect(700, 20, 100, 100)).id());
}

TEST_F(DesktopScreenX11Test, GetDisplayMatchingOverlap) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay, gfx::Rect(0, 0, 640, 480)));
  displays.push_back(gfx::Display(kSecondDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  screen()->ProcessDisplayChange(displays);

  EXPECT_EQ(kSecondDisplay,
            screen()->GetDisplayMatching(gfx::Rect(630, 20, 100, 100)).id());
}

TEST_F(DesktopScreenX11Test, GetPrimaryDisplay) {
  std::vector<gfx::Display> displays;
  displays.push_back(gfx::Display(kFirstDisplay,
                                  gfx::Rect(640, 0, 1024, 768)));
  displays.push_back(gfx::Display(kSecondDisplay, gfx::Rect(0, 0, 640, 480)));
  screen()->ProcessDisplayChange(displays);

  // The first display in the list is always the primary, even if other
  // displays are to the left in screen layout.
  EXPECT_EQ(kFirstDisplay, screen()->GetPrimaryDisplay().id());
}

}  // namespace views
