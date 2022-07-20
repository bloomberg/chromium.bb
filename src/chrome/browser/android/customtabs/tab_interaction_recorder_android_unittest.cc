// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/customtabs/tab_interaction_recorder_android.h"

#include <memory>
#include <utility>

#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory_test_api.h"
#include "components/autofill/content/browser/content_autofill_driver_test_api.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/common/autofill_tick_clock.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::AutofillManager;
using testing::_;
using testing::NiceMock;
using AutofillObsever = AutofillManager::Observer;

namespace customtabs {

namespace {
class MockAutofillClient : public autofill::TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  ~MockAutofillClient() override = default;
};

class MockAutofillDriver : public autofill::TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  ~MockAutofillDriver() override = default;
};

class MockAutofillManager : public autofill::TestBrowserAutofillManager {
 public:
  MockAutofillManager(autofill::TestAutofillDriver* driver,
                      autofill::TestAutofillClient* client)
      : autofill::TestBrowserAutofillManager(driver, client) {}
  MockAutofillManager(const MockAutofillManager&) = delete;
  MockAutofillManager& operator=(const MockAutofillManager&) = delete;
  ~MockAutofillManager() override = default;
};

void OnTextFieldDidChangeForAutofillManager(AutofillManager* autofill_manager) {
  autofill::FormData form;
  autofill::test::CreateTestAddressFormData(&form);
  autofill::FormFieldData field = form.fields.front();

  autofill_manager->OnTextFieldDidChange(
      form, field, gfx::RectF(), autofill::AutofillTickClock::NowTicks());
}
}  // namespace

class AutofillObserverImplTest : public testing::Test {
 public:
  AutofillObserverImplTest() = default;

  void SetUp() override {
    client_.SetPrefs(autofill::test::PrefServiceForTesting());
    driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    manager_ = std::make_unique<MockAutofillManager>(driver_.get(), &client_);
  }

  void TearDown() override { driver_.reset(); }

  MockAutofillManager* autofill_manager() { return manager_.get(); }

  void DestroyManager() { manager_.release(); }

 protected:
  base::test::TaskEnvironment task_environment_;
  NiceMock<MockAutofillClient> client_;
  std::unique_ptr<MockAutofillDriver> driver_;
  std::unique_ptr<MockAutofillManager> manager_;
};

TEST_F(AutofillObserverImplTest, TestFormInteraction) {
  base::MockOnceCallback<void()> callback;
  AutofillObserverImpl obsever(autofill_manager(), callback.Get());

  EXPECT_CALL(callback, Run()).Times(1);
  OnTextFieldDidChangeForAutofillManager(autofill_manager());

  // Observer should no longer get notified after the first interaction.
  EXPECT_CALL(callback, Run()).Times(0);
  OnTextFieldDidChangeForAutofillManager(autofill_manager());
}

TEST_F(AutofillObserverImplTest, TestNoFormInteraction) {
  base::MockOnceCallback<void()> callback;
  auto* observer = new AutofillObserverImpl(autofill_manager(), callback.Get());

  EXPECT_CALL(callback, Run()).Times(0);
  delete observer;
}

TEST_F(AutofillObserverImplTest, TestAutofillManagerDestroy) {
  base::MockOnceCallback<void()> callback;
  auto* observer = new AutofillObserverImpl(autofill_manager(), callback.Get());

  DestroyManager();

  EXPECT_CALL(callback, Run()).Times(0);
  delete observer;
}

// === TabInteractionRecorderAndroidTest ===

class TabInteractionRecorderAndroidTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TabInteractionRecorderAndroidTest() = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    client_.SetPrefs(autofill::test::PrefServiceForTesting());
    driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    manager_ = std::make_unique<MockAutofillManager>(driver_.get(), &client_);
  }

  void TearDown() override {
    manager_.reset();
    driver_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    std::unique_ptr<content::WebContents> contents =
        ChromeRenderViewHostTestHarness::CreateTestWebContents();
    TabInteractionRecorderAndroid::CreateForWebContents(contents.get());
    auto* helper =
        TabInteractionRecorderAndroid::FromWebContents(contents.get());
    helper->SetAutofillManagerForTest(autofill_manager());

    // Simulate a navigation event to force the initialization of the main
    // frame.
    content::WebContentsTester::For(contents.get())
        ->NavigateAndCommit(GURL("https://foo.com"));
    task_environment()->RunUntilIdle();
    return contents;
  }

  MockAutofillManager* autofill_manager() { return manager_.get(); }

 protected:
  NiceMock<MockAutofillClient> client_;
  std::unique_ptr<MockAutofillDriver> driver_;
  std::unique_ptr<MockAutofillManager> manager_;

  base::test::ScopedFeatureList test_feature_list_;
};

TEST_F(TabInteractionRecorderAndroidTest, HadFormInteraction) {
  test_feature_list_.InitAndEnableFeature(chrome::android::kCCTRetainingState);

  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = TabInteractionRecorderAndroid::FromWebContents(contents.get());

  EXPECT_FALSE(helper->has_form_interactions());
  OnTextFieldDidChangeForAutofillManager(autofill_manager());
  EXPECT_TRUE(helper->has_form_interactions());
}

TEST_F(TabInteractionRecorderAndroidTest, HasNavigatedFromFirstPage) {
  test_feature_list_.InitAndEnableFeature(chrome::android::kCCTRetainingState);
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = TabInteractionRecorderAndroid::FromWebContents(contents.get());

  EXPECT_FALSE(helper->HasNavigatedFromFirstPage());

  content::WebContentsTester::For(contents.get())
      ->NavigateAndCommit(GURL("https://bar.com"));
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(helper->HasNavigatedFromFirstPage());
}

}  // namespace customtabs
