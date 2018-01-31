// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/presentation/PresentationAvailabilityState.h"

#include "modules/presentation/MockPresentationService.h"
#include "modules/presentation/PresentationAvailabilityCallbacks.h"
#include "modules/presentation/PresentationAvailabilityObserver.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebCallbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace blink {

using mojom::blink::ScreenAvailability;

class MockPresentationAvailabilityObserver
    : public PresentationAvailabilityObserver {
 public:
  explicit MockPresentationAvailabilityObserver(const Vector<KURL>& urls)
      : urls_(urls) {}
  ~MockPresentationAvailabilityObserver() override = default;

  MOCK_METHOD1(AvailabilityChanged, void(ScreenAvailability availability));
  const Vector<KURL>& Urls() const override { return urls_; }

 private:
  const Vector<KURL> urls_;
};

class MockPresentationAvailabilityCallbacks
    : public PresentationAvailabilityCallbacks {
 public:
  MockPresentationAvailabilityCallbacks()
      : PresentationAvailabilityCallbacks(nullptr, WTF::Vector<KURL>()) {}
  ~MockPresentationAvailabilityCallbacks() override = default;

  MOCK_METHOD1(Resolve, void(bool value));
  MOCK_METHOD0(RejectAvailabilityNotSupported, void());
};

class PresentationAvailabilityStateTest : public ::testing::Test {
 public:
  PresentationAvailabilityStateTest()
      : url1_(KURL("https://www.example.com/1.html")),
        url2_(KURL("https://www.example.com/2.html")),
        url3_(KURL("https://www.example.com/3.html")),
        url4_(KURL("https://www.example.com/4.html")),
        urls_({url1_, url2_, url3_, url4_}),
        mock_observer_all_urls_(urls_),
        mock_observer1_({url1_, url2_, url3_}),
        mock_observer2_({url2_, url3_, url4_}),
        mock_observer3_({url2_, url3_}),
        mock_observers_({&mock_observer1_, &mock_observer2_, &mock_observer3_}),
        mock_presentation_service_(),
        state_(&mock_presentation_service_) {}

  ~PresentationAvailabilityStateTest() override = default;

  void ChangeURLState(const KURL& url, ScreenAvailability state) {
    if (state != ScreenAvailability::UNKNOWN)
      state_.UpdateAvailability(url, state);
  }

  void RequestAvailabilityAndAddObservers() {
    for (auto* mock_observer : mock_observers_) {
      state_.RequestAvailability(
          mock_observer->Urls(),
          std::make_unique<MockPresentationAvailabilityCallbacks>());
      state_.AddObserver(mock_observer);
    }
  }

  // Tests that PresenationService is called for getAvailability(urls), after
  // |urls| change state to |states|. This function takes ownership of
  // |mock_callback|.
  void TestRequestAvailability(
      const Vector<KURL>& urls,
      const std::vector<ScreenAvailability>& states,
      MockPresentationAvailabilityCallbacks* mock_callback) {
    DCHECK_EQ(urls.size(), states.size());

    for (const auto& url : urls) {
      EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
          .Times(1);
      EXPECT_CALL(mock_presentation_service_,
                  StopListeningForScreenAvailability(url))
          .Times(1);
    }

    state_.RequestAvailability(
        urls,
        std::unique_ptr<MockPresentationAvailabilityCallbacks>(mock_callback));
    for (size_t i = 0; i < urls.size(); i++)
      ChangeURLState(urls[i], states[i]);
  }

 protected:
  const KURL url1_;
  const KURL url2_;
  const KURL url3_;
  const KURL url4_;
  const Vector<KURL> urls_;
  MockPresentationAvailabilityObserver mock_observer_all_urls_;
  MockPresentationAvailabilityObserver mock_observer1_;
  MockPresentationAvailabilityObserver mock_observer2_;
  MockPresentationAvailabilityObserver mock_observer3_;
  std::vector<MockPresentationAvailabilityObserver*> mock_observers_;

  MockPresentationService mock_presentation_service_;
  PresentationAvailabilityState state_;
};

TEST_F(PresentationAvailabilityStateTest, RequestAvailability) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url));
  }

  state_.RequestAvailability(
      urls_, std::make_unique<MockPresentationAvailabilityCallbacks>());
  state_.UpdateAvailability(url1_, ScreenAvailability::AVAILABLE);

  for (const auto& url : urls_)
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));

  state_.AddObserver(&mock_observer_all_urls_);

  EXPECT_CALL(mock_observer_all_urls_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  state_.UpdateAvailability(url1_, ScreenAvailability::UNAVAILABLE);
  EXPECT_CALL(mock_observer_all_urls_,
              AvailabilityChanged(ScreenAvailability::AVAILABLE));
  state_.UpdateAvailability(url1_, ScreenAvailability::AVAILABLE);
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url));
  }
  state_.RemoveObserver(&mock_observer_all_urls_);

  // After RemoveObserver(), |mock_observer_all_urls_| should no longer be
  // notified.
  EXPECT_CALL(mock_observer_all_urls_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE))
      .Times(0);
  state_.UpdateAvailability(url1_, ScreenAvailability::UNAVAILABLE);
}

TEST_F(PresentationAvailabilityStateTest,
       ScreenAvailabilitySourceNotSupported) {
  for (const auto& url : urls_)
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url));

  state_.AddObserver(&mock_observer_all_urls_);

  EXPECT_CALL(mock_observer_all_urls_,
              AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  state_.UpdateAvailability(url1_, ScreenAvailability::SOURCE_NOT_SUPPORTED);

  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url));
  }
  state_.RemoveObserver(&mock_observer_all_urls_);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlNoAvailabilityChange) {
  auto* mock_callback =
      new ::testing::StrictMock<MockPresentationAvailabilityCallbacks>();

  EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url1_))
      .Times(1);

  state_.RequestAvailability(
      Vector<KURL>({url1_}),
      std::unique_ptr<PresentationAvailabilityCallbacks>(mock_callback));
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesAvailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, Resolve(true));

  TestRequestAvailability({url1_}, {ScreenAvailability::AVAILABLE},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesNotCompatible) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, Resolve(false));

  TestRequestAvailability({url1_}, {ScreenAvailability::SOURCE_NOT_SUPPORTED},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesUnavailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, Resolve(false));

  TestRequestAvailability({url1_}, {ScreenAvailability::UNAVAILABLE},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityOneUrlBecomesUnsupported) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, RejectAvailabilityNotSupported());

  TestRequestAvailability({url1_}, {ScreenAvailability::DISABLED},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesAvailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, Resolve(true)).Times(1);

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::AVAILABLE, ScreenAvailability::AVAILABLE},
      mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesUnavailable) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, Resolve(false)).Times(1);

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::UNAVAILABLE, ScreenAvailability::UNAVAILABLE},
      mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesNotCompatible) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, Resolve(false)).Times(1);

  TestRequestAvailability({url1_, url2_},
                          {ScreenAvailability::SOURCE_NOT_SUPPORTED,
                           ScreenAvailability::SOURCE_NOT_SUPPORTED},
                          mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityMultipleUrlsAllBecomesUnsupported) {
  auto* mock_callback = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback, RejectAvailabilityNotSupported()).Times(1);

  TestRequestAvailability(
      {url1_, url2_},
      {ScreenAvailability::DISABLED, ScreenAvailability::DISABLED},
      mock_callback);
}

TEST_F(PresentationAvailabilityStateTest,
       RequestAvailabilityReturnsDirectlyForAlreadyListeningUrls) {
  // First getAvailability() call.
  auto* mock_callback_1 = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback_1, Resolve(false)).Times(1);

  std::vector<ScreenAvailability> state_seq = {ScreenAvailability::UNAVAILABLE,
                                               ScreenAvailability::AVAILABLE,
                                               ScreenAvailability::UNAVAILABLE};
  TestRequestAvailability({url1_, url2_, url3_}, state_seq, mock_callback_1);

  // Second getAvailability() call.
  for (const auto& url : mock_observer3_.Urls()) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }
  auto* mock_callback_2 = new MockPresentationAvailabilityCallbacks();
  EXPECT_CALL(*mock_callback_2, Resolve(true)).Times(1);

  state_.RequestAvailability(
      mock_observer3_.Urls(),
      std::unique_ptr<MockPresentationAvailabilityCallbacks>(mock_callback_2));
}

TEST_F(PresentationAvailabilityStateTest, StartListeningListenToEachURLOnce) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  RequestAvailabilityAndAddObservers();
}

TEST_F(PresentationAvailabilityStateTest, StopListeningListenToEachURLOnce) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
    EXPECT_CALL(mock_presentation_service_,
                StopListeningForScreenAvailability(url))
        .Times(1);
  }

  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(mock_observer2_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(mock_observer3_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));

  RequestAvailabilityAndAddObservers();

  // Clean up callbacks.
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);

  for (auto* mock_observer : mock_observers_)
    state_.RemoveObserver(mock_observer);
}

TEST_F(PresentationAvailabilityStateTest,
       StopListeningDoesNotStopIfURLListenedByOthers) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  //  |url1_| is only listened to by |observer1_|.
  EXPECT_CALL(mock_presentation_service_,
              StopListeningForScreenAvailability(url1_))
      .Times(1);
  EXPECT_CALL(mock_presentation_service_,
              StopListeningForScreenAvailability(url2_))
      .Times(0);
  EXPECT_CALL(mock_presentation_service_,
              StopListeningForScreenAvailability(url3_))
      .Times(0);

  RequestAvailabilityAndAddObservers();

  for (auto* mock_observer : mock_observers_)
    state_.AddObserver(mock_observer);

  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(mock_observer2_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  EXPECT_CALL(mock_observer3_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));

  // Clean up callbacks.
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);
  state_.RemoveObserver(&mock_observer1_);
}

TEST_F(PresentationAvailabilityStateTest,
       UpdateAvailabilityInvokesAvailabilityChanged) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::AVAILABLE));

  RequestAvailabilityAndAddObservers();

  ChangeURLState(url1_, ScreenAvailability::AVAILABLE);

  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  ChangeURLState(url1_, ScreenAvailability::UNAVAILABLE);

  EXPECT_CALL(mock_observer1_,
              AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  ChangeURLState(url1_, ScreenAvailability::SOURCE_NOT_SUPPORTED);
}

TEST_F(PresentationAvailabilityStateTest,
       UpdateAvailabilityInvokesMultipleAvailabilityChanged) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  for (auto* mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::AVAILABLE));
  }

  RequestAvailabilityAndAddObservers();

  ChangeURLState(url2_, ScreenAvailability::AVAILABLE);

  for (auto* mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::UNAVAILABLE));
  }
  ChangeURLState(url2_, ScreenAvailability::UNAVAILABLE);
}

TEST_F(PresentationAvailabilityStateTest,
       SourceNotSupportedPropagatedToMultipleObservers) {
  for (const auto& url : urls_) {
    EXPECT_CALL(mock_presentation_service_, ListenForScreenAvailability(url))
        .Times(1);
  }

  RequestAvailabilityAndAddObservers();
  for (auto* mock_observer : mock_observers_) {
    EXPECT_CALL(*mock_observer,
                AvailabilityChanged(ScreenAvailability::SOURCE_NOT_SUPPORTED));
  }
  ChangeURLState(url2_, ScreenAvailability::SOURCE_NOT_SUPPORTED);
}

}  // namespace blink
