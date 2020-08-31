// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/android_affiliation/mock_affiliation_consumer.h"

#include "base/bind.h"
#include "base/bind_helpers.h"

namespace password_manager {

MockAffiliationConsumer::MockAffiliationConsumer() {
  EXPECT_CALL(*this, OnResultCallback(testing::_, testing::_)).Times(0);
}

MockAffiliationConsumer::~MockAffiliationConsumer() {
}

void MockAffiliationConsumer::ExpectSuccessWithResult(
    const AffiliatedFacets& expected_result) {
  EXPECT_CALL(*this, OnResultCallback(
                         testing::UnorderedElementsAreArray(expected_result),
                         true)).Times(1);
}

void MockAffiliationConsumer::ExpectFailure() {
  EXPECT_CALL(*this, OnResultCallback(testing::UnorderedElementsAre(), false))
      .Times(1);
}

AffiliationService::ResultCallback
MockAffiliationConsumer::GetResultCallback() {
  return base::BindOnce(&MockAffiliationConsumer::OnResultCallback,
                        base::Unretained(this));
}

}  // namespace password_manager
