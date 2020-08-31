// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/unique_id_tab_helper.h"

#import "ios/web/public/test/fakes/test_web_state.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Test fixture for TabIdTabHelper class.
class UniqueIDTabHelperTest : public PlatformTest {
 protected:
  web::TestWebState first_web_state_;
  web::TestWebState second_web_state_;
};

// Tests that a tab ID is returned for a WebState, and tab ID's are different
// for different WebStates if they were once set differently.
TEST_F(UniqueIDTabHelperTest, UniqueIdentifiers) {
  UniqueIDTabHelper::CreateForWebState(&first_web_state_);
  UniqueIDTabHelper::CreateForWebState(&second_web_state_);

  uint32_t first_available_unique_id =
      UniqueIDTabHelper::FromWebState(&first_web_state_)
          ->GetNextAvailableRendererID();
  uint32_t second_available_unique_id =
      UniqueIDTabHelper::FromWebState(&second_web_state_)
          ->GetNextAvailableRendererID();

  EXPECT_EQ(first_available_unique_id, 0U);
  EXPECT_EQ(first_available_unique_id, second_available_unique_id);

  UniqueIDTabHelper::FromWebState(&second_web_state_)
      ->SetNextAvailableRendererID(10U);
  UniqueIDTabHelper::FromWebState(&second_web_state_)
      ->SetNextAvailableRendererID(20U);

  first_available_unique_id = UniqueIDTabHelper::FromWebState(&first_web_state_)
                                  ->GetNextAvailableRendererID();
  second_available_unique_id =
      UniqueIDTabHelper::FromWebState(&second_web_state_)
          ->GetNextAvailableRendererID();

  EXPECT_NE(first_available_unique_id, second_available_unique_id);
}

// Tests that a tab ID is stable across successive calls.
TEST_F(UniqueIDTabHelperTest, StableAcrossCalls) {
  UniqueIDTabHelper::CreateForWebState(&first_web_state_);
  UniqueIDTabHelper* tab_helper =
      UniqueIDTabHelper::FromWebState(&first_web_state_);
  tab_helper->SetNextAvailableRendererID(10U);

  const uint32_t first_call_available_unique_id =
      tab_helper->GetNextAvailableRendererID();
  const uint32_t second_call_available_unique_id =
      tab_helper->GetNextAvailableRendererID();

  EXPECT_EQ(first_call_available_unique_id, second_call_available_unique_id);
}
