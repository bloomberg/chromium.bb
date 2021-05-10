// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/profile.h"

#include <stddef.h>

#include <memory>

#include "base/bind.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "base/version.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/chrome_version_service.h"
#include "chrome/browser/profiles/profile_destroyer.h"
#include "chrome/browser/profiles/profile_impl.h"
#include "chrome/browser/profiles/profile_keep_alive_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/browser/profiles/scoped_profile_keep_alive.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "extensions/buildflags/buildflags.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/dns/mock_host_resolver.h"
#include "net/net_buildflags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/browser/extension_protocols.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile_attributes_entry.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "components/account_id/account_id.h"
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)

namespace {

// A helper class which creates a SimpleURLLoader with an expected final status
// and the ability to wait until a request completes. It's not considered a
// failure for the load to never complete.
class SimpleURLLoaderHelper {
 public:
  // Creating the SimpleURLLoaderHelper automatically creates and starts a
  // SimpleURLLoader.
  SimpleURLLoaderHelper(network::mojom::URLLoaderFactory* factory,
                        const GURL& url,
                        int expected_error_code,
                        int load_flags = net::LOAD_NORMAL)
      : expected_error_code_(expected_error_code), is_complete_(false) {
    auto request = std::make_unique<network::ResourceRequest>();
    request->url = url;
    request->load_flags = load_flags;

    // Populate Network Isolation Key so that the request is cacheable.
    url::Origin origin = url::Origin::Create(url);
    request->trusted_params = network::ResourceRequest::TrustedParams();
    request->trusted_params->isolation_info =
        net::IsolationInfo::CreateForInternalRequest(origin);
    request->site_for_cookies =
        request->trusted_params->isolation_info.site_for_cookies();

    loader_ = network::SimpleURLLoader::Create(std::move(request),
                                               TRAFFIC_ANNOTATION_FOR_TESTS);

    loader_->DownloadToStringOfUnboundedSizeUntilCrashAndDie(
        factory, base::BindOnce(&SimpleURLLoaderHelper::OnSimpleLoaderComplete,
                                base::Unretained(this)));
  }

  void OnSimpleLoaderComplete(std::unique_ptr<std::string> response_body) {
    EXPECT_EQ(expected_error_code_, loader_->NetError());
    is_complete_ = true;
    run_loop_.Quit();
  }

  void WaitForCompletion() { run_loop_.Run(); }

  bool is_complete() const { return is_complete_; }

 private:
  const int expected_error_code_;
  base::RunLoop run_loop_;

  bool is_complete_;
  std::unique_ptr<network::SimpleURLLoader> loader_;

  DISALLOW_COPY_AND_ASSIGN(SimpleURLLoaderHelper);
};

class MockProfileDelegate : public Profile::Delegate {
 public:
  MOCK_METHOD1(OnPrefsLoaded, void(Profile*));
  MOCK_METHOD3(OnProfileCreated, void(Profile*, bool, bool));
};

class ProfileDestructionWatcher : public ProfileObserver {
 public:
  ProfileDestructionWatcher() = default;
  ~ProfileDestructionWatcher() override = default;

  void Watch(Profile* profile) { observed_profiles_.Add(profile); }

  bool destroyed() const { return destroyed_; }

  void WaitForDestruction() { run_loop_.Run(); }

 private:
  // ProfileObserver:
  void OnProfileWillBeDestroyed(Profile* profile) override {
    DCHECK(!destroyed_) << "Double profile destruction";
    destroyed_ = true;
    run_loop_.Quit();
    observed_profiles_.Remove(profile);
  }

  bool destroyed_ = false;
  base::RunLoop run_loop_;
  ScopedObserver<Profile, ProfileObserver> observed_profiles_{this};

  DISALLOW_COPY_AND_ASSIGN(ProfileDestructionWatcher);
};

// Creates a prefs file in the given directory.
void CreatePrefsFileInDirectory(const base::FilePath& directory_path) {
  base::FilePath pref_path(directory_path.Append(chrome::kPreferencesFilename));
  std::string data("{}");
  ASSERT_TRUE(base::WriteFile(pref_path, data));
}

void CheckChromeVersion(Profile *profile, bool is_new) {
  std::string created_by_version;
  if (is_new) {
    created_by_version = version_info::GetVersionNumber();
  } else {
    created_by_version = "1.0.0.0";
  }
  std::string pref_version =
      ChromeVersionService::GetVersion(profile->GetPrefs());
  // Assert that created_by_version pref gets set to current version.
  EXPECT_EQ(created_by_version, pref_version);
}

void FlushTaskRunner(base::SequencedTaskRunner* runner) {
  ASSERT_TRUE(runner);
  base::WaitableEvent unblock(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                              base::WaitableEvent::InitialState::NOT_SIGNALED);

  runner->PostTask(FROM_HERE, base::BindOnce(&base::WaitableEvent::Signal,
                                             base::Unretained(&unblock)));

  unblock.Wait();
}

void SpinThreads() {
  // Give threads a chance to do their stuff before shutting down (i.e.
  // deleting scoped temp dir etc).
  // Should not be necessary anymore once Profile deletion is fixed
  // (see crbug.com/88586).
  content::RunAllPendingInMessageLoop();

  // This prevents HistoryBackend from accessing its databases after the
  // directory that contains them has been deleted.
  base::ThreadPoolInstance::Get()->FlushForTesting();
}

class BrowserCloseObserver : public BrowserListObserver {
 public:
  explicit BrowserCloseObserver(Browser* browser) : browser_(browser) {
    BrowserList::AddObserver(this);
  }
  ~BrowserCloseObserver() override { BrowserList::RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

  // BrowserListObserver implementation.
  void OnBrowserRemoved(Browser* browser) override {
    if (browser == browser_)
      run_loop_.Quit();
  }

 private:
  Browser* browser_;
  base::RunLoop run_loop_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCloseObserver);
};

}  // namespace

class ProfileBrowserTest : public InProcessBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
#if BUILDFLAG(IS_CHROMEOS_ASH)
    command_line->AppendSwitch(
        chromeos::switches::kIgnoreUserProfileMappingForTests);
#endif
  }

  std::unique_ptr<Profile> CreateProfile(const base::FilePath& path,
                                         Profile::Delegate* delegate,
                                         Profile::CreateMode create_mode) {
    std::unique_ptr<Profile> profile =
        Profile::CreateProfile(path, delegate, create_mode);
    EXPECT_TRUE(profile.get());

    // Store the Profile's IO task runner so we can wind it down.
    profile_io_task_runner_ = profile->GetIOTaskRunner();

    return profile;
  }

  void FlushIoTaskRunnerAndSpinThreads() {
    FlushTaskRunner(profile_io_task_runner_.get());
    SpinThreads();
  }

  // Starts a test where a SimpleURLLoader is active during profile
  // shutdown. The test completes during teardown of the test fixture. The
  // request should be canceled by |context_getter| during profile shutdown,
  // before the URLRequestContext is destroyed. If that doesn't happen, the
  // Context's will still have outstanding requests during its destruction, and
  // will trigger a CHECK failure.
  void StartActiveLoaderDuringProfileShutdownTest(
      network::mojom::URLLoaderFactory* factory) {
    // This method should only be called once per test.
    DCHECK(!simple_loader_helper_);

    // Start a hanging request.  This request may or may not completed before
    // the end of the request.
    simple_loader_helper_ = std::make_unique<SimpleURLLoaderHelper>(
        factory, embedded_test_server()->GetURL("/hung"), net::ERR_FAILED);

    // Start a second mock request that just fails, and wait for it to complete.
    // This ensures the first request has reached the network stack.
    SimpleURLLoaderHelper simple_loader_helper2(
        factory, embedded_test_server()->GetURL("/echo?status=400"),
        net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    simple_loader_helper2.WaitForCompletion();

    // The first request should still be hung.
    EXPECT_FALSE(simple_loader_helper_->is_complete());
  }

  // Runs a test where an incognito profile's SimpleURLLoader is active during
  // teardown of the profile, and makes sure the request fails as expected.
  // Also tries issuing a request after the incognito profile has been
  // destroyed.
  static void RunURLLoaderActiveDuringIncognitoTeardownTest(
      net::EmbeddedTestServer* embedded_test_server,
      Browser* incognito_browser,
      network::mojom::URLLoaderFactory* factory) {
    // Start a hanging request.
    SimpleURLLoaderHelper simple_loader_helper1(
        factory, embedded_test_server->GetURL("/hung"), net::ERR_FAILED);

    // Start a second mock request that just fails, and wait for it to complete.
    // This ensures the first request has reached the network stack.
    SimpleURLLoaderHelper simple_loader_helper2(
        factory, embedded_test_server->GetURL("/echo?status=400"),
        net::ERR_HTTP_RESPONSE_CODE_FAILURE);
    simple_loader_helper2.WaitForCompletion();

    // The first request should still be hung.
    EXPECT_FALSE(simple_loader_helper1.is_complete());

    // Close all incognito tabs, starting profile shutdown.
    incognito_browser->tab_strip_model()->CloseAllTabs();

    // The request should have been canceled when the Profile shut down.
    simple_loader_helper1.WaitForCompletion();

    // Requests issued after Profile shutdown should fail in a similar manner.
    SimpleURLLoaderHelper simple_loader_helper3(
        factory, embedded_test_server->GetURL("/hung"), net::ERR_FAILED);
    simple_loader_helper3.WaitForCompletion();
  }

  scoped_refptr<base::SequencedTaskRunner> profile_io_task_runner_;

  // SimpleURLLoader that outlives the Profile, to test shutdown.
  std::unique_ptr<SimpleURLLoaderHelper> simple_loader_helper_;
};

// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile synchronously.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateNewProfileSynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
    CheckChromeVersion(profile.get(), true);

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile synchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateOldProfileSynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.GetPath());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));
    CheckChromeVersion(profile.get(), false);

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Flaky: http://crbug.com/393177
// Test OnProfileCreate is called with is_new_profile set to true when
// creating a new profile asynchronously.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateNewProfileAsynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    run_loop.Run();
    CheckChromeVersion(profile.get(), true);
  }

  FlushIoTaskRunnerAndSpinThreads();
}


// Flaky: http://crbug.com/393177
// Test OnProfileCreate is called with is_new_profile set to false when
// creating a profile asynchronously with an existing prefs file.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       DISABLED_CreateOldProfileAsynchronous) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  CreatePrefsFileInDirectory(temp_dir.GetPath());

  MockProfileDelegate delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, false))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    run_loop.Run();
    CheckChromeVersion(profile.get(), false);
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Flaky: http://crbug.com/393177
// Test that a README file is created for profiles that didn't have it.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, DISABLED_ProfileReadmeCreated) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  base::RunLoop run_loop;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true))
      .WillOnce(testing::InvokeWithoutArgs([&run_loop]() { run_loop.Quit(); }));

  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_ASYNCHRONOUS));

    // Wait for the profile to be created.
    run_loop.Run();

    // Verify that README exists.
    EXPECT_TRUE(
        base::PathExists(temp_dir.GetPath().Append(chrome::kReadmeFilename)));
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// Test that repeated setting of exit type is handled correctly.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, ExitType) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  EXPECT_CALL(delegate, OnProfileCreated(testing::NotNull(), true, true));
  {
    std::unique_ptr<Profile> profile(CreateProfile(
        temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));

    PrefService* prefs = profile->GetPrefs();
    // The initial state is crashed; store for later reference.
    std::string crash_value(prefs->GetString(prefs::kSessionExitType));

    // The first call to a type other than crashed should change the value.
    profile->SetExitType(Profile::EXIT_SESSION_ENDED);
    std::string first_call_value(prefs->GetString(prefs::kSessionExitType));
    EXPECT_NE(crash_value, first_call_value);

    // Subsequent calls to a non-crash value should be ignored.
    profile->SetExitType(Profile::EXIT_NORMAL);
    std::string second_call_value(prefs->GetString(prefs::kSessionExitType));
    EXPECT_EQ(first_call_value, second_call_value);

    // Setting back to a crashed value should work.
    profile->SetExitType(Profile::EXIT_CRASHED);
    std::string final_value(prefs->GetString(prefs::kSessionExitType));
    EXPECT_EQ(crash_value, final_value);

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }

  FlushIoTaskRunnerAndSpinThreads();
}

// The EndSession IO synchronization is only critical on Windows, but also
// happens under the USE_X11 define. See BrowserProcessImpl::EndSession.
#if defined(USE_X11) || defined(OS_WIN) || defined(USE_OZONE)

namespace {

std::string GetExitTypePreferenceFromDisk(Profile* profile) {
  base::FilePath prefs_path =
      profile->GetPath().Append(chrome::kPreferencesFilename);
  std::string prefs;
  if (!base::ReadFileToString(prefs_path, &prefs))
    return std::string();

  std::unique_ptr<base::Value> value = base::JSONReader::ReadDeprecated(prefs);
  if (!value)
    return std::string();

  base::DictionaryValue* dict = NULL;
  if (!value->GetAsDictionary(&dict) || !dict)
    return std::string();

  std::string exit_type;
  if (!dict->GetString("profile.exit_type", &exit_type))
    return std::string();

  return exit_type;
}

}  // namespace

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       WritesProfilesSynchronouslyOnEndSession) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  ASSERT_TRUE(profile_manager);
  std::vector<Profile*> loaded_profiles = profile_manager->GetLoadedProfiles();

  ASSERT_NE(loaded_profiles.size(), 0UL);
  Profile* profile = loaded_profiles[0];

#if BUILDFLAG(IS_CHROMEOS_ASH)
  for (auto* loaded_profile : loaded_profiles) {
    if (!chromeos::ProfileHelper::IsSigninProfile(loaded_profile)) {
      profile = loaded_profile;
      break;
    }
  }
#endif

  // It is important that the MessageLoop not pump extra messages during
  // EndSession() as some of those may be tasks queued to attempt to revive
  // services and processes that were just intentionally killed. This is a
  // regression blocker for https://crbug.com/318527.
  // Need to use this WeakPtr workaround as the browser test harness runs all
  // tasks until idle when tearing down.
  struct FailsIfCalledWhileOnStack
      : public base::SupportsWeakPtr<FailsIfCalledWhileOnStack> {
    void Fail() { ADD_FAILURE(); }
  } fails_if_called_while_on_stack;
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FailsIfCalledWhileOnStack::Fail,
                                fails_if_called_while_on_stack.AsWeakPtr()));

  // This retry loop reduces flakiness due to the fact that this ultimately
  // tests whether or not a code path hits a timed wait.
  bool succeeded = false;
  for (size_t retries = 0; !succeeded && retries < 3; ++retries) {
    // Flush the profile data to disk for all loaded profiles.
    profile->SetExitType(Profile::EXIT_CRASHED);
    profile->GetPrefs()->CommitPendingWrite();
    FlushTaskRunner(profile->GetIOTaskRunner().get());

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "Crashed");

    // The blocking wait in EndSession has a timeout.
    base::Time start = base::Time::Now();

    // This must not return until the profile data has been written to disk.
    // If this test flakes, then logoff on Windows has broken again.
    g_browser_process->EndSession();

    base::Time end = base::Time::Now();

    // The EndSession timeout is 10 seconds. If we take more than half that,
    // go around again, as we may have timed out on the wait.
    // This helps against flakes, and also ensures that if the IO thread starts
    // blocking systemically for that length of time (e.g. deadlocking or such),
    // we'll get a consistent test failure.
    if (end - start > base::TimeDelta::FromSeconds(5))
      continue;

    // Make sure that the prefs file was written with the expected key/value.
    ASSERT_EQ(GetExitTypePreferenceFromDisk(profile), "SessionEnded");

    // Mark the success.
    succeeded = true;
  }

  ASSERT_TRUE(succeeded) << "profile->EndSession() timed out too often.";
}

#endif  // defined(USE_X11) || defined(OS_WIN) || defined(USE_OZONE)

// The following tests make sure that it's safe to shut down while one of the
// Profile's URLLoaderFactories is in use by a SimpleURLLoader.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       SimpleURLLoaderUsingMainContextDuringShutdown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  StartActiveLoaderDuringProfileShutdownTest(
      content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}

// The following tests make sure that it's safe to destroy an incognito profile
// while one of the its URLLoaderFactory is in use by a SimpleURLLoader.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       SimpleURLLoaderUsingMainContextDuringIncognitoTeardown) {
  ASSERT_TRUE(embedded_test_server()->Start());
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  RunURLLoaderActiveDuringIncognitoTeardownTest(
      embedded_test_server(), incognito_browser,
      content::BrowserContext::GetDefaultStoragePartition(
          incognito_browser->profile())
          ->GetURLLoaderFactoryForBrowserProcess()
          .get());
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
// Regression test for https://crbug.com/1136214 - verification that
// ExtensionURLLoaderFactory won't hit a use-after-free bug when used after
// a Profile has been torn down already.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       ExtensionURLLoaderFactoryAfterIncognitoTeardown) {
  // Create a mojo::Remote to ExtensionURLLoaderFactory for the incognito
  // profile.
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  Profile* incognito_profile = incognito_browser->profile();
  mojo::Remote<network::mojom::URLLoaderFactory> url_loader_factory;
  url_loader_factory.Bind(extensions::CreateExtensionNavigationURLLoaderFactory(
      incognito_profile, ukm::kInvalidSourceIdObj,
      false /* is_web_view_request */));

  // Verify that the factory works fine while the profile is still alive.
  // We don't need to test with a real extension URL - it is sufficient to
  // verify that the factory responds with ERR_BLOCKED_BY_CLIENT that indicates
  // a missing extension.
  GURL missing_extension_url("chrome-extension://no-such-extension/blah");
  {
    SimpleURLLoaderHelper simple_loader_helper(url_loader_factory.get(),
                                               missing_extension_url,
                                               net::ERR_BLOCKED_BY_CLIENT);
    simple_loader_helper.WaitForCompletion();
  }

  {
    // Start monitoring |incognito_profile| for shutdown.
    ProfileManager* profile_manager = g_browser_process->profile_manager();
    EXPECT_TRUE(profile_manager->IsValidProfile(incognito_profile));
    ProfileDestructionWatcher watcher;
    watcher.Watch(incognito_profile);

    // Close all incognito tabs, starting profile shutdown.
    incognito_browser->tab_strip_model()->CloseAllTabs();

    // ProfileDestructionWatcher waits for
    // BrowserContext::NotifyWillBeDestroyed, but after the RunLoop unwinds, the
    // profile should already be gone - let's assert this below (since this
    // ensures that |simple_loader_helper2| really tests what needs to be
    // tested).
    watcher.WaitForDestruction();
    EXPECT_FALSE(profile_manager->IsValidProfile(incognito_profile));
  }

  // Verify that the factory doesn't crash (https://crbug.com/1136214), but
  // instead SimpleURLLoaderImpl::OnMojoDisconnect reports net::ERR_FAILED.
  {
    SimpleURLLoaderHelper simple_loader_helper2(
        url_loader_factory.get(), missing_extension_url, net::ERR_FAILED);
    simple_loader_helper2.WaitForCompletion();
  }
}
#endif

// Verifies the cache directory supports multiple profiles when it's overridden
// by group policy or command line switches.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, DiskCacheDirOverride) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const base::FilePath::StringPieceType profile_name =
      FILE_PATH_LITERAL("Profile 1");
  base::ScopedTempDir mock_user_data_dir;
  ASSERT_TRUE(mock_user_data_dir.CreateUniqueTempDir());
  base::FilePath profile_path =
      mock_user_data_dir.GetPath().Append(profile_name);

  {
    base::ScopedTempDir temp_disk_cache_dir;
    ASSERT_TRUE(temp_disk_cache_dir.CreateUniqueTempDir());
    g_browser_process->local_state()->SetFilePath(
        prefs::kDiskCacheDir, temp_disk_cache_dir.GetPath());
  }
}

// Verifies the last selected directory has a default value.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, LastSelectedDirectory) {
  ProfileImpl* profile_impl = static_cast<ProfileImpl*>(browser()->profile());
  base::FilePath home;
  base::PathService::Get(base::DIR_HOME, &home);
  ASSERT_EQ(profile_impl->last_selected_directory(), home);
}

// Verifies creating an OTR with non-primary id results in a different profile
// from incognito profile.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateNonPrimaryOTR) {
  Profile::OTRProfileID otr_profile_id("profile::otr");

  Profile* regular_profile = browser()->profile();
  EXPECT_FALSE(regular_profile->HasAnyOffTheRecordProfile());

  Profile* otr_profile =
      regular_profile->GetOffTheRecordProfile(otr_profile_id);
  EXPECT_TRUE(regular_profile->HasAnyOffTheRecordProfile());
  EXPECT_TRUE(otr_profile->IsOffTheRecord());
  EXPECT_EQ(otr_profile_id, otr_profile->GetOTRProfileID());
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(otr_profile_id));
  EXPECT_NE(otr_profile, regular_profile->GetOffTheRecordProfile(
                             Profile::OTRProfileID::PrimaryID()));

  regular_profile->DestroyOffTheRecordProfile(otr_profile);
  EXPECT_FALSE(regular_profile->HasOffTheRecordProfile(otr_profile_id));
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID()));
  EXPECT_TRUE(regular_profile->HasAnyOffTheRecordProfile());
}

// Verifies creating two OTRs with different ids results in different profiles.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, CreateTwoNonPrimaryOTRs) {
  Profile::OTRProfileID otr_profile_id1("profile::otr1");
  Profile::OTRProfileID otr_profile_id2("profile::otr2");

  Profile* regular_profile = browser()->profile();

  Profile* otr_profile1 =
      regular_profile->GetOffTheRecordProfile(otr_profile_id1);
  Profile* otr_profile2 =
      regular_profile->GetOffTheRecordProfile(otr_profile_id2);

  EXPECT_NE(otr_profile1, otr_profile2);
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(otr_profile_id1));
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(otr_profile_id2));

  regular_profile->DestroyOffTheRecordProfile(otr_profile1);
  EXPECT_FALSE(regular_profile->HasOffTheRecordProfile(otr_profile_id1));
  EXPECT_TRUE(regular_profile->HasOffTheRecordProfile(otr_profile_id2));
}

class ProfileBrowserTestWithoutDestroyProfile : public ProfileBrowserTest {
 public:
  ProfileBrowserTestWithoutDestroyProfile() {
    scoped_feature_list_.InitAndDisableFeature(
        features::kDestroyProfileOnBrowserClose);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies destroying regular profile will result in destruction of OTR
// profiles.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTestWithoutDestroyProfile,
                       DestroyRegularProfileBeforeOTRs) {
  Profile::OTRProfileID otr_profile_id1("profile::otr1");
  Profile::OTRProfileID otr_profile_id2("profile::otr2");

  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  MockProfileDelegate delegate;
  std::unique_ptr<Profile> regular_profile(CreateProfile(
      temp_dir.GetPath(), &delegate, Profile::CREATE_MODE_SYNCHRONOUS));

  // Creating a profile causes an implicit connection attempt to a Mojo
  // service, which occurs as part of a new task. Before deleting |profile|,
  // ensure this task runs to prevent a crash.
  FlushIoTaskRunnerAndSpinThreads();

  Profile* otr_profile1 =
      regular_profile->GetOffTheRecordProfile(otr_profile_id1);
  Profile* otr_profile2 =
      regular_profile->GetOffTheRecordProfile(otr_profile_id2);

  ProfileDestructionWatcher watcher1;
  ProfileDestructionWatcher watcher2;
  watcher1.Watch(otr_profile1);
  watcher2.Watch(otr_profile2);

  ProfileDestroyer::DestroyProfileWhenAppropriate(regular_profile.release());

  EXPECT_TRUE(watcher1.destroyed());
  EXPECT_TRUE(watcher2.destroyed());
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
class ProfileBrowserTestWithDestroyProfile : public ProfileBrowserTest {
 public:
  ProfileBrowserTestWithDestroyProfile() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kDestroyProfileOnBrowserClose);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies the regular Profile doesn't get destroyed as long as there's an OTR
// Profile around.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTestWithDestroyProfile,
                       OTRProfileKeepsRegularProfileAlive) {
  ProfileManager* profile_manager = g_browser_process->profile_manager();
  Profile* regular_profile = browser()->profile();
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kOffTheRecordProfile));

  Profile::OTRProfileID otr_profile_id("profile::otr");
  Profile* otr_profile =
      regular_profile->GetOffTheRecordProfile(otr_profile_id);

  ProfileDestructionWatcher regular_watcher;
  ProfileDestructionWatcher otr_watcher;
  regular_watcher.Watch(regular_profile);
  otr_watcher.Watch(otr_profile);

  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kBrowserWindow));
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kOffTheRecordProfile));

  // Close the browser. Because there's an OTR profile open, the regular Profile
  // shouldn't get deleted.
  browser()->tab_strip_model()->CloseAllTabs();
  base::RunLoop().RunUntilIdle();
  EXPECT_FALSE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kBrowserWindow));
  EXPECT_TRUE(profile_manager->HasKeepAliveForTesting(
      regular_profile, ProfileKeepAliveOrigin::kOffTheRecordProfile));

  // Destroy the OTR profile. *Now* the regular Profile should get deleted.
  ProfileDestroyer::DestroyProfileWhenAppropriate(otr_profile);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(regular_watcher.destroyed());
  EXPECT_TRUE(otr_watcher.destroyed());
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Tests Profile::GetAllOffTheRecordProfiles
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, TestGetAllOffTheRecordProfiles) {
  Profile::OTRProfileID otr_profile_id1("profile::otr1");
  Profile::OTRProfileID otr_profile_id2("profile::otr2");

  Profile* regular_profile = browser()->profile();

  Profile* otr_profile1 =
      regular_profile->GetOffTheRecordProfile(otr_profile_id1);
  Profile* otr_profile2 =
      regular_profile->GetOffTheRecordProfile(otr_profile_id2);
  Profile* incognito_profile = regular_profile->GetOffTheRecordProfile(
      Profile::OTRProfileID::PrimaryID());

  std::vector<Profile*> all_otrs =
      regular_profile->GetAllOffTheRecordProfiles();

  EXPECT_EQ(3u, all_otrs.size());
  EXPECT_TRUE(base::Contains(all_otrs, otr_profile1));
  EXPECT_TRUE(base::Contains(all_otrs, otr_profile2));
  EXPECT_TRUE(base::Contains(all_otrs, incognito_profile));
}

// Tests Profile::IsSameOrParent
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest, TestIsSameOrParent) {
  Profile::OTRProfileID otr_profile_id("profile::otr");

  Profile* regular_profile = browser()->profile();
  Profile* otr_profile =
      regular_profile->GetOffTheRecordProfile(otr_profile_id);
  Profile* incognito_profile = regular_profile->GetPrimaryOTRProfile();

  EXPECT_TRUE(regular_profile->IsSameOrParent(otr_profile));
  EXPECT_TRUE(otr_profile->IsSameOrParent(regular_profile));

  EXPECT_TRUE(regular_profile->IsSameOrParent(incognito_profile));
  EXPECT_TRUE(incognito_profile->IsSameOrParent(regular_profile));

  EXPECT_FALSE(incognito_profile->IsSameOrParent(otr_profile));
  EXPECT_FALSE(otr_profile->IsSameOrParent(incognito_profile));
}

// Tests if browser creation using non primary OTRs is blocked.
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       TestCreatingBrowserUsingNonPrimaryOffTheRecordProfile) {
  Profile::OTRProfileID otr_profile_id("profile::otr");
  Profile* otr_profile =
      browser()->profile()->GetOffTheRecordProfile(otr_profile_id);

  EXPECT_EQ(Browser::CreationStatus::kErrorProfileUnsuitable,
            Browser::GetCreationStatusForProfile(otr_profile));
}

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

// TODO(https://crbug.com/1125474): Expand to cover ChromeOS.
class GuestProfileLifetimeBrowserTest
    : public ProfileBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  GuestProfileLifetimeBrowserTest() : is_ephemeral_(GetParam()) {
    // Change the value if Ephemeral is not supported.
    is_ephemeral_ &=
        TestingProfile::SetScopedFeatureListForEphemeralGuestProfiles(
            scoped_feature_list_, is_ephemeral_);
  }

  bool is_ephemeral() const { return is_ephemeral_; }

 private:
  bool is_ephemeral_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(GuestProfileLifetimeBrowserTest, UnderOneMinute) {
  base::HistogramTester tester;
  Browser* browser = CreateGuestBrowser();
  BrowserCloseObserver close_observer(browser);

  BrowserList::CloseAllBrowsersWithProfile(browser->profile());
  close_observer.Wait();
  tester.ExpectUniqueSample("Profile.Guest.OTR.Lifetime", 0,
                            is_ephemeral() ? 0 : 1);
  tester.ExpectUniqueSample("Profile.Guest.Ephemeral.Lifetime", 0,
                            is_ephemeral() ? 1 : 0);
  tester.ExpectUniqueSample("Profile.Guest.BlankState.Lifetime", 0, 1);
  // TODO(https://crbug.com/1157764): Add test for |SigninTransferred| case.
}

IN_PROC_BROWSER_TEST_P(GuestProfileLifetimeBrowserTest, OneHour) {
  base::HistogramTester tester;
  Browser* browser = CreateGuestBrowser();
  BrowserCloseObserver close_observer(browser);

  browser->profile()->SetCreationTimeForTesting(
      base::Time::Now() - base::TimeDelta::FromSeconds(60) * 60);
  BrowserList::CloseAllBrowsersWithProfile(browser->profile());
  close_observer.Wait();
  tester.ExpectUniqueSample("Profile.Guest.OTR.Lifetime", 60,
                            is_ephemeral() ? 0 : 1);
  tester.ExpectUniqueSample("Profile.Guest.Ephemeral.Lifetime", 60,
                            is_ephemeral() ? 1 : 0);
  tester.ExpectUniqueSample("Profile.Guest.BlankState.Lifetime", 60, 1);
  // TODO(https://crbug.com/1157764): Add test for |SigninTransferred| case.
}

INSTANTIATE_TEST_SUITE_P(AllGuestTypes,
                         GuestProfileLifetimeBrowserTest,
                         /*is_ephemeral=*/testing::Bool());

class EphemeralGuestProfileBrowserTest : public ProfileBrowserTest {
 public:
  EphemeralGuestProfileBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kEnableEphemeralGuestProfilesOnDesktop);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Tests profile type functions on an ephemeral Guest profile.
IN_PROC_BROWSER_TEST_F(EphemeralGuestProfileBrowserTest, TestProfileType) {
  Profile* guest_profile = CreateGuestBrowser()->profile();

  EXPECT_TRUE(guest_profile->IsRegularProfile());
  EXPECT_FALSE(guest_profile->IsOffTheRecord());
  EXPECT_FALSE(guest_profile->IsGuestSession());
  EXPECT_TRUE(guest_profile->IsEphemeralGuestProfile());
}

// Tests if ephemeral Guest profile paths are persistent as long as one does not
// close all Guest browsers.
IN_PROC_BROWSER_TEST_F(EphemeralGuestProfileBrowserTest,
                       TestProfilePathIsStableWhileNotClosed) {
  Browser* guest1 = CreateGuestBrowser();
  base::FilePath guest_path1 = guest1->profile()->GetPath();

  Browser* guest2 = CreateGuestBrowser();
  base::FilePath guest_path2 = guest2->profile()->GetPath();

  EXPECT_EQ(guest_path1, guest_path2);

  CloseBrowserSynchronously(guest1);

  Browser* guest3 = CreateGuestBrowser();
  base::FilePath guest_path3 = guest3->profile()->GetPath();

  EXPECT_EQ(guest_path1, guest_path3);
}

// Tests if closing all ephemeral Guest profiles will result in a new path for
// the next ephemeral Guest profile.
IN_PROC_BROWSER_TEST_F(EphemeralGuestProfileBrowserTest,
                       TestGuestGetsNewPathAfterClosing) {
  Browser* guest1 = CreateGuestBrowser();
  base::FilePath guest_path1 = guest1->profile()->GetPath();

  CloseBrowserSynchronously(guest1);

  Browser* guest2 = CreateGuestBrowser();
  base::FilePath guest_path2 = guest2->profile()->GetPath();

  EXPECT_NE(guest_path1, guest_path2);
}

#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

#if BUILDFLAG(IS_CHROMEOS_LACROS)
IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       IsMainProfileReturnsFalseForNonDefaultPaths) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  {
    base::FilePath profile_path = temp_dir.GetPath().Append("1Default");
    std::unique_ptr<Profile> profile(
        CreateProfile(profile_path, /* delegate= */ nullptr,
                      Profile::CREATE_MODE_SYNCHRONOUS));

    EXPECT_FALSE(profile->IsMainProfile());

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }
  FlushIoTaskRunnerAndSpinThreads();
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       IsMainProfileReturnsTrueForPublicSessions) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  {
    base::FilePath profile_path =
        temp_dir.GetPath().Append(chrome::kInitialProfile);
    std::unique_ptr<Profile> profile(
        CreateProfile(profile_path, /* delegate= */ nullptr,
                      Profile::CREATE_MODE_SYNCHRONOUS));

    crosapi::mojom::BrowserInitParamsPtr init_params =
        crosapi::mojom::BrowserInitParams::New();
    init_params->session_type = crosapi::mojom::SessionType::kPublicSession;
    chromeos::LacrosChromeServiceImpl::Get()->SetInitParamsForTests(
        std::move(init_params));

    EXPECT_TRUE(profile->IsMainProfile());

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }
  FlushIoTaskRunnerAndSpinThreads();
}

IN_PROC_BROWSER_TEST_F(
    ProfileBrowserTest,
    IsMainProfileReturnsTrueForActiveDirectoryEnrolledDevices) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  {
    base::FilePath profile_path =
        temp_dir.GetPath().Append(chrome::kInitialProfile);
    std::unique_ptr<Profile> profile(
        CreateProfile(profile_path, /* delegate= */ nullptr,
                      Profile::CREATE_MODE_SYNCHRONOUS));

    crosapi::mojom::BrowserInitParamsPtr init_params =
        crosapi::mojom::BrowserInitParams::New();
    init_params->session_type = crosapi::mojom::SessionType::kRegularSession;
    init_params->device_mode =
        crosapi::mojom::DeviceMode::kEnterpriseActiveDirectory;
    chromeos::LacrosChromeServiceImpl::Get()->SetInitParamsForTests(
        std::move(init_params));

    EXPECT_TRUE(profile->IsMainProfile());

    // Creating a profile causes an implicit connection attempt to a Mojo
    // service, which occurs as part of a new task. Before deleting |profile|,
    // ensure this task runs to prevent a crash.
    FlushIoTaskRunnerAndSpinThreads();
  }
  FlushIoTaskRunnerAndSpinThreads();
}

// TODO(sinhak): Remove this test after launching go/cros-dent-1-lacros.
IN_PROC_BROWSER_TEST_F(
    ProfileBrowserTest,
    IsMainProfileReturnsTrueForMainProfileInRegularSessions) {
  // Setup.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const std::string kFakePrimaryUsername = "user@example.com";
  const std::string kFakeGaiaId = "fake-gaia-id";
  ProfileAttributesStorage& profile_attributes_storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  const base::FilePath profile_path =
      browser()->profile()->GetPath().DirName().Append(chrome::kInitialProfile);
  // Creates a new Profile and (fake) signs in `kFakeGaiaId`.
  profile_attributes_storage.AddProfile(
      profile_path, base::UTF8ToUTF16(chrome::kInitialProfile), kFakeGaiaId,
      base::UTF8ToUTF16(kFakePrimaryUsername),
      /*is_consented_primary_account=*/false, /*icon_index=*/0,
      /*supervised_user_id*/ std::string(), EmptyAccountId());

  crosapi::mojom::BrowserInitParamsPtr init_params =
      crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kRegularSession;
  init_params->device_mode = crosapi::mojom::DeviceMode::kConsumer;
  init_params->device_account_gaia_id = kFakeGaiaId;
  chromeos::LacrosChromeServiceImpl::Get()->SetInitParamsForTests(
      std::move(init_params));

  // Test.
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  EXPECT_TRUE(profile->IsMainProfile());
}

IN_PROC_BROWSER_TEST_F(ProfileBrowserTest,
                       IsMainProfileReturnsTrueForOTRProfileInRegularSessions) {
  // Setup.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  const std::string kFakePrimaryUsername = "user@example.com";
  const std::string kFakeGaiaId = "fake-gaia-id";
  ProfileAttributesStorage& profile_attributes_storage =
      g_browser_process->profile_manager()->GetProfileAttributesStorage();
  const base::FilePath profile_path =
      browser()->profile()->GetPath().DirName().Append(chrome::kInitialProfile);
  // Creates a new Profile and (fake) signs in `kFakeGaiaId`.
  profile_attributes_storage.AddProfile(
      profile_path, base::UTF8ToUTF16(chrome::kInitialProfile), kFakeGaiaId,
      base::UTF8ToUTF16(kFakePrimaryUsername),
      /*is_consented_primary_account=*/false, /*icon_index=*/0,
      /*supervised_user_id*/ std::string(), EmptyAccountId());

  crosapi::mojom::BrowserInitParamsPtr init_params =
      crosapi::mojom::BrowserInitParams::New();
  init_params->session_type = crosapi::mojom::SessionType::kRegularSession;
  init_params->device_mode = crosapi::mojom::DeviceMode::kConsumer;
  init_params->device_account_gaia_id = kFakeGaiaId;
  chromeos::LacrosChromeServiceImpl::Get()->SetInitParamsForTests(
      std::move(init_params));

  // Test.
  Profile* profile =
      g_browser_process->profile_manager()->GetProfileByPath(profile_path);
  EXPECT_FALSE(profile->GetPrimaryOTRProfile()->IsMainProfile());
}
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
