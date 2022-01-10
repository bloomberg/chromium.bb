// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/download/save_package.h"
#include "content/browser/web_package/web_bundle_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/download_test_observer.h"
#include "content/shell/browser/shell.h"
#include "content/shell/browser/shell_download_manager_delegate.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace content {

namespace {

const char kTestFile[] = "/simple_page.html";

class TestShellDownloadManagerDelegate : public ShellDownloadManagerDelegate {
 public:
  explicit TestShellDownloadManagerDelegate(SavePageType save_page_type)
      : save_page_type_(save_page_type) {}

  void ChooseSavePath(WebContents* web_contents,
                      const base::FilePath& suggested_path,
                      const base::FilePath::StringType& default_extension,
                      bool can_save_as_complete,
                      SavePackagePathPickedCallback callback) override {
    base::FilePath suggested_path_copy = suggested_path;
    if (save_page_type_ == SAVE_PAGE_TYPE_AS_WEB_BUNDLE) {
      suggested_path_copy =
          suggested_path_copy.ReplaceExtension(FILE_PATH_LITERAL("wbn"));
    }
    std::move(callback).Run(suggested_path_copy, save_page_type_,
                            SavePackageDownloadCreatedCallback());
  }

  void GetSaveDir(BrowserContext* context,
                  base::FilePath* website_save_dir,
                  base::FilePath* download_save_dir) override {
    *website_save_dir = download_dir_;
    *download_save_dir = download_dir_;
  }

  bool ShouldCompleteDownload(download::DownloadItem* download,
                              base::OnceClosure closure) override {
    return true;
  }

  base::FilePath download_dir_;
  SavePageType save_page_type_;
};

class DownloadicidalObserver : public DownloadManager::Observer {
 public:
  DownloadicidalObserver(bool remove_download, base::OnceClosure after_closure)
      : remove_download_(remove_download),
        after_closure_(std::move(after_closure)) {}
  void OnDownloadCreated(DownloadManager* manager,
                         download::DownloadItem* item) override {
    base::SequencedTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(
                       [](bool remove_download, base::OnceClosure closure,
                          download::DownloadItem* item) {
                         remove_download ? item->Remove() : item->Cancel(true);
                         std::move(closure).Run();
                       },
                       remove_download_, std::move(after_closure_), item));
  }

 private:
  bool remove_download_;
  base::OnceClosure after_closure_;
};

class DownloadCompleteObserver : public DownloadManager::Observer {
 public:
  explicit DownloadCompleteObserver(base::OnceClosure completed_closure)
      : completed_closure_(std::move(completed_closure)) {}
  DownloadCompleteObserver(const DownloadCompleteObserver&) = delete;
  DownloadCompleteObserver& operator=(const DownloadCompleteObserver&) = delete;

  void OnDownloadCreated(DownloadManager* manager,
                         download::DownloadItem* item) override {
    DCHECK(!item_observer_);
    mime_type_ = item->GetMimeType();
    target_file_path_ = item->GetTargetFilePath();
    item_observer_ = std::make_unique<DownloadItemCompleteObserver>(
        item, std::move(completed_closure_));
  }

  const std::string& mime_type() const { return mime_type_; }
  const base::FilePath& target_file_path() const { return target_file_path_; }

 private:
  class DownloadItemCompleteObserver : public download::DownloadItem::Observer {
   public:
    DownloadItemCompleteObserver(download::DownloadItem* item,
                                 base::OnceClosure completed_closure)
        : item_(item), completed_closure_(std::move(completed_closure)) {
      item_->AddObserver(this);
    }
    DownloadItemCompleteObserver(const DownloadItemCompleteObserver&) = delete;
    DownloadItemCompleteObserver& operator=(
        const DownloadItemCompleteObserver&) = delete;

    ~DownloadItemCompleteObserver() override {
      if (item_)
        item_->RemoveObserver(this);
    }

   private:
    void OnDownloadUpdated(download::DownloadItem* item) override {
      DCHECK_EQ(item_, item);
      if (item_->GetState() == download::DownloadItem::COMPLETE)
        std::move(completed_closure_).Run();
    }

    void OnDownloadDestroyed(download::DownloadItem* item) override {
      DCHECK_EQ(item_, item);
      item_->RemoveObserver(this);
      item_ = nullptr;
    }

    raw_ptr<download::DownloadItem> item_;
    base::OnceClosure completed_closure_;
  };

  std::unique_ptr<DownloadItemCompleteObserver> item_observer_;
  base::OnceClosure completed_closure_;
  std::string mime_type_;
  base::FilePath target_file_path_;
};

}  // namespace

class SavePackageBrowserTest : public ContentBrowserTest {
 protected:
  void SetUp() override {
    ASSERT_TRUE(save_dir_.CreateUniqueTempDir());
    ContentBrowserTest::SetUp();
  }

  // Returns full paths of destination file and directory.
  void GetDestinationPaths(const std::string& prefix,
                           base::FilePath* full_file_name,
                           base::FilePath* dir) {
    *full_file_name = save_dir_.GetPath().AppendASCII(prefix + ".htm");
    *dir = save_dir_.GetPath().AppendASCII(prefix + "_files");
  }

  // Start a SavePackage download and then cancels it. If |remove_download| is
  // true, the download item will be removed while page is being saved.
  // Otherwise, the download item will be canceled.
  void RunAndCancelSavePackageDownload(SavePageType save_page_type,
                                       bool remove_download) {
    ASSERT_TRUE(embedded_test_server()->Start());
    GURL url = embedded_test_server()->GetURL("/page_with_iframe.html");
    EXPECT_TRUE(NavigateToURL(shell(), url));
    auto* download_manager = static_cast<DownloadManagerImpl*>(
        shell()->web_contents()->GetBrowserContext()->GetDownloadManager());
    auto delegate =
        std::make_unique<TestShellDownloadManagerDelegate>(save_page_type);
    delegate->download_dir_ = save_dir_.GetPath();
    auto* old_delegate = download_manager->GetDelegate();
    download_manager->SetDelegate(delegate.get());

    {
      base::RunLoop run_loop;
      DownloadicidalObserver download_item_killer(remove_download,
                                                  run_loop.QuitClosure());
      download_manager->AddObserver(&download_item_killer);

      scoped_refptr<SavePackage> save_package(
          new SavePackage(shell()->web_contents()->GetPrimaryPage()));
      save_package->GetSaveInfo();
      run_loop.Run();
      download_manager->RemoveObserver(&download_item_killer);
      EXPECT_TRUE(save_package->canceled());
    }

    // Run a second download to completion so that any pending tasks will get
    // flushed out. If the previous SavePackage operation didn't cleanup after
    // itself, then there could be stray tasks that invoke the now defunct
    // download item.
    {
      base::RunLoop run_loop;
      SavePackageFinishedObserver finished_observer(download_manager,
                                                    run_loop.QuitClosure());
      shell()->web_contents()->OnSavePage();
      run_loop.Run();
    }
    download_manager->SetDelegate(old_delegate);
  }

  // Temporary directory we will save pages to.
  base::ScopedTempDir save_dir_;
};

// Create a SavePackage and delete it without calling Init.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ImplicitCancel) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kTestFile);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(
      new SavePackage(shell()->web_contents()->GetPrimaryPage(),
                      SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name, dir));
}

// Create a SavePackage, call Cancel, then delete it.
// SavePackage dtor has various asserts/checks that should not fire.
IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, ExplicitCancel) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(kTestFile);
  EXPECT_TRUE(NavigateToURL(shell(), url));
  base::FilePath full_file_name, dir;
  GetDestinationPaths("a", &full_file_name, &dir);
  scoped_refptr<SavePackage> save_package(
      new SavePackage(shell()->web_contents()->GetPrimaryPage(),
                      SAVE_PAGE_TYPE_AS_ONLY_HTML, full_file_name, dir));
  save_package->Cancel(true);
}

IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, DownloadItemDestroyed) {
  RunAndCancelSavePackageDownload(SAVE_PAGE_TYPE_AS_COMPLETE_HTML, true);
}

IN_PROC_BROWSER_TEST_F(SavePackageBrowserTest, DownloadItemCanceled) {
  RunAndCancelSavePackageDownload(SAVE_PAGE_TYPE_AS_MHTML, false);
}

class SavePackageWebBundleBrowserTest : public SavePackageBrowserTest {
 public:
  void SetUp() override {
    std::vector<base::Feature> enable_features;
    std::vector<base::Feature> disabled_features;
    enable_features.push_back(features::kSavePageAsWebBundle);
    enable_features.push_back(features::kWebBundles);
    feature_list_.InitWithFeatures(enable_features, disabled_features);

    SavePackageBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(SavePackageWebBundleBrowserTest, OnePageSimple) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/web_bundle/save_page_as_web_bundle/one_page_simple.html");
  EXPECT_TRUE(NavigateToURL(shell(), url));
  auto* download_manager = static_cast<DownloadManagerImpl*>(
      shell()->web_contents()->GetBrowserContext()->GetDownloadManager());
  auto delegate = std::make_unique<TestShellDownloadManagerDelegate>(
      SAVE_PAGE_TYPE_AS_WEB_BUNDLE);
  delegate->download_dir_ = save_dir_.GetPath();
  auto* old_delegate = download_manager->GetDelegate();
  download_manager->SetDelegate(delegate.get());

  GURL wbn_file_url;
  {
    base::RunLoop run_loop;
    DownloadCompleteObserver observer(run_loop.QuitClosure());
    download_manager->AddObserver(&observer);
    scoped_refptr<SavePackage> save_package(
        new SavePackage(shell()->web_contents()->GetPrimaryPage()));
    save_package->GetSaveInfo();
    run_loop.Run();
    download_manager->RemoveObserver(&observer);
    EXPECT_TRUE(save_package->finished());
    EXPECT_EQ("application/webbundle", observer.mime_type());
    wbn_file_url = net::FilePathToFileURL(observer.target_file_path());
  }

  download_manager->SetDelegate(old_delegate);
  EXPECT_TRUE(NavigateToURL(shell(), GURL("about:blank")));

  std::u16string expected_title = u"Hello";
  TitleWatcher title_watcher(shell()->web_contents(), expected_title);
  EXPECT_TRUE(NavigateToURL(
      shell(), wbn_file_url,
      web_bundle_utils::GetSynthesizedUrlForWebBundle(wbn_file_url, url)));
  EXPECT_EQ(expected_title, title_watcher.WaitAndGetTitle());
}

}  // namespace content
