// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/browsing_data/browsing_data_remover_browsertest_base.h"
#include "base/memory/raw_ptr.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/test/bind.h"
#include "build/buildflag.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browsing_data/browsing_data_file_system_util.h"
#include "chrome/browser/browsing_data/counters/site_data_counting_helper.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/base/filename_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/models/tree_model.h"

#if !defined(OS_ANDROID)
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#endif

namespace {

#if defined(OS_ANDROID)
// TODO(crbug/1179729): Move these functions to
// /chrome/test/base/test_utils.{h|cc}.
base::FilePath GetTestFilePath(const char* dir, const char* file) {
  base::FilePath path;
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::PathService::Get(chrome::DIR_TEST_DATA, &path);
  if (dir)
    path = path.AppendASCII(dir);
  return path.AppendASCII(file);
}

GURL GetTestUrl(const char* dir, const char* file) {
  return net::FilePathToFileURL(GetTestFilePath(dir, file));
}
#endif

// Class for waiting for download manager to be initiailized.
class DownloadManagerWaiter : public content::DownloadManager::Observer {
 public:
  explicit DownloadManagerWaiter(content::DownloadManager* download_manager)
      : initialized_(false), download_manager_(download_manager) {
    download_manager_->AddObserver(this);
  }

  ~DownloadManagerWaiter() override { download_manager_->RemoveObserver(this); }

  void WaitForInitialized() {
    initialized_ = download_manager_->IsManagerInitialized();
    if (initialized_)
      return;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void OnManagerInitialized() override {
    initialized_ = true;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
  bool initialized_;
  raw_ptr<content::DownloadManager> download_manager_;
};

class CookiesTreeObserver : public CookiesTreeModel::Observer {
 public:
  explicit CookiesTreeObserver(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}
  ~CookiesTreeObserver() override = default;

  void TreeModelBeginBatch(CookiesTreeModel* model) override {}

  void TreeModelEndBatch(CookiesTreeModel* model) override {
    std::move(quit_closure_).Run();
  }

  void TreeNodesAdded(ui::TreeModel* model,
                      ui::TreeModelNode* parent,
                      size_t start,
                      size_t count) override {}
  void TreeNodesRemoved(ui::TreeModel* model,
                        ui::TreeModelNode* parent,
                        size_t start,
                        size_t count) override {}
  void TreeNodeChanged(ui::TreeModel* model, ui::TreeModelNode* node) override {
  }

 private:
  base::OnceClosure quit_closure_;
};

// Check if |file| matches any regex in |ignore_file_patterns|.
bool ShouldIgnoreFile(const std::string& file,
                      const std::vector<std::string>& ignore_file_patterns) {
  for (const std::string& pattern : ignore_file_patterns) {
    if (RE2::PartialMatch(file, pattern))
      return true;
  }
  return false;
}

}  // namespace

BrowsingDataRemoverBrowserTestBase::BrowsingDataRemoverBrowserTestBase() =
    default;

BrowsingDataRemoverBrowserTestBase::~BrowsingDataRemoverBrowserTestBase() =
    default;

void BrowsingDataRemoverBrowserTestBase::InitFeatureList(
    std::vector<base::Feature> enabled_features) {
  feature_list_.InitWithFeatures(enabled_features, {});
}

#if !defined(OS_ANDROID)
Browser* BrowsingDataRemoverBrowserTestBase::GetBrowser() const {
  return incognito_browser_ ? incognito_browser_.get() : browser();
}

// Call to use an Incognito browser rather than the default.
void BrowsingDataRemoverBrowserTestBase::UseIncognitoBrowser() {
  ASSERT_EQ(nullptr, incognito_browser_.get());
  incognito_browser_ = CreateIncognitoBrowser();
}

void BrowsingDataRemoverBrowserTestBase::RestartIncognitoBrowser() {
  ASSERT_NE(nullptr, incognito_browser_);
  CloseBrowserSynchronously(incognito_browser_);
  incognito_browser_ = nullptr;
  UseIncognitoBrowser();
}

#endif

void BrowsingDataRemoverBrowserTestBase::SetUpOnMainThread() {
  base::FilePath path;
  base::PathService::Get(content::DIR_TEST_DATA, &path);
  embedded_test_server()->ServeFilesFromDirectory(path);
  ASSERT_TRUE(embedded_test_server()->Start());
}

void BrowsingDataRemoverBrowserTestBase::RunScriptAndCheckResult(
    const std::string& script,
    const std::string& result,
    content::WebContents* web_contents) {
  if (!web_contents)
    web_contents = GetActiveWebContents();
  std::string data;
  ASSERT_TRUE(
      content::ExecuteScriptAndExtractString(web_contents, script, &data));
  ASSERT_EQ(data, result);
}

bool BrowsingDataRemoverBrowserTestBase::RunScriptAndGetBool(
    const std::string& script,
    content::WebContents* web_contents) {
  EXPECT_TRUE(web_contents);
  bool data;
  EXPECT_TRUE(
      content::ExecuteScriptAndExtractBool(web_contents, script, &data));
  return data;
}

void BrowsingDataRemoverBrowserTestBase::VerifyDownloadCount(size_t expected,
                                                             Profile* profile) {
  if (!profile)
    profile = GetProfile();
  content::DownloadManager* download_manager = profile->GetDownloadManager();
  DownloadManagerWaiter download_manager_waiter(download_manager);
  download_manager_waiter.WaitForInitialized();
  std::vector<download::DownloadItem*> downloads;
  download_manager->GetAllDownloads(&downloads);
  EXPECT_EQ(expected, downloads.size());
}

void BrowsingDataRemoverBrowserTestBase::DownloadAnItem() {
  // Start a download.
  content::DownloadManager* download_manager =
      GetProfile()->GetDownloadManager();
  auto observer = std::make_unique<content::DownloadTestObserverTerminal>(
      download_manager, 1,
      content::DownloadTestObserver::ON_DANGEROUS_DOWNLOAD_ACCEPT);

#if defined(OS_ANDROID)
  GURL download_url = GetTestUrl("downloads", "a_zip_file.zip");
  ASSERT_TRUE(content::NavigateToURL(GetActiveWebContents(), download_url,
                                     GURL("about:blank")));
#else
  GURL download_url =
      ui_test_utils::GetTestUrl(base::FilePath().AppendASCII("downloads"),
                                base::FilePath().AppendASCII("a_zip_file.zip"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(GetBrowser(), download_url));
#endif
  observer->WaitForFinished();

  VerifyDownloadCount(1u);
}

bool BrowsingDataRemoverBrowserTestBase::HasDataForType(
    const std::string& type,
    content::WebContents* web_contents) {
  if (!web_contents)
    web_contents = GetActiveWebContents();
  return RunScriptAndGetBool("has" + type + "()", web_contents);
}

void BrowsingDataRemoverBrowserTestBase::SetDataForType(
    const std::string& type,
    content::WebContents* web_contents) {
  if (!web_contents)
    web_contents = GetActiveWebContents();
  ASSERT_TRUE(RunScriptAndGetBool("set" + type + "()", web_contents))
      << "Couldn't create data for: " << type;
}

int BrowsingDataRemoverBrowserTestBase::GetSiteDataCount(
    content::WebContents* web_contents) {
  if (!web_contents)
    web_contents = GetActiveWebContents();
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  base::RunLoop run_loop;
  int count = -1;

  (new SiteDataCountingHelper(profile, base::Time(), base::Time::Max(),
                              base::BindLambdaForTesting([&](int c) {
                                count = c;
                                run_loop.Quit();
                              })))
      ->CountAndDestroySelfWhenFinished();
  run_loop.Run();
  return count;
}

network::mojom::NetworkContext*
BrowsingDataRemoverBrowserTestBase::network_context() {
  return GetProfile()->GetDefaultStoragePartition()->GetNetworkContext();
}

// Returns the active WebContents. On desktop this is in the first browser
// window created by tests, more specific behaviour requires other means.
content::WebContents*
BrowsingDataRemoverBrowserTestBase::GetActiveWebContents() {
#if defined(OS_ANDROID)
  return chrome_test_utils::GetActiveWebContents(this);
#else
  return GetBrowser()->tab_strip_model()->GetActiveWebContents();
#endif
}

#if !defined(OS_ANDROID)
content::WebContents* BrowsingDataRemoverBrowserTestBase::GetActiveWebContents(
    Browser* browser) {
  return browser->tab_strip_model()->GetActiveWebContents();
}
#endif  // !defined(OS_ANDROID)

Profile* BrowsingDataRemoverBrowserTestBase::GetProfile() {
#if defined(OS_ANDROID)
  return chrome_test_utils::GetProfile(this);
#else
  return GetBrowser()->profile();
#endif
}

bool BrowsingDataRemoverBrowserTestBase::CheckUserDirectoryForString(
    const std::string& hostname,
    const std::vector<std::string>& ignore_file_patterns,
    bool check_leveldb_content) {
  base::FilePath user_data_dir =
      g_browser_process->profile_manager()->user_data_dir();
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FileEnumerator enumerator(
      user_data_dir, true /* recursive */,
      base::FileEnumerator::FILES | base::FileEnumerator::DIRECTORIES);
  int found = 0;
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    // Remove |user_data_dir| part from path.
    std::string file =
        path.NormalizePathSeparatorsTo('/').AsUTF8Unsafe().substr(
            user_data_dir.AsUTF8Unsafe().length());

    // Check file name.
    if (file.find(hostname) != std::string::npos) {
      if (ShouldIgnoreFile(file, ignore_file_patterns)) {
        LOG(INFO) << "Ignored: " << file;
      } else {
        found++;
        LOG(WARNING) << "Found file name: " << file;
      }
    }

    // Check leveldb content.
    if (check_leveldb_content && path.BaseName().AsUTF8Unsafe() == "CURRENT") {
      // LevelDB instances consist of a folder where most files have variable
      // names that contain a revision number.
      // All leveldb folders have a "CURRENT" file that points to the current
      // manifest. We consider all folders with a CURRENT file to be leveldb
      // instances and try to open them.
      std::unique_ptr<leveldb::DB> db;
      std::string db_file = path.DirName().AsUTF8Unsafe();
      auto status = leveldb_env::OpenDB(leveldb_env::Options(), db_file, &db);
      if (status.ok()) {
        std::unique_ptr<leveldb::Iterator> it(
            db->NewIterator(leveldb::ReadOptions()));
        for (it->SeekToFirst(); it->Valid(); it->Next()) {
          std::string entry =
              it->key().ToString() + ":" + it->value().ToString();
          if (entry.find(hostname) != std::string::npos) {
            LOG(WARNING) << "Found leveldb entry: " << file << " " << entry;
            found++;
          }
        }
      } else {
        // TODO(https://crbug.com/1238325): Most databases are already open and
        // the LOCK prevents us from accessing them.
        LOG(INFO) << "Could not open: " << file << " " << status.ToString();
      }
    }

    // TODO(crbug.com/846297): Add support for sqlite and other formats that
    // possibly contain non-plaintext data.

    // Check file content.
    if (enumerator.GetInfo().IsDirectory())
      continue;
    std::string content;
    if (!base::ReadFileToString(path, &content)) {
      LOG(INFO) << "Could not read: " << file;
      continue;
    }
    size_t pos = content.find(hostname);
    if (pos != std::string::npos) {
      if (ShouldIgnoreFile(file, ignore_file_patterns)) {
        LOG(INFO) << "Ignored: " << file;
        continue;
      }
      found++;
      // Print surrounding text of the match.
      std::string partial_content = content.substr(
          pos < 30 ? 0 : pos - 30,
          std::min(content.size() - 1, pos + hostname.size() + 30));
      LOG(WARNING) << "Found file content: " << file << "\n"
                   << partial_content << "\n";
    }
  }
  return found;
}

int BrowsingDataRemoverBrowserTestBase::GetCookiesTreeModelCount(
    const CookieTreeNode* root) {
  int count = 0;
  for (const auto& node : root->children()) {
    EXPECT_GE(node->children().size(), 1u);
    count += std::count_if(node->children().cbegin(), node->children().cend(),
                           [](const auto& child) {
                             // TODO(crbug.com/642955): Include quota nodes.
                             return child->GetDetailedInfo().node_type !=
                                    CookieTreeNode::DetailedInfo::TYPE_QUOTA;
                           });
  }
  return count;
}

std::string BrowsingDataRemoverBrowserTestBase::GetCookiesTreeModelInfo(
    const CookieTreeNode* root) {
  std::stringstream info;
  info << "CookieTreeModel: " << std::endl;
  for (const auto& node : root->children()) {
    info << node->GetTitle() << std::endl;
    for (const auto& child : node->children()) {
      // Quota nodes are not included in the UI due to crbug.com/642955.
      const auto node_type = child->GetDetailedInfo().node_type;
      if (node_type != CookieTreeNode::DetailedInfo::TYPE_QUOTA)
        info << "  " << child->GetTitle() << " " << node_type << std::endl;
    }
  }
  return info.str();
}

std::unique_ptr<CookiesTreeModel>
BrowsingDataRemoverBrowserTestBase::GetCookiesTreeModel(Profile* profile) {
  content::StoragePartition* storage_partition =
      profile->GetDefaultStoragePartition();
  content::ServiceWorkerContext* service_worker_context =
      storage_partition->GetServiceWorkerContext();
  storage::FileSystemContext* file_system_context =
      storage_partition->GetFileSystemContext();
  content::NativeIOContext* native_io_context =
      storage_partition->GetNativeIOContext();
  auto container = std::make_unique<LocalDataContainer>(
      base::MakeRefCounted<browsing_data::CookieHelper>(
          storage_partition,
          CookiesTreeModel::GetCookieDeletionDisabledCallback(profile)),
      base::MakeRefCounted<browsing_data::DatabaseHelper>(profile),
      base::MakeRefCounted<browsing_data::LocalStorageHelper>(profile),
      /*session_storage_helper=*/nullptr,
      base::MakeRefCounted<browsing_data::IndexedDBHelper>(storage_partition),
      base::MakeRefCounted<browsing_data::FileSystemHelper>(
          file_system_context,
          browsing_data_file_system_util::GetAdditionalFileSystemTypes(),
          native_io_context),
      BrowsingDataQuotaHelper::Create(profile),
      base::MakeRefCounted<browsing_data::ServiceWorkerHelper>(
          service_worker_context),
      base::MakeRefCounted<browsing_data::SharedWorkerHelper>(
          storage_partition),
      base::MakeRefCounted<browsing_data::CacheStorageHelper>(
          storage_partition),
      BrowsingDataMediaLicenseHelper::Create(file_system_context));
  base::RunLoop run_loop;
  CookiesTreeObserver observer(run_loop.QuitClosure());
  auto model = std::make_unique<CookiesTreeModel>(
      std::move(container), profile->GetExtensionSpecialStoragePolicy());
  model->AddCookiesTreeObserver(&observer);
  run_loop.Run();
  return model;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
bool BrowsingDataRemoverBrowserTestBase::SetGaiaCookieForProfile(
    Profile* profile) {
  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();
  auto cookie = net::CanonicalCookie::CreateUnsafeCookieForTesting(
      "SAPISID", std::string(), "." + google_url.host(), "/", base::Time(),
      base::Time(), base::Time(), true /* secure */, false /* httponly */,
      net::CookieSameSite::NO_RESTRICTION, net::COOKIE_PRIORITY_DEFAULT,
      false /* same_party */);
  bool success = false;
  base::RunLoop loop;
  base::OnceCallback<void(net::CookieAccessResult)> callback =
      base::BindLambdaForTesting([&success, &loop](net::CookieAccessResult r) {
        success = r.status.IsInclude();
        loop.Quit();
      });
  network::mojom::CookieManager* cookie_manager =
      profile->GetDefaultStoragePartition()
          ->GetCookieManagerForBrowserProcess();
  cookie_manager->SetCanonicalCookie(*cookie, google_url,
                                     net::CookieOptions::MakeAllInclusive(),
                                     std::move(callback));
  loop.Run();
  return success;
}
#endif
