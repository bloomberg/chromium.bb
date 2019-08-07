// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TEST_MOCK_TRACKER_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TEST_MOCK_TRACKER_H_

#include <memory>
#include <string>

#include "base/macros.h"
#include "components/feature_engagement/public/tracker.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace feature_engagement {
namespace test {

class MockTracker : public Tracker {
 public:
  MockTracker();
  ~MockTracker() override;

  // Tracker implememtation.
  MOCK_METHOD1(NotifyEvent, void(const std::string& event));
  MOCK_METHOD1(ShouldTriggerHelpUI, bool(const base::Feature& feature));
  MOCK_CONST_METHOD1(WouldTriggerHelpUI, bool(const base::Feature& feature));
  MOCK_CONST_METHOD1(GetTriggerState,
                     Tracker::TriggerState(const base::Feature& feature));
  MOCK_CONST_METHOD0(IsInitialized, bool());
  MOCK_METHOD1(Dismissed, void(const base::Feature& feature));
  MOCK_METHOD0(AcquireDisplayLock, std::unique_ptr<DisplayLockHandle>());
  MOCK_METHOD1(AddOnInitializedCallback, void(OnInitializedCallback callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTracker);
};

}  // namespace test
}  // namespace feature_engagement

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TEST_MOCK_TRACKER_H_
