// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browsing_data/content/local_storage_helper.h"

#include <stddef.h>

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/thread_test_helper.h"
#include "base/threading/thread_restrictions.h"
#include "components/browsing_data/content/browsing_data_helper_browsertest.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/dom_storage_context.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserContext;
using content::BrowserThread;
using content::DOMStorageContext;

namespace browsing_data {
namespace {

using TestCompletionCallback =
    BrowsingDataHelperCallback<content::StorageUsageInfo>;

constexpr base::FilePath::CharType kTestFile0[] =
    FILE_PATH_LITERAL("http_www.chromium.org_0.localstorage");

const char kOriginOfTestFile0[] = "http://www.chromium.org/";

constexpr base::FilePath::CharType kTestFile1[] =
    FILE_PATH_LITERAL("http_www.google.com_0.localstorage");

constexpr base::FilePath::CharType kTestFileInvalid[] =
    FILE_PATH_LITERAL("http_www.google.com_localstorage_0.foo");

// This is only here to test that extension state is not listed by the helper.
constexpr base::FilePath::CharType kTestFileExtension[] = FILE_PATH_LITERAL(
    "chrome-extension_behllobkkfkfnphdnhnkndlbkcpglgmj_0.localstorage");

class LocalStorageHelperTest : public content::ContentBrowserTest {
 protected:
  void CreateLocalStorageFilesForTest() {
    base::ScopedAllowBlockingForTesting allow_blocking;
    // Note: This helper depends on details of how the dom_storage library
    // stores data in the host file system.
    base::FilePath storage_path = GetLocalStoragePath();
    base::CreateDirectory(storage_path);
    static constexpr const base::FilePath::CharType* kFilesToCreate[] = {
        kTestFile0, kTestFile1, kTestFileInvalid, kTestFileExtension};
    for (size_t i = 0; i < base::size(kFilesToCreate); ++i) {
      base::FilePath file_path = storage_path.Append(kFilesToCreate[i]);
      base::WriteFile(file_path, nullptr, 0);
    }
  }

  base::FilePath GetLocalStoragePath() {
    return shell()->web_contents()->GetBrowserContext()->GetPath().AppendASCII(
        "Local Storage");
  }
};

// This class is notified by LocalStorageHelper on the UI thread
// once it finishes fetching the local storage data.
class StopTestOnCallback {
 public:
  explicit StopTestOnCallback(LocalStorageHelper* local_storage_helper)
      : local_storage_helper_(local_storage_helper) {
    DCHECK(local_storage_helper_);
  }

  void Callback(
      const std::list<content::StorageUsageInfo>& local_storage_info) {
    DCHECK_CURRENTLY_ON(BrowserThread::UI);
    // There's no guarantee on the order, ensure these files are there.
    const char* const kTestHosts[] = {"www.chromium.org", "www.google.com"};
    bool test_hosts_found[base::size(kTestHosts)] = {false, false};
    ASSERT_EQ(base::size(kTestHosts), local_storage_info.size());
    for (size_t i = 0; i < base::size(kTestHosts); ++i) {
      for (const auto& info : local_storage_info) {
        ASSERT_EQ(info.origin.scheme(), "http");
        if (info.origin.host() == kTestHosts[i]) {
          ASSERT_FALSE(test_hosts_found[i]);
          test_hosts_found[i] = true;
        }
      }
    }
    for (size_t i = 0; i < base::size(kTestHosts); ++i) {
      ASSERT_TRUE(test_hosts_found[i]) << kTestHosts[i];
    }
    base::RunLoop::QuitCurrentWhenIdleDeprecated();
  }

 private:
  LocalStorageHelper* local_storage_helper_;
};

IN_PROC_BROWSER_TEST_F(LocalStorageHelperTest, CallbackCompletes) {
  scoped_refptr<LocalStorageHelper> local_storage_helper(
      new LocalStorageHelper(shell()->web_contents()->GetBrowserContext()));
  CreateLocalStorageFilesForTest();
  StopTestOnCallback stop_test_on_callback(local_storage_helper.get());
  local_storage_helper->StartFetching(base::Bind(
      &StopTestOnCallback::Callback, base::Unretained(&stop_test_on_callback)));
  // Blocks until StopTestOnCallback::Callback is notified.
  content::RunMessageLoop();
}

// Disable due to flaky. https://crbug.com/1028676
IN_PROC_BROWSER_TEST_F(LocalStorageHelperTest, DISABLED_DeleteSingleFile) {
  scoped_refptr<LocalStorageHelper> local_storage_helper(
      new LocalStorageHelper(shell()->web_contents()->GetBrowserContext()));
  CreateLocalStorageFilesForTest();
  base::RunLoop run_loop;
  local_storage_helper->DeleteOrigin(
      url::Origin::Create(GURL(kOriginOfTestFile0)), run_loop.QuitClosure());
  run_loop.Run();

  // Ensure the file has been deleted.
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FileEnumerator file_enumerator(GetLocalStoragePath(), false,
                                       base::FileEnumerator::FILES);
  int num_files = 0;
  for (base::FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    ASSERT_FALSE(base::FilePath(kTestFile0) == file_path.BaseName());
    ++num_files;
  }
  ASSERT_EQ(3, num_files);
}

IN_PROC_BROWSER_TEST_F(LocalStorageHelperTest, CannedAddLocalStorage) {
  const GURL origin1("http://host1:1/");
  const GURL origin2("http://host2:1/");

  scoped_refptr<CannedLocalStorageHelper> helper(new CannedLocalStorageHelper(
      shell()->web_contents()->GetBrowserContext()));
  helper->Add(url::Origin::Create(origin1));
  helper->Add(url::Origin::Create(origin2));

  TestCompletionCallback callback;
  helper->StartFetching(base::Bind(&TestCompletionCallback::callback,
                                   base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(2u, result.size());
  auto info = result.begin();
  EXPECT_EQ(origin1, info->origin.GetURL());
  info++;
  EXPECT_EQ(origin2, info->origin.GetURL());
}

IN_PROC_BROWSER_TEST_F(LocalStorageHelperTest, CannedUnique) {
  const GURL origin("http://host1:1/");

  scoped_refptr<CannedLocalStorageHelper> helper(new CannedLocalStorageHelper(
      shell()->web_contents()->GetBrowserContext()));
  helper->Add(url::Origin::Create(origin));
  helper->Add(url::Origin::Create(origin));

  TestCompletionCallback callback;
  helper->StartFetching(base::Bind(&TestCompletionCallback::callback,
                                   base::Unretained(&callback)));

  std::list<content::StorageUsageInfo> result = callback.result();

  ASSERT_EQ(1u, result.size());
  EXPECT_EQ(origin, result.begin()->origin.GetURL());
}

}  // namespace
}  // namespace browsing_data
