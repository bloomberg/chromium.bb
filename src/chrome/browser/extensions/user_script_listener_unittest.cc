// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/user_script_listener.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/extensions/unpacked_installer.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "extensions/browser/extension_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/fake_chrome_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#endif

using content::NavigationThrottle;

namespace extensions {

namespace {

const char kMatchingUrl[] = "http://google.com/";
const char kNotMatchingUrl[] = "http://example.com/";

// Yoinked from extension_manifest_unittest.cc.
std::unique_ptr<base::DictionaryValue> LoadManifestFile(
    const base::FilePath path,
    std::string* error) {
  EXPECT_TRUE(base::PathExists(path));
  JSONFileValueDeserializer deserializer(path);
  return base::DictionaryValue::From(deserializer.Deserialize(nullptr, error));
}

scoped_refptr<Extension> LoadExtension(const std::string& filename,
                                       std::string* error) {
  base::FilePath path;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  path = path.
      AppendASCII("extensions").
      AppendASCII("manifest_tests").
      AppendASCII(filename.c_str());
  std::unique_ptr<base::DictionaryValue> value = LoadManifestFile(path, error);
  if (!value)
    return nullptr;
  return Extension::Create(path.DirName(), Manifest::UNPACKED, *value,
                           Extension::NO_FLAGS, error);
}

}  // namespace

class UserScriptListenerTest : public testing::Test {
 public:
  UserScriptListenerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        profile_manager_(
            new TestingProfileManager(TestingBrowserProcess::GetGlobal())) {}

  void SetUp() override {
#if defined(OS_CHROMEOS)
    user_manager_enabler_ = std::make_unique<user_manager::ScopedUserManager>(
        std::make_unique<chromeos::FakeChromeUserManager>());
#endif
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-profile");
    ASSERT_TRUE(profile_);
    TestExtensionSystem* test_extension_system =
        static_cast<TestExtensionSystem*>(ExtensionSystem::Get(profile_));
    service_ = test_extension_system->CreateExtensionService(
        base::CommandLine::ForCurrentProcess(), base::FilePath(), false);

    auto instance = content::SiteInstance::Create(profile_);
    instance->GetProcess()->Init();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_, std::move(instance));
  }

 protected:
  NavigationThrottle::ThrottleCheckResult StartTestRequest(
      const std::string& url_string) {
    handle_ = content::NavigationHandle::CreateNavigationHandleForTesting(
        GURL(url_string), web_contents_->GetMainFrame());

    std::unique_ptr<NavigationThrottle> throttle =
        listener_.CreateNavigationThrottle(handle_.get());
    if (throttle)
      handle_->RegisterThrottleForTesting(std::move(throttle));
    return handle_->CallWillStartRequestForTesting();
  }

  void LoadTestExtension() {
    base::FilePath test_dir;
    ASSERT_TRUE(base::PathService::Get(chrome::DIR_TEST_DATA, &test_dir));
    base::FilePath extension_path = test_dir
        .AppendASCII("extensions")
        .AppendASCII("good")
        .AppendASCII("Extensions")
        .AppendASCII("behllobkkfkfnphdnhnkndlbkcpglgmj")
        .AppendASCII("1.0.0.0");
    UnpackedInstaller::Create(service_)->Load(extension_path);
    content::RunAllTasksUntilIdle();
  }

  void UnloadTestExtension() {
    const extensions::ExtensionSet& extensions =
        ExtensionRegistry::Get(profile_)->enabled_extensions();
    ASSERT_FALSE(extensions.is_empty());
    service_->UnloadExtension((*extensions.begin())->id(),
                              UnloadedExtensionReason::DISABLE);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  UserScriptListener listener_;
  TestingProfile* profile_ = nullptr;
  ExtensionService* service_ = nullptr;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<content::NavigationHandle> handle_;
#if defined(OS_CHROMEOS)
  std::unique_ptr<user_manager::ScopedUserManager> user_manager_enabler_;
#endif
};

namespace {

TEST_F(UserScriptListenerTest, DelayAndUpdate) {
  LoadTestExtension();

  EXPECT_EQ(NavigationThrottle::DEFER, StartTestRequest(kMatchingUrl));

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle_->IsDeferredForTesting());
}

TEST_F(UserScriptListenerTest, DelayAndUnload) {
  LoadTestExtension();

  EXPECT_EQ(NavigationThrottle::DEFER, StartTestRequest(kMatchingUrl));

  UnloadTestExtension();
  base::RunLoop().RunUntilIdle();

  // This is still not enough to start delayed requests. We have to notify the
  // listener that the user scripts have been updated.
  EXPECT_TRUE(handle_->IsDeferredForTesting());

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle_->IsDeferredForTesting());
}

TEST_F(UserScriptListenerTest, NoDelayNoExtension) {
  EXPECT_EQ(NavigationThrottle::PROCEED, StartTestRequest(kMatchingUrl));
  EXPECT_FALSE(handle_->IsDeferredForTesting());
}

TEST_F(UserScriptListenerTest, NoDelayNotMatching) {
  LoadTestExtension();

  EXPECT_EQ(NavigationThrottle::PROCEED, StartTestRequest(kNotMatchingUrl));
  EXPECT_FALSE(handle_->IsDeferredForTesting());
}

TEST_F(UserScriptListenerTest, MultiProfile) {
  LoadTestExtension();

  // Fire up a second profile and have it load an extension with a content
  // script.
  TestingProfile* profile2 =
      profile_manager_->CreateTestingProfile("test-profile2");
  ASSERT_TRUE(profile2);
  std::string error;
  scoped_refptr<Extension> extension = LoadExtension(
      "content_script_yahoo.json", &error);
  ASSERT_TRUE(extension.get());

  ExtensionRegistry* registry = ExtensionRegistry::Get(profile2);
  registry->AddEnabled(extension);
  registry->TriggerOnLoaded(extension.get());

  EXPECT_EQ(NavigationThrottle::DEFER, StartTestRequest(kMatchingUrl));

  // When the first profile's user scripts are ready, the request should still
  // be blocked waiting for profile2.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(handle_->IsDeferredForTesting());

  // After profile2 is ready, the request should proceed.
  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile2),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(handle_->IsDeferredForTesting());
}

// Test when the script updated notification occurs before the throttle's
// WillStartRequest function is called.  This can occur when there are multiple
// throttles.
TEST_F(UserScriptListenerTest, ResumeBeforeStart) {
  LoadTestExtension();
  handle_ = content::NavigationHandle::CreateNavigationHandleForTesting(
      GURL(kMatchingUrl), web_contents_->GetMainFrame());

  std::unique_ptr<NavigationThrottle> throttle =
      listener_.CreateNavigationThrottle(handle_.get());
  ASSERT_TRUE(throttle);

  content::NotificationService::current()->Notify(
      extensions::NOTIFICATION_USER_SCRIPTS_UPDATED,
      content::Source<Profile>(profile_),
      content::NotificationService::NoDetails());
  base::RunLoop().RunUntilIdle();

  ASSERT_EQ(content::NavigationThrottle::PROCEED, throttle->WillStartRequest());
}

}  // namespace

}  // namespace extensions
