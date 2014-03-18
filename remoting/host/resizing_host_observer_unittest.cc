// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/resizing_host_observer.h"
#include "remoting/host/screen_resolution.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_geometry.h"

namespace remoting {

std::ostream& operator<<(std::ostream& os, const ScreenResolution& resolution) {
  return os << resolution.dimensions().width() << "x"
            << resolution.dimensions().height() << " @ "
            << resolution.dpi().x() << "x" << resolution.dpi().y();
}

bool operator==(const ScreenResolution& a, const ScreenResolution& b) {
  return a.Equals(b);
}

const int kDefaultDPI = 96;

ScreenResolution MakeResolution(int width, int height) {
  return ScreenResolution(webrtc::DesktopSize(width, height),
                          webrtc::DesktopVector(kDefaultDPI, kDefaultDPI));
}

class FakeDesktopResizer : public DesktopResizer {
 public:
  FakeDesktopResizer(const ScreenResolution& initial_resolution,
                     bool exact_size_supported,
                     const ScreenResolution* supported_resolutions,
                     int num_supported_resolutions,
                     int* restore_resolution_call_count)
      : initial_resolution_(initial_resolution),
        current_resolution_(initial_resolution),
        exact_size_supported_(exact_size_supported),
        set_resolution_call_count_(0),
        restore_resolution_call_count_(restore_resolution_call_count) {
    for (int i = 0; i < num_supported_resolutions; ++i) {
      supported_resolutions_.push_back(supported_resolutions[i]);
    }
  }

  virtual ~FakeDesktopResizer() {
    EXPECT_EQ(initial_resolution_, GetCurrentResolution());
  }

  int set_resolution_call_count() { return set_resolution_call_count_; }

  // remoting::DesktopResizer interface
  virtual ScreenResolution GetCurrentResolution() OVERRIDE {
    return current_resolution_;
  }
  virtual std::list<ScreenResolution> GetSupportedResolutions(
      const ScreenResolution& preferred) OVERRIDE {
    std::list<ScreenResolution> result = supported_resolutions_;
    if (exact_size_supported_) {
      result.push_back(preferred);
    }
    return result;
  }
  virtual void SetResolution(const ScreenResolution& resolution) OVERRIDE {
    current_resolution_ = resolution;
    ++set_resolution_call_count_;
  }
  virtual void RestoreResolution(const ScreenResolution& resolution) OVERRIDE {
    current_resolution_ = resolution;
    if (restore_resolution_call_count_)
      ++(*restore_resolution_call_count_);
  }

 private:
  ScreenResolution initial_resolution_;
  ScreenResolution current_resolution_;
  bool exact_size_supported_;
  std::list<ScreenResolution> supported_resolutions_;

  int set_resolution_call_count_;
  int* restore_resolution_call_count_;
};

class ResizingHostObserverTest : public testing::Test {
 public:
  ResizingHostObserverTest()
      : desktop_resizer_(NULL),
        now_(base::Time::Now()) {
  }

  // This needs to be public because the derived test-case class needs to
  // pass it to Bind, which fails if it's protected.
  base::Time GetTime() {
    return now_;
  }

 protected:
  void SetDesktopResizer(scoped_ptr<FakeDesktopResizer> desktop_resizer) {
    CHECK(!desktop_resizer_) << "Call SetDeskopResizer once per test";
    desktop_resizer_ = desktop_resizer.get();

    resizing_host_observer_.reset(
        new ResizingHostObserver(desktop_resizer.PassAs<DesktopResizer>()));
    resizing_host_observer_->SetNowFunctionForTesting(
        base::Bind(&ResizingHostObserverTest::GetTimeAndIncrement,
                   base::Unretained(this)));
  }

  ScreenResolution GetBestResolution(const ScreenResolution& client_size) {
    resizing_host_observer_->SetScreenResolution(client_size);
    return desktop_resizer_->GetCurrentResolution();
  }

  void VerifySizes(const ScreenResolution* client_sizes,
                   const ScreenResolution* expected_sizes,
                   int number_of_sizes) {
    for (int i = 0; i < number_of_sizes; ++i) {
      ScreenResolution best_size = GetBestResolution(client_sizes[i]);
      EXPECT_EQ(expected_sizes[i], best_size)
          << "Input resolution = " << client_sizes[i];
    }
  }

  base::Time GetTimeAndIncrement() {
    base::Time result = now_;
    now_ += base::TimeDelta::FromSeconds(1);
    return result;
  }

  scoped_ptr<ResizingHostObserver> resizing_host_observer_;
  FakeDesktopResizer* desktop_resizer_;
  base::Time now_;
};

// Check that the resolution isn't restored if it wasn't changed by this class.
TEST_F(ResizingHostObserverTest, NoRestoreResolution) {
  int restore_resolution_call_count = 0;
  ScreenResolution initial = MakeResolution(640, 480);
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(initial, false, NULL, 0,
                             &restore_resolution_call_count));
  SetDesktopResizer(desktop_resizer.Pass());
  VerifySizes(NULL, NULL, 0);
  resizing_host_observer_.reset();
  EXPECT_EQ(0, restore_resolution_call_count);
}

// Check that the host is not resized if GetSupportedSizes returns an empty
// list (even if GetCurrentSize is supported).
TEST_F(ResizingHostObserverTest, EmptyGetSupportedSizes) {
  int restore_resolution_call_count = 0;
  ScreenResolution initial = MakeResolution(640, 480);
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(initial, false, NULL, 0,
                             &restore_resolution_call_count));
  SetDesktopResizer(desktop_resizer.Pass());

  ScreenResolution client_sizes[] = { MakeResolution(200, 100),
                                      MakeResolution(100, 200) };
  ScreenResolution expected_sizes[] = { initial, initial };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));

  resizing_host_observer_.reset();
  EXPECT_EQ(0, restore_resolution_call_count);
}

// Check that if the implementation supports exact size matching, it is used.
TEST_F(ResizingHostObserverTest, SelectExactSize) {
  int restore_resolution_call_count = 0;
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
        new FakeDesktopResizer(MakeResolution(640, 480), true, NULL, 0,
                               &restore_resolution_call_count));
  SetDesktopResizer(desktop_resizer.Pass());

  ScreenResolution client_sizes[] = { MakeResolution(200, 100),
                                      MakeResolution(100, 200),
                                      MakeResolution(640, 480),
                                      MakeResolution(480, 640),
                                      MakeResolution(1280, 1024) };
  VerifySizes(client_sizes, client_sizes, arraysize(client_sizes));
  resizing_host_observer_.reset();
  EXPECT_EQ(1, restore_resolution_call_count);
}

// Check that if the implementation supports a size that is no larger than
// the requested size, then the largest such size is used.
TEST_F(ResizingHostObserverTest, SelectBestSmallerSize) {
  ScreenResolution supported_sizes[] = { MakeResolution(639, 479),
                                         MakeResolution(640, 480) };
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(MakeResolution(640, 480), false,
                             supported_sizes, arraysize(supported_sizes),
                             NULL));
  SetDesktopResizer(desktop_resizer.Pass());

  ScreenResolution client_sizes[] = { MakeResolution(639, 479),
                                      MakeResolution(640, 480),
                                      MakeResolution(641, 481),
                                      MakeResolution(999, 999) };
  ScreenResolution expected_sizes[] = { supported_sizes[0], supported_sizes[1],
                               supported_sizes[1], supported_sizes[1] };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports only sizes that are larger than
// the requested size, then the one that requires the least down-scaling.
TEST_F(ResizingHostObserverTest, SelectBestScaleFactor) {
  ScreenResolution supported_sizes[] = { MakeResolution(100, 100),
                                         MakeResolution(200, 100) };
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(MakeResolution(200, 100), false,
                             supported_sizes, arraysize(supported_sizes),
                             NULL));
  SetDesktopResizer(desktop_resizer.Pass());

  ScreenResolution client_sizes[] = { MakeResolution(1, 1),
                                      MakeResolution(99, 99),
                                      MakeResolution(199, 99) };
  ScreenResolution expected_sizes[] = { supported_sizes[0], supported_sizes[0],
                               supported_sizes[1] };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports two sizes that have the same
// resultant scale factor, then the widest one is selected.
TEST_F(ResizingHostObserverTest, SelectWidest) {
  ScreenResolution supported_sizes[] = { MakeResolution(640, 480),
                                         MakeResolution(480, 640) };
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(MakeResolution(480, 640), false,
                             supported_sizes, arraysize(supported_sizes),
                             NULL));
  SetDesktopResizer(desktop_resizer.Pass());

  ScreenResolution client_sizes[] = { MakeResolution(100, 100),
                                      MakeResolution(480, 480),
                                      MakeResolution(500, 500),
                                      MakeResolution(640, 640),
                                      MakeResolution(1000, 1000) };
  ScreenResolution expected_sizes[] = { supported_sizes[0], supported_sizes[0],
                               supported_sizes[0], supported_sizes[0],
                               supported_sizes[0] };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the best match for the client size doesn't change, then we
// don't call SetSize.
TEST_F(ResizingHostObserverTest, NoSetSizeForSameSize) {
  ScreenResolution supported_sizes[] = { MakeResolution(640, 480),
                                         MakeResolution(480, 640) };
  FakeDesktopResizer* desktop_resizer =
      new FakeDesktopResizer(MakeResolution(480, 640), false,
                             supported_sizes, arraysize(supported_sizes), NULL);
  SetDesktopResizer(scoped_ptr<FakeDesktopResizer>(desktop_resizer));

  ScreenResolution client_sizes[] = { MakeResolution(640, 640),
                                      MakeResolution(1024, 768),
                                      MakeResolution(640, 480) };
  ScreenResolution expected_sizes[] = { MakeResolution(640, 480),
                                        MakeResolution(640, 480),
                                        MakeResolution(640, 480) };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
  EXPECT_EQ(desktop_resizer->set_resolution_call_count(), 1);
}

// Check that desktop resizes are rate-limited, and that if multiple resize
// requests are received in the time-out period, the most recent is respected.
TEST_F(ResizingHostObserverTest, RateLimited) {
  FakeDesktopResizer* desktop_resizer =
      new FakeDesktopResizer(MakeResolution(640, 480), true, NULL, 0, NULL);
  SetDesktopResizer(scoped_ptr<FakeDesktopResizer>(desktop_resizer));
  resizing_host_observer_->SetNowFunctionForTesting(
      base::Bind(&ResizingHostObserverTest::GetTime, base::Unretained(this)));

  base::MessageLoop message_loop;
  base::RunLoop run_loop;

  EXPECT_EQ(GetBestResolution(MakeResolution(100, 100)),
            MakeResolution(100, 100));
  now_ += base::TimeDelta::FromMilliseconds(900);
  EXPECT_EQ(GetBestResolution(MakeResolution(200, 200)),
            MakeResolution(100, 100));
  now_ += base::TimeDelta::FromMilliseconds(99);
  EXPECT_EQ(GetBestResolution(MakeResolution(300, 300)),
            MakeResolution(100, 100));
  now_ += base::TimeDelta::FromMilliseconds(1);

  // Due to the kMinimumResizeIntervalMs constant in resizing_host_observer.cc,
  // We need to wait a total of 1000ms for the final resize to be processed.
  // Since it was queued 900 + 99 ms after the first, we need to wait an
  // additional 1ms. However, since RunLoop is not guaranteed to process tasks
  // with the same due time in FIFO order, wait an additional 1ms for safety.
  message_loop.PostDelayedTask(
      FROM_HERE,
      run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(2));
  run_loop.Run();

  // If the QuitClosure fired before the final resize, it's a test failure.
  EXPECT_EQ(desktop_resizer_->GetCurrentResolution(), MakeResolution(300, 300));
}

}  // namespace remoting
