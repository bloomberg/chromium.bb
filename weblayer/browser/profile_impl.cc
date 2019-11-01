// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/profile_impl.h"

#include "base/bind.h"
#include "base/callback.h"
#include "build/build_config.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browsing_data_remover.h"
#include "content/public/browser/download_manager_delegate.h"
#include "content/public/browser/resource_context.h"
#include "weblayer/browser/browser_controller_impl.h"
#include "weblayer/browser/ssl_host_state_delegate_impl.h"
#include "weblayer/public/download_delegate.h"

#if defined(OS_ANDROID)
#include "base/android/callback_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "weblayer/browser/java/jni/ProfileImpl_jni.h"
#endif

#if defined(OS_ANDROID)
using base::android::AttachCurrentThread;
#endif

namespace weblayer {

namespace {

class ResourceContextImpl : public content::ResourceContext {
 public:
  ResourceContextImpl() = default;
  ~ResourceContextImpl() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResourceContextImpl);
};

class DownloadManagerDelegateImpl : public content::DownloadManagerDelegate {
 public:
  DownloadManagerDelegateImpl() = default;
  ~DownloadManagerDelegateImpl() override = default;

  bool InterceptDownloadIfApplicable(
      const GURL& url,
      const std::string& user_agent,
      const std::string& content_disposition,
      const std::string& mime_type,
      const std::string& request_origin,
      int64_t content_length,
      bool is_transient,
      content::WebContents* web_contents) override {
    // If there's no DownloadDelegate, the download is simply dropped.
    auto* browser = BrowserControllerImpl::FromWebContents(web_contents);
    if (!browser)
      return true;

    DownloadDelegate* delegate = browser->download_delegate();
    if (!delegate)
      return true;

    delegate->DownloadRequested(url, user_agent, content_disposition, mime_type,
                                content_length);
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadManagerDelegateImpl);
};

}  // namespace

class ProfileImpl::BrowserContextImpl : public content::BrowserContext {
 public:
  BrowserContextImpl(const base::FilePath& path) : path_(path) {
    resource_context_ = std::make_unique<ResourceContextImpl>();
    content::BrowserContext::Initialize(this, path_);
  }

  ~BrowserContextImpl() override { NotifyWillBeDestroyed(this); }

  // BrowserContext implementation:
#if !defined(OS_ANDROID)
  std::unique_ptr<content::ZoomLevelDelegate> CreateZoomLevelDelegate(
      const base::FilePath&) override {
    return nullptr;
  }
#endif  // !defined(OS_ANDROID)

  base::FilePath GetPath() override { return path_; }

  bool IsOffTheRecord() override { return path_.empty(); }

  content::DownloadManagerDelegate* GetDownloadManagerDelegate() override {
    return &download_delegate_;
  }

  content::ResourceContext* GetResourceContext() override {
    return resource_context_.get();
  }

  content::BrowserPluginGuestManager* GetGuestManager() override {
    return nullptr;
  }

  storage::SpecialStoragePolicy* GetSpecialStoragePolicy() override {
    return nullptr;
  }

  content::PushMessagingService* GetPushMessagingService() override {
    return nullptr;
  }

  content::StorageNotificationService* GetStorageNotificationService()
      override {
    return nullptr;
  }

  content::SSLHostStateDelegate* GetSSLHostStateDelegate() override {
    return &ssl_host_state_delegate_;
  }

  content::PermissionControllerDelegate* GetPermissionControllerDelegate()
      override {
    return nullptr;
  }

  content::ClientHintsControllerDelegate* GetClientHintsControllerDelegate()
      override {
    return nullptr;
  }

  content::BackgroundFetchDelegate* GetBackgroundFetchDelegate() override {
    return nullptr;
  }

  content::BackgroundSyncController* GetBackgroundSyncController() override {
    return nullptr;
  }

  content::BrowsingDataRemoverDelegate* GetBrowsingDataRemoverDelegate()
      override {
    return nullptr;
  }

  content::ContentIndexProvider* GetContentIndexProvider() override {
    return nullptr;
  }

 private:
  base::FilePath path_;
  std::unique_ptr<ResourceContextImpl> resource_context_;
  DownloadManagerDelegateImpl download_delegate_;
  SSLHostStateDelegateImpl ssl_host_state_delegate_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContextImpl);
};

class ProfileImpl::DataClearer : public content::BrowsingDataRemover::Observer {
 public:
  DataClearer(content::BrowserContext* browser_context,
              base::OnceCallback<void()> callback)
      : remover_(
            content::BrowserContext::GetBrowsingDataRemover(browser_context)),
        callback_(std::move(callback)) {
    remover_->AddObserver(this);
  }

  ~DataClearer() override { remover_->RemoveObserver(this); }

  void ClearData(int mask) {
    int origin_types =
        content::BrowsingDataRemover::ORIGIN_TYPE_UNPROTECTED_WEB |
        content::BrowsingDataRemover::ORIGIN_TYPE_PROTECTED_WEB;
    remover_->RemoveAndReply(base::Time(), base::Time::Max(), mask,
                             origin_types, this);
  }

  void OnBrowsingDataRemoverDone() override {
    std::move(callback_).Run();
    delete this;
  }

 private:
  content::BrowsingDataRemover* const remover_;
  base::OnceCallback<void()> callback_;
};

ProfileImpl::ProfileImpl(const base::FilePath& path) : path_(path) {
  browser_context_ = std::make_unique<BrowserContextImpl>(path_);
}

ProfileImpl::~ProfileImpl() {
  browser_context_->ShutdownStoragePartitions();
}

content::BrowserContext* ProfileImpl::GetBrowserContext() {
  return browser_context_.get();
}

void ProfileImpl::ClearBrowsingData(std::vector<BrowsingDataType> data_types,
                                    base::OnceCallback<void()> callback) {
  auto* clearer = new DataClearer(browser_context_.get(), std::move(callback));
  // DataClearer will delete itself in OnBrowsingDataRemoverDone().
  // If Profile is destroyed during clearing, it would lead to destroying
  // browser_context_ and then BrowsingDataRemover, which in turn would call
  // OnBrowsingDataRemoverDone(), even though the clearing hasn't been finished.

  int remove_mask = 0;
  // This follows what Chrome does: see browsing_data_bridge.cc.
  for (auto data_type : data_types) {
    switch (data_type) {
      case BrowsingDataType::COOKIES_AND_SITE_DATA:
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_COOKIES;
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_DOM_STORAGE;
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_MEDIA_LICENSES;
        break;
      case BrowsingDataType::CACHE:
        remove_mask |= content::BrowsingDataRemover::DATA_TYPE_CACHE;
        break;
      default:
        NOTREACHED();
    }
  }
  clearer->ClearData(remove_mask);
}

std::unique_ptr<Profile> Profile::Create(const base::FilePath& path) {
  return std::make_unique<ProfileImpl>(path);
}

#if defined(OS_ANDROID)
ProfileImpl::ProfileImpl(JNIEnv* env,
                         const base::android::JavaParamRef<jstring>& path)
    : ProfileImpl(base::FilePath(ConvertJavaStringToUTF8(env, path))) {}

static jlong JNI_ProfileImpl_CreateProfile(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& path) {
  return reinterpret_cast<jlong>(new ProfileImpl(env, path));
}

static void JNI_ProfileImpl_DeleteProfile(JNIEnv* env, jlong profile) {
  delete reinterpret_cast<ProfileImpl*>(profile);
}

void ProfileImpl::ClearBrowsingData(
    JNIEnv* env,
    const base::android::JavaParamRef<jintArray>& j_data_types,
    const base::android::JavaRef<jobject>& j_callback) {
  std::vector<int> data_type_ints;
  base::android::JavaIntArrayToIntVector(env, j_data_types, &data_type_ints);
  std::vector<BrowsingDataType> data_types;
  data_types.reserve(data_type_ints.size());
  for (int type : data_type_ints) {
    data_types.push_back(static_cast<BrowsingDataType>(type));
  }
  ClearBrowsingData(
      data_types,
      base::BindOnce(base::android::RunRunnableAndroid,
                     base::android::ScopedJavaGlobalRef<jobject>(j_callback)));
}
#endif  // OS_ANDROID

}  // namespace weblayer
