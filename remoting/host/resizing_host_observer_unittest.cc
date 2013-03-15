// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/resizing_host_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSize.h"

std::ostream& operator<<(std::ostream& os, const SkISize& size) {
  return os << size.width() << "x" << size.height();
}

namespace remoting {

class FakeDesktopResizer : public DesktopResizer {
 public:
  FakeDesktopResizer(const SkISize& initial_size, bool exact_size_supported,
                     const SkISize* supported_sizes, int num_supported_sizes)
      : initial_size_(initial_size),
        current_size_(initial_size),
        exact_size_supported_(exact_size_supported),
        set_size_call_count_(0) {
    for (int i = 0; i < num_supported_sizes; ++i) {
      supported_sizes_.push_back(supported_sizes[i]);
    }
  }

  virtual ~FakeDesktopResizer() {
    EXPECT_EQ(initial_size_, GetCurrentSize());
  }

  int set_size_call_count() { return set_size_call_count_; }

  // remoting::DesktopResizer interface
  virtual SkISize GetCurrentSize() OVERRIDE {
    return current_size_;
  }
  virtual std::list<SkISize> GetSupportedSizes(
      const SkISize& preferred) OVERRIDE {
    std::list<SkISize> result = supported_sizes_;
    if (exact_size_supported_) {
      result.push_back(preferred);
    }
    return result;
  }
  virtual void SetSize(const SkISize& size) OVERRIDE {
    current_size_ = size;
    ++set_size_call_count_;
  }
  virtual void RestoreSize(const SkISize& size) OVERRIDE {
    current_size_ = size;
  }

 private:
  SkISize initial_size_;
  SkISize current_size_;
  bool exact_size_supported_;
  std::list<SkISize> supported_sizes_;

  int set_size_call_count_;
};

class ResizingHostObserverTest : public testing::Test {
 public:
  ResizingHostObserverTest() : desktop_resizer_(NULL) {
  }

  void SetDesktopResizer(scoped_ptr<FakeDesktopResizer> desktop_resizer) {
    CHECK(!desktop_resizer_) << "Call SetDeskopResizer once per test";
    desktop_resizer_ = desktop_resizer.get();

    resizing_host_observer_.reset(
        new ResizingHostObserver(desktop_resizer.PassAs<DesktopResizer>()));
  }

  SkISize GetBestSize(const SkISize& client_size) {
    resizing_host_observer_->OnClientResolutionChanged(SkIPoint(), client_size);
    return desktop_resizer_->GetCurrentSize();
  }

  void VerifySizes(const SkISize* client_sizes, const SkISize* expected_sizes,
                   int number_of_sizes) {
    for (int i = 0; i < number_of_sizes; ++i) {
      SkISize best_size = GetBestSize(client_sizes[i]);
      EXPECT_EQ(expected_sizes[i], best_size)
          << "Input size = " << client_sizes[i];
    }
  }

 private:
  scoped_ptr<ResizingHostObserver> resizing_host_observer_;
  FakeDesktopResizer* desktop_resizer_;
};

// Check that the host is not resized if GetSupportedSizes returns an empty
// list (even if GetCurrentSize is supported).
TEST_F(ResizingHostObserverTest, EmptyGetSupportedSizes) {
  SkISize initial = { 640, 480 };
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(initial, false, NULL, 0));
  SetDesktopResizer(desktop_resizer.Pass());

  SkISize client_sizes[] = { { 200, 100 }, { 100, 200 } };
  SkISize expected_sizes[] = { initial, initial };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports exact size matching, it is used.
TEST_F(ResizingHostObserverTest, SelectExactSize) {
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(SkISize::Make(640, 480), true, NULL, 0));
  SetDesktopResizer(desktop_resizer.Pass());

  SkISize client_sizes[] = { { 200, 100 }, { 100, 200 } , { 640, 480 },
                             { 480, 640 }, { 1280, 1024 } };
  VerifySizes(client_sizes, client_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports a size that is no larger than
// the requested size, then the largest such size is used.
TEST_F(ResizingHostObserverTest, SelectBestSmallerSize) {
  SkISize supported_sizes[] = {
    SkISize::Make(639, 479), SkISize::Make(640, 480) };
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(SkISize::Make(640, 480), false,
                             supported_sizes, arraysize(supported_sizes)));
  SetDesktopResizer(desktop_resizer.Pass());

  SkISize client_sizes[] = { { 639, 479 }, { 640, 480 }, { 641, 481 },
                             { 999, 999 } };
  SkISize expected_sizes[] = { supported_sizes[0], supported_sizes[1],
                               supported_sizes[1], supported_sizes[1] };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports only sizes that are larger than
// the requested size, then the one that requires the least down-scaling.
TEST_F(ResizingHostObserverTest, SelectBestScaleFactor) {
  SkISize supported_sizes[] = { { 100, 100 }, { 200, 100 } };
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(SkISize::Make(200, 100), false,
                             supported_sizes, arraysize(supported_sizes)));
  SetDesktopResizer(desktop_resizer.Pass());

  SkISize client_sizes[] = { { 1, 1 }, { 99, 99 }, { 199, 99 } };
  SkISize expected_sizes[] = { supported_sizes[0], supported_sizes[0],
                               supported_sizes[1] };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports two sizes that have the same
// resultant scale factor, then the widest one is selected.
TEST_F(ResizingHostObserverTest, SelectWidest) {
  SkISize supported_sizes[] = { { 640, 480 }, { 480, 640 } };
  scoped_ptr<FakeDesktopResizer> desktop_resizer(
      new FakeDesktopResizer(SkISize::Make(480, 640), false,
                             supported_sizes, arraysize(supported_sizes)));
  SetDesktopResizer(desktop_resizer.Pass());

  SkISize client_sizes[] = { { 100, 100 }, { 480, 480 }, { 500, 500 },
                             { 640, 640 }, { 1000, 1000 } };
  SkISize expected_sizes[] = { supported_sizes[0], supported_sizes[0],
                               supported_sizes[0], supported_sizes[0],
                               supported_sizes[0] };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the best match for the client size doesn't change, then we
// don't call SetSize.
TEST_F(ResizingHostObserverTest, NoSetSizeForSameSize) {
  SkISize supported_sizes[] = { { 640, 480 }, { 480, 640 } };
  FakeDesktopResizer* desktop_resizer =
      new FakeDesktopResizer(SkISize::Make(640, 480), false,
                             supported_sizes, arraysize(supported_sizes));
  SetDesktopResizer(scoped_ptr<FakeDesktopResizer>(desktop_resizer));

  SkISize client_sizes[] = { { 640, 640 }, { 1024, 768 }, { 640, 480 } };
  SkISize expected_sizes[] = { { 640, 480 }, { 640, 480 }, { 640, 480 } };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
  EXPECT_EQ(desktop_resizer->set_size_call_count(), 0);
}

}  // namespace remoting
