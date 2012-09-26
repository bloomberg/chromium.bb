// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resizing_host_observer.h"
#include "remoting/host/desktop_resizer.h"

#include <list>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSize.h"

std::ostream& operator<<(std::ostream& os, const SkISize& size) {
  return os << size.width() << "x" << size.height();
}

namespace remoting {

class MockDesktopResizer : public DesktopResizer {
 public:
  MockDesktopResizer(const SkISize& initial_size, bool exact_size_supported,
                     const SkISize* supported_sizes, int num_supported_sizes)
      : initial_size_(initial_size),
        current_size_(initial_size),
        exact_size_supported_(exact_size_supported) {
    for (int i = 0; i < num_supported_sizes; ++i) {
      supported_sizes_.push_back(supported_sizes[i]);
    }
  }

  const SkISize& initial_size() { return initial_size_; }

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
  }
  virtual void RestoreSize(const SkISize& size) OVERRIDE {
    current_size_ = size;
  }

 private:
  SkISize initial_size_;
  SkISize current_size_;
  bool exact_size_supported_;
  std::list<SkISize> supported_sizes_;
};

class ResizingHostObserverTest : public testing::Test {
 public:
  void SetDesktopResizer(MockDesktopResizer* desktop_resizer) {
    CHECK(!desktop_resizer_.get()) << "Call SetDeskopResizer once per test";
    resizing_host_observer_.reset(new ResizingHostObserver(desktop_resizer,
                                                           NULL));
    desktop_resizer_.reset(desktop_resizer);
    resizing_host_observer_->OnClientAuthenticated("");
  }

  SkISize GetBestSize(const SkISize& client_size) {
    resizing_host_observer_->OnClientDimensionsChanged("", client_size);
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

  void Reconnect() {
    resizing_host_observer_->OnClientDisconnected("");
    resizing_host_observer_->OnClientAuthenticated("");
  }

  // testing::Test interface
  virtual void TearDown() OVERRIDE {
    resizing_host_observer_->OnClientDisconnected("");
    EXPECT_EQ(desktop_resizer_->initial_size(),
              desktop_resizer_->GetCurrentSize());
  }

 private:
  scoped_ptr<ResizingHostObserver> resizing_host_observer_;
  scoped_ptr<MockDesktopResizer> desktop_resizer_;
};

// Check that the host is not resized if it reports an initial size of zero
// (even if it GetSupportedSizes does not return an empty list).
TEST_F(ResizingHostObserverTest, ZeroGetCurrentSize) {
  SkISize zero = { 0, 0 };
  SetDesktopResizer(
      new MockDesktopResizer(zero, true, NULL, 0));
  SkISize client_sizes[] = { { 200, 100 }, { 100, 200 } };
  SkISize expected_sizes[] = { zero, zero };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that the host is not resized if GetSupportedSizes returns an empty
// list (even if GetCurrentSize is supported).
TEST_F(ResizingHostObserverTest, EmptyGetSupportedSizes) {
  SkISize initial = { 640, 480 };
  SetDesktopResizer(
      new MockDesktopResizer(initial, false, NULL, 0));
  SkISize client_sizes[] = { { 200, 100 }, { 100, 200 } };
  SkISize expected_sizes[] = { initial, initial };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports exact size matching, it is used.
TEST_F(ResizingHostObserverTest, SelectExactSize) {
  SetDesktopResizer(
      new MockDesktopResizer(SkISize::Make(640, 480), true, NULL, 0));
  SkISize client_sizes[] = { { 200, 100 }, { 100, 200 } , { 640, 480 },
                             { 480, 640 }, { 1280, 1024 } };
  VerifySizes(client_sizes, client_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports a size that is no larger than
// the requested size, then the largest such size is used.
TEST_F(ResizingHostObserverTest, SelectBestSmallerSize) {
  SkISize supported_sizes[] = {
    SkISize::Make(639, 479), SkISize::Make(640, 480) };
  SetDesktopResizer(
      new MockDesktopResizer(SkISize::Make(640, 480), false,
                             supported_sizes, arraysize(supported_sizes)));
  SkISize client_sizes[] = { { 639, 479 }, { 640, 480 }, { 641, 481 },
                             { 999, 999 } };
  SkISize expected_sizes[] = { supported_sizes[0], supported_sizes[1],
                               supported_sizes[1], supported_sizes[1] };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports only sizes that are larger than
// the requested size, then the one that requires the least down-scaling.
TEST_F(ResizingHostObserverTest, SelectBestScaleFactor) {
  SkISize supported_sizes[] = {
    SkISize::Make(100, 100), SkISize::Make(200, 100) };
  SetDesktopResizer(
      new MockDesktopResizer(SkISize::Make(200, 100), false,
                             supported_sizes, arraysize(supported_sizes)));
  SkISize client_sizes[] = { { 1, 1 }, { 99, 99 }, { 199, 99 } };
  SkISize expected_sizes[] = { supported_sizes[0], supported_sizes[0],
                               supported_sizes[1] };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that if the implementation supports two sizes that have the same
// resultant scale factor, then the widest one is selected.
TEST_F(ResizingHostObserverTest, SelectWidest) {
  SkISize supported_sizes[] = {
    SkISize::Make(640, 480), SkISize::Make(480, 640) };
  SetDesktopResizer(
      new MockDesktopResizer(SkISize::Make(480, 640), false,
                             supported_sizes, arraysize(supported_sizes)));
  SkISize client_sizes[] = { { 100, 100 }, { 480, 480 }, { 500, 500 },
                             { 640, 640 }, { 1000, 1000 } };
  SkISize expected_sizes[] = { supported_sizes[0], supported_sizes[0],
                               supported_sizes[0], supported_sizes[0],
                               supported_sizes[0] };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
}

// Check that resize-to-client is disabled if the size is changed explicitly.
TEST_F(ResizingHostObserverTest, ManualResize) {
  MockDesktopResizer* desktop_resizer =
      new MockDesktopResizer(SkISize::Make(640, 480), true, NULL, 0);
  SetDesktopResizer(desktop_resizer);
  SkISize client_sizes[] = { { 1, 1 }, { 2, 2 } , { 3, 3 } };
  VerifySizes(client_sizes, client_sizes, arraysize(client_sizes));
  SkISize explicit_size = SkISize::Make(640, 480);
  desktop_resizer->SetSize(explicit_size);
  SkISize expected_sizes[] = { explicit_size, explicit_size, explicit_size };
  VerifySizes(client_sizes, expected_sizes, arraysize(client_sizes));
  // Make sure this behaviour doesn't persist across reconnect.
  Reconnect();
  VerifySizes(client_sizes, client_sizes, arraysize(client_sizes));
}

}  // namespace remoting
