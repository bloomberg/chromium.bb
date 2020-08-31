// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/sync/web_view_sync_client.h"

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "components/autofill/core/browser/webdata/mock_autofill_webdata_service.h"
#include "components/history/core/common/pref_names.h"
#include "components/invalidation/impl/fake_invalidation_service.h"
#include "components/password_manager/core/browser/test_password_store.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/data_type_controller.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync/model/test_model_type_store_service.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#import "testing/gtest_mac.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_web_view {

class WebViewSyncClientTest : public PlatformTest {
 protected:
  WebViewSyncClientTest()
      : profile_web_data_service_(
            base::MakeRefCounted<autofill::MockAutofillWebDataService>()),
        account_web_data_service_(
            base::MakeRefCounted<autofill::MockAutofillWebDataService>()),
        profile_password_store_(
            base::MakeRefCounted<password_manager::TestPasswordStore>()),
        account_password_store_(
            base::MakeRefCounted<password_manager::TestPasswordStore>(
                /*is_account_store=*/true)),
        client_(profile_web_data_service_.get(),
                account_web_data_service_.get(),
                profile_password_store_.get(),
                account_password_store_.get(),
                &pref_service_,
                identity_test_environment_.identity_manager(),
                &model_type_store_service_,
                &device_info_sync_service_,
                &invalidation_service_) {
    pref_service_.registry()->RegisterBooleanPref(
        prefs::kSavingBrowserHistoryDisabled, true);
    profile_password_store_->Init(&pref_service_, base::DoNothing());
    account_password_store_->Init(&pref_service_, base::DoNothing());
  }

  ~WebViewSyncClientTest() override {
    profile_password_store_->ShutdownOnUIThread();
    account_password_store_->ShutdownOnUIThread();
  }

  web::WebTaskEnvironment task_environment_;
  scoped_refptr<autofill::MockAutofillWebDataService> profile_web_data_service_;
  scoped_refptr<autofill::MockAutofillWebDataService> account_web_data_service_;
  scoped_refptr<password_manager::TestPasswordStore> profile_password_store_;
  scoped_refptr<password_manager::TestPasswordStore> account_password_store_;
  TestingPrefServiceSimple pref_service_;
  signin::IdentityTestEnvironment identity_test_environment_;
  syncer::TestModelTypeStoreService model_type_store_service_;
  syncer::FakeDeviceInfoSyncService device_info_sync_service_;
  invalidation::FakeInvalidationService invalidation_service_;
  WebViewSyncClient client_;
};

// Verify enabled data types.
TEST_F(WebViewSyncClientTest, CreateDataTypeControllers) {
  syncer::TestSyncService sync_service;
  syncer::DataTypeController::TypeVector data_type_controllers =
      client_.CreateDataTypeControllers(&sync_service);
  syncer::ModelTypeSet allowed_types = syncer::ModelTypeSet(
      syncer::DEVICE_INFO, syncer::AUTOFILL, syncer::AUTOFILL_PROFILE,
      syncer::AUTOFILL_WALLET_DATA, syncer::AUTOFILL_WALLET_METADATA,
      syncer::PASSWORDS);
  for (const auto& data_type_controller : data_type_controllers) {
    ASSERT_TRUE(allowed_types.Has(data_type_controller->type()));
    allowed_types.Remove(data_type_controller->type());
  }
  EXPECT_TRUE(allowed_types.Empty());
}

}  // namespace ios_web_view
