// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "chrome/browser/web_applications/web_app_install_task.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/installable/installable_metrics.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ssl/security_state_tab_helper.h"
#include "chrome/browser/web_applications/components/app_registrar.h"
#include "chrome/browser/web_applications/components/app_shortcut_manager.h"
#include "chrome/browser/web_applications/components/file_handler_manager.h"
#include "chrome/browser/web_applications/components/install_bounce_metric.h"
#include "chrome/browser/web_applications/components/web_app_constants.h"
#include "chrome/browser/web_applications/components/web_app_data_retriever.h"
#include "chrome/browser/web_applications/components/web_app_helpers.h"
#include "chrome/browser/web_applications/components/web_app_icon_generator.h"
#include "chrome/browser/web_applications/components/web_app_install_utils.h"
#include "chrome/browser/web_applications/components/web_app_url_loader.h"
#include "chrome/browser/web_applications/components/web_app_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/web_application_info.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/blink/public/common/manifest/manifest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/mojom/app.mojom.h"
#include "components/arc/mojom/intent_helper.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "net/base/url_util.h"
#endif

namespace web_app {

namespace {

#if defined(OS_CHROMEOS)
const char kChromeOsPlayPlatform[] = "chromeos_play";
const char kPlayIntentPrefix[] =
    "https://play.google.com/store/apps/details?id=";
const char kPlayStorePackage[] = "com.android.vending";

constexpr bool kAddAppsToQuickLaunchBarByDefault = false;

std::string ExtractQueryValueForName(const GURL& url, const std::string& name) {
  for (net::QueryIterator it(url); !it.IsAtEnd(); it.Advance()) {
    if (it.GetKey() == name)
      return it.GetValue();
  }
  return std::string();
}
#else
constexpr bool kAddAppsToQuickLaunchBarByDefault = true;
#endif  // defined(OS_CHROMEOS)

}  // namespace

WebAppInstallTask::WebAppInstallTask(
    Profile* profile,
    AppRegistrar* registrar,
    AppShortcutManager* shortcut_manager,
    FileHandlerManager* file_handler_manager,
    InstallFinalizer* install_finalizer,
    std::unique_ptr<WebAppDataRetriever> data_retriever)
    : data_retriever_(std::move(data_retriever)),
      registrar_(registrar),
      shortcut_manager_(shortcut_manager),
      file_handler_manager_(file_handler_manager),
      install_finalizer_(install_finalizer),
      profile_(profile) {}

WebAppInstallTask::~WebAppInstallTask() = default;

void WebAppInstallTask::ExpectAppId(const AppId& expected_app_id) {
  expected_app_id_ = expected_app_id;
}

void WebAppInstallTask::SetInstallParams(
    const InstallManager::InstallParams& install_params) {
  if (!install_params.locally_installed) {
    DCHECK(!install_params.add_to_applications_menu);
    DCHECK(!install_params.add_to_desktop);
    DCHECK(!install_params.add_to_quick_launch_bar);
  }
  install_params_ = install_params;
}

void WebAppInstallTask::LoadWebAppAndCheckInstallability(
    const GURL& url,
    WebappInstallSource install_source,
    WebAppUrlLoader* url_loader,
    LoadWebAppAndCheckInstallabilityCallback callback) {
  DCHECK(url_loader);
  // Create a WebContents instead of reusing a shared one because we will pass
  // it back to be used for opening the web app.
  // TODO(loyso): Implement stealing of shared web_contents in upcoming
  // WebContentsManager.
  std::unique_ptr<content::WebContents> web_contents =
      CreateWebContents(profile_);

  // Grab WebContents pointer now, before the call to BindOnce might null out
  // |web_contents|.
  content::WebContents* web_contents_ptr = web_contents.get();

  Observe(web_contents.get());
  background_installation_ = false;
  install_callback_ =
      base::BindOnce(std::move(callback), std::move(web_contents));
  install_source_ = install_source;

  url_loader->LoadUrl(
      url, web_contents_ptr,
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(
          &WebAppInstallTask::
              OnWebAppUrlLoadedCheckInstallabilityAndRetrieveManifest,
          base::Unretained(this), web_contents_ptr));
}

void WebAppInstallTask::InstallWebAppFromManifest(
    content::WebContents* contents,
    WebappInstallSource install_source,
    InstallManager::WebAppInstallDialogCallback dialog_callback,
    InstallManager::OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  auto web_app_info = std::make_unique<WebApplicationInfo>();

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(), /*bypass_service_worker_check=*/false,
      base::BindOnce(&WebAppInstallTask::OnDidPerformInstallableCheck,
                     base::Unretained(this), std::move(web_app_info),
                     /*force_shortcut_app=*/false));
}

void WebAppInstallTask::InstallWebAppFromManifestWithFallback(
    content::WebContents* contents,
    bool force_shortcut_app,
    WebappInstallSource install_source,
    InstallManager::WebAppInstallDialogCallback dialog_callback,
    InstallManager::OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  dialog_callback_ = std::move(dialog_callback);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  data_retriever_->GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebApplicationInfo,
                     base::Unretained(this), force_shortcut_app));
}

void WebAppInstallTask::LoadAndInstallWebAppFromManifestWithFallback(
    const GURL& launch_url,
    content::WebContents* contents,
    WebAppUrlLoader* url_loader,
    WebappInstallSource install_source,
    InstallManager::OnceInstallCallback install_callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  Observe(contents);
  background_installation_ = true;
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;

  url_loader->LoadUrl(
      launch_url, contents,
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&WebAppInstallTask::OnWebAppUrlLoadedGetWebApplicationInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppInstallTask::InstallWebAppFromInfo(
    std::unique_ptr<WebApplicationInfo> web_application_info,
    ForInstallableSite for_installable_site,
    WebappInstallSource install_source,
    InstallManager::OnceInstallCallback callback) {
  DCHECK(AreWebAppsUserInstallable(profile_));
  CheckInstallPreconditions();

  FilterAndResizeIconsGenerateMissing(web_application_info.get(),
                                      /*icons_map*/ nullptr);

  install_source_ = install_source;
  background_installation_ = true;

  RecordInstallEvent(for_installable_site);

  InstallFinalizer::FinalizeOptions options;
  options.install_source = install_source;
  options.locally_installed = true;

  install_finalizer_->FinalizeInstall(*web_application_info, options,
                                      std::move(callback));
}

void WebAppInstallTask::InstallWebAppWithParams(
    content::WebContents* contents,
    const InstallManager::InstallParams& install_params,
    WebappInstallSource install_source,
    InstallManager::OnceInstallCallback install_callback) {
  CheckInstallPreconditions();

  Observe(contents);
  SetInstallParams(install_params);
  install_callback_ = std::move(install_callback);
  install_source_ = install_source;
  background_installation_ = true;

  data_retriever_->GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebApplicationInfo,
                     base::Unretained(this), /*force_shortcut_app=*/false));
}

void WebAppInstallTask::UpdateWebAppFromInfo(
    content::WebContents* web_contents,
    const AppId& app_id,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    InstallManager::OnceInstallCallback callback) {
  Observe(web_contents);
  install_callback_ = std::move(callback);
  background_installation_ = true;

  std::vector<GURL> icon_urls =
      GetValidIconUrlsToDownload(*web_application_info);

  data_retriever_->GetIcons(
      web_contents, std::move(icon_urls),
      /*skip_page_fav_icons=*/true, WebAppIconDownloader::Histogram::kForUpdate,
      base::BindOnce(&WebAppInstallTask::OnIconsRetrievedFinalizeUpdate,
                     base::Unretained(this), std::move(web_application_info)));
}

void WebAppInstallTask::LoadAndRetrieveWebApplicationInfoWithIcons(
    const GURL& app_url,
    WebAppUrlLoader* url_loader,
    RetrieveWebApplicationInfoWithIconsCallback callback) {
  CheckInstallPreconditions();

  retrieve_info_callback_ = std::move(callback);
  background_installation_ = true;
  only_retrieve_web_application_info_ = true;

  web_contents_ = CreateWebContents(profile_);
  Observe(web_contents_.get());

  DCHECK(url_loader);
  url_loader->LoadUrl(
      app_url, web_contents(),
      WebAppUrlLoader::UrlComparison::kIgnoreQueryParamsAndRef,
      base::BindOnce(&WebAppInstallTask::OnWebAppUrlLoadedGetWebApplicationInfo,
                     weak_ptr_factory_.GetWeakPtr()));
}

// static
std::unique_ptr<content::WebContents> WebAppInstallTask::CreateWebContents(
    Profile* profile) {
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(content::WebContents::CreateParams(profile));

  InstallableManager::CreateForWebContents(web_contents.get());
  SecurityStateTabHelper::CreateForWebContents(web_contents.get());
  favicon::CreateContentFaviconDriverForWebContents(web_contents.get());
  if (auto* performance_manager_registry =
          performance_manager::PerformanceManagerRegistry::GetInstance()) {
    performance_manager_registry->CreatePageNodeForWebContents(
        web_contents.get());
  }

  return web_contents;
}

void WebAppInstallTask::WebContentsDestroyed() {
  CallInstallCallback(AppId(), InstallResultCode::kWebContentsDestroyed);
}

void WebAppInstallTask::SetInstallFinalizerForTesting(
    InstallFinalizer* install_finalizer) {
  install_finalizer_ = install_finalizer;
}

void WebAppInstallTask::CheckInstallPreconditions() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Concurrent calls are not allowed.
  DCHECK(!web_contents());
  CHECK(!install_callback_);
  CHECK(!retrieve_info_callback_);
}

void WebAppInstallTask::RecordInstallEvent(
    ForInstallableSite for_installable_site) {
  DCHECK(install_source_ != kNoInstallSource);

  if (InstallableMetrics::IsReportableInstallSource(install_source_) &&
      for_installable_site == ForInstallableSite::kYes) {
    InstallableMetrics::TrackInstallEvent(install_source_);
  }
}

void WebAppInstallTask::CallInstallCallback(const AppId& app_id,
                                            InstallResultCode code) {
  Observe(nullptr);
  dialog_callback_.Reset();

  install_source_ = kNoInstallSource;

  if (only_retrieve_web_application_info_) {
    DCHECK(retrieve_info_callback_);
    std::move(retrieve_info_callback_).Run(std::move(web_application_info_));
    return;
  }

  DCHECK(install_callback_);
  std::move(install_callback_).Run(app_id, code);
}

bool WebAppInstallTask::ShouldStopInstall() const {
  // Install should stop early if WebContents is being destroyed.
  // WebAppInstallTask::WebContentsDestroyed will get called eventually and the
  // callback will be invoked at that point.
  return !web_contents() || web_contents()->IsBeingDestroyed();
}

void WebAppInstallTask::OnWebAppUrlLoadedGetWebApplicationInfo(
    WebAppUrlLoader::Result result) {
  if (ShouldStopInstall())
    return;

  if (result == WebAppUrlLoader::Result::kRedirectedUrlLoaded) {
    CallInstallCallback(AppId(), InstallResultCode::kInstallURLRedirected);
    return;
  }

  if (result == WebAppUrlLoader::Result::kFailedPageTookTooLong) {
    CallInstallCallback(AppId(), InstallResultCode::kInstallURLLoadTimeOut);
    return;
  }

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    CallInstallCallback(AppId(), InstallResultCode::kInstallURLLoadFailed);
    return;
  }

  data_retriever_->GetWebApplicationInfo(
      web_contents(),
      base::BindOnce(&WebAppInstallTask::OnGetWebApplicationInfo,
                     base::Unretained(this), /*force_shortcut_app*/ false));
}

void WebAppInstallTask::OnWebAppUrlLoadedCheckInstallabilityAndRetrieveManifest(
    content::WebContents* web_contents,
    WebAppUrlLoader::Result result) {
  if (ShouldStopInstall())
    return;

  if (result == WebAppUrlLoader::Result::kRedirectedUrlLoaded) {
    CallInstallCallback(AppId(), InstallResultCode::kInstallURLRedirected);
    return;
  }

  if (result == WebAppUrlLoader::Result::kFailedPageTookTooLong) {
    CallInstallCallback(AppId(), InstallResultCode::kInstallURLLoadTimeOut);
    return;
  }

  if (result != WebAppUrlLoader::Result::kUrlLoaded) {
    CallInstallCallback(AppId(), InstallResultCode::kInstallURLLoadFailed);
    return;
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents,
      /*bypass_service_worker_check=*/false,
      base::BindOnce(&WebAppInstallTask::OnWebAppInstallabilityChecked,
                     base::Unretained(this)));
}

void WebAppInstallTask::OnWebAppInstallabilityChecked(
    base::Optional<blink::Manifest> manifest,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  if (ShouldStopInstall())
    return;

  if (is_installable) {
    DCHECK(manifest);
    CallInstallCallback(GenerateAppIdFromURL(manifest->start_url),
                        InstallResultCode::kSuccessNewInstall);
  } else {
    CallInstallCallback(AppId(), InstallResultCode::kNotInstallable);
  }
}

void WebAppInstallTask::OnGetWebApplicationInfo(
    bool force_shortcut_app,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (ShouldStopInstall())
    return;

  if (!web_app_info) {
    CallInstallCallback(AppId(),
                        InstallResultCode::kGetWebApplicationInfoFailed);
    return;
  }

  bool bypass_service_worker_check = false;
  if (install_params_) {
    bypass_service_worker_check = install_params_->bypass_service_worker_check;
    // Set app_url to fallback_start_url as web_contents may have been
    // redirected. Will be overridden by manifest values if present.
    DCHECK(install_params_->fallback_start_url.is_valid());
    web_app_info->app_url = install_params_->fallback_start_url;

    if (install_params_->fallback_app_name.has_value())
      web_app_info->title = install_params_->fallback_app_name.value();

    // If `additional_search_terms` was a manifest property, it would be
    // sanitized while parsing the manifest. Since it's not, we sanitize it
    // here.
    for (std::string& search_term : install_params_->additional_search_terms) {
      if (!search_term.empty())
        web_app_info->additional_search_terms.push_back(std::move(search_term));
    }
  }

  data_retriever_->CheckInstallabilityAndRetrieveManifest(
      web_contents(), bypass_service_worker_check,
      base::BindOnce(&WebAppInstallTask::OnDidPerformInstallableCheck,
                     base::Unretained(this), std::move(web_app_info),
                     force_shortcut_app));
}

void WebAppInstallTask::OnDidPerformInstallableCheck(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    bool force_shortcut_app,
    base::Optional<blink::Manifest> manifest,
    bool valid_manifest_for_web_app,
    bool is_installable) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);
  DCHECK(!manifest || !manifest->IsEmpty());

  if (install_params_ && install_params_->require_manifest &&
      !valid_manifest_for_web_app) {
    LOG(WARNING) << "Did not install " << web_app_info->app_url.spec()
                 << " because it didn't have a manifest for web app";
    CallInstallCallback(AppId(), InstallResultCode::kNotValidManifestForWebApp);
    return;
  }

  const auto for_installable_site = is_installable && !force_shortcut_app
                                        ? ForInstallableSite::kYes
                                        : ForInstallableSite::kNo;

  if (manifest)
    UpdateWebAppInfoFromManifest(*manifest, web_app_info.get());

  AppId app_id = GenerateAppIdFromURL(web_app_info->app_url);

  // Do the app_id expectation check if requested.
  if (expected_app_id_.has_value() && *expected_app_id_ != app_id) {
    CallInstallCallback(std::move(app_id),
                        InstallResultCode::kExpectedAppIdCheckFailed);
    return;
  }

  std::vector<GURL> icon_urls = GetValidIconUrlsToDownload(*web_app_info);

  // A system app should always have a manifest icon.
  if (install_source_ == WebappInstallSource::SYSTEM_DEFAULT) {
    DCHECK(manifest);
    DCHECK(!manifest->icons.empty());
  }

  // If the manifest specified icons, don't use the page icons.
  const bool skip_page_favicons = manifest && !manifest->icons.empty();

  CheckForPlayStoreIntentOrGetIcons(manifest, std::move(web_app_info),
                                    std::move(icon_urls), for_installable_site,
                                    skip_page_favicons);
}

void WebAppInstallTask::CheckForPlayStoreIntentOrGetIcons(
    base::Optional<blink::Manifest> manifest,
    std::unique_ptr<WebApplicationInfo> web_app_info,
    std::vector<GURL> icon_urls,
    ForInstallableSite for_installable_site,
    bool skip_page_favicons) {
#if defined(OS_CHROMEOS)
  // Background installations are not a user-triggered installs, and thus cannot
  // be sent to the store.
  if (base::FeatureList::IsEnabled(features::kApkWebAppInstalls) &&
      for_installable_site == ForInstallableSite::kYes &&
      !background_installation_ && manifest) {
    for (const auto& application : manifest->related_applications) {
      std::string id = base::UTF16ToUTF8(application.id.string());
      if (!base::EqualsASCII(application.platform.string(),
                             kChromeOsPlayPlatform)) {
        continue;
      }

      std::string id_from_app_url =
          ExtractQueryValueForName(application.url, "id");

      if (id.empty()) {
        if (id_from_app_url.empty())
          continue;
        id = id_from_app_url;
      }

      auto* arc_service_manager = arc::ArcServiceManager::Get();
      if (arc_service_manager) {
        auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
            arc_service_manager->arc_bridge_service()->app(), IsInstallable);
        if (instance) {
          // Attach the referrer value.
          std::string referrer =
              ExtractQueryValueForName(application.url, "referrer");
          if (!referrer.empty())
            referrer = "&referrer=" + referrer;

          std::string intent = kPlayIntentPrefix + id + referrer;
          instance->IsInstallable(
              id,
              base::BindOnce(&WebAppInstallTask::OnDidCheckForIntentToPlayStore,
                             weak_ptr_factory_.GetWeakPtr(),
                             std::move(web_app_info), std::move(icon_urls),
                             for_installable_site, skip_page_favicons, intent));
          return;
        }
      }
    }
  }

#endif  // defined(OS_CHROMEOS)
  OnDidCheckForIntentToPlayStore(std::move(web_app_info), std::move(icon_urls),
                                 for_installable_site, skip_page_favicons,
                                 /*intent=*/"",
                                 /*should_intent_to_store=*/false);
}

void WebAppInstallTask::OnDidCheckForIntentToPlayStore(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    std::vector<GURL> icon_urls,
    ForInstallableSite for_installable_site,
    bool skip_page_favicons,
    const std::string& intent,
    bool should_intent_to_store) {
#if defined(OS_CHROMEOS)
  if (should_intent_to_store && !intent.empty()) {
    auto* arc_service_manager = arc::ArcServiceManager::Get();
    if (arc_service_manager) {
      auto* instance = ARC_GET_INSTANCE_FOR_METHOD(
          arc_service_manager->arc_bridge_service()->intent_helper(),
          HandleUrl);
      if (instance) {
        instance->HandleUrl(intent, kPlayStorePackage);
        CallInstallCallback(AppId(), InstallResultCode::kIntentToPlayStore);
        return;
      }
    }
  }
#endif  // defined(OS_CHROMEOS)

  data_retriever_->GetIcons(
      web_contents(), icon_urls, skip_page_favicons,
      install_source_ == WebappInstallSource::SYNC
          ? WebAppIconDownloader::Histogram::kForSync
          : WebAppIconDownloader::Histogram::kForCreate,
      base::BindOnce(&WebAppInstallTask::OnIconsRetrievedShowDialog,
                     base::Unretained(this), std::move(web_app_info),
                     for_installable_site));
}

void WebAppInstallTask::InstallWebAppFromInfoRetrieveIcons(
    content::WebContents* web_contents,
    std::unique_ptr<WebApplicationInfo> web_application_info,
    InstallFinalizer::FinalizeOptions finalize_options,
    InstallManager::OnceInstallCallback callback) {
  CheckInstallPreconditions();

  Observe(web_contents);
  install_callback_ = std::move(callback);
  install_source_ = finalize_options.install_source;
  background_installation_ = true;

  std::vector<GURL> icon_urls =
      GetValidIconUrlsToDownload(*web_application_info);

  // Skip downloading the page favicons as everything in is the URL list.
  data_retriever_->GetIcons(
      web_contents, icon_urls, /*skip_page_fav_icons=*/true,
      install_source_ == WebappInstallSource::SYNC
          ? WebAppIconDownloader::Histogram::kForSync
          : WebAppIconDownloader::Histogram::kForCreate,
      base::BindOnce(&WebAppInstallTask::OnIconsRetrieved,
                     base::Unretained(this), std::move(web_application_info),
                     finalize_options));
}

void WebAppInstallTask::OnIconsRetrieved(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    InstallFinalizer::FinalizeOptions finalize_options,
    IconsMap icons_map) {
  DCHECK(background_installation_);

  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  // Installing from sync should not change icon links.
  FilterAndResizeIconsGenerateMissing(web_app_info.get(), &icons_map);

  install_finalizer_->FinalizeInstall(
      *web_app_info, finalize_options,
      base::BindOnce(&WebAppInstallTask::OnInstallFinalized,
                     weak_ptr_factory_.GetWeakPtr()));
}

void WebAppInstallTask::OnIconsRetrievedShowDialog(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    ForInstallableSite for_installable_site,
    IconsMap icons_map) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  // The old BookmarkApp Sync System uses |WebAppInstallTask::OnIconsRetrieved|.
  // The new WebApp USS System has no sync wars and it doesn't need to preserve
  // icons. |is_for_sync| is always false for USS.
  FilterAndResizeIconsGenerateMissing(web_app_info.get(), &icons_map);

  if (background_installation_) {
    DCHECK(!dialog_callback_);
    OnDialogCompleted(for_installable_site, /*user_accepted=*/true,
                      std::move(web_app_info));
  } else {
    DCHECK(dialog_callback_);
    std::move(dialog_callback_)
        .Run(web_contents(), std::move(web_app_info), for_installable_site,
             base::BindOnce(&WebAppInstallTask::OnDialogCompleted,
                            weak_ptr_factory_.GetWeakPtr(),
                            for_installable_site));
  }
}

void WebAppInstallTask::OnIconsRetrievedFinalizeUpdate(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    IconsMap icons_map) {
  if (ShouldStopInstall())
    return;

  DCHECK(web_app_info);

  // TODO(crbug.com/926083): Abort update if icons fail to download.
  FilterAndResizeIconsGenerateMissing(web_app_info.get(), &icons_map);

  install_finalizer_->FinalizeUpdate(
      *web_app_info, base::BindOnce(&WebAppInstallTask::OnInstallFinalized,
                                    weak_ptr_factory_.GetWeakPtr()));
}

void WebAppInstallTask::OnDialogCompleted(
    ForInstallableSite for_installable_site,
    bool user_accepted,
    std::unique_ptr<WebApplicationInfo> web_app_info) {
  if (ShouldStopInstall())
    return;

  if (!user_accepted) {
    CallInstallCallback(AppId(), InstallResultCode::kUserInstallDeclined);
    return;
  }

  if (only_retrieve_web_application_info_) {
    web_application_info_ = std::move(web_app_info);
    CallInstallCallback(AppId(), InstallResultCode::kSuccessNewInstall);
    return;
  }

  WebApplicationInfo web_app_info_copy = *web_app_info;

  // This metric is recorded regardless of the installation result.
  RecordInstallEvent(for_installable_site);

  InstallFinalizer::FinalizeOptions finalize_options;
  finalize_options.install_source = install_source_;
  finalize_options.locally_installed = true;
  if (install_params_) {
    finalize_options.locally_installed = install_params_->locally_installed;

    if (IsChromeOs()) {
      finalize_options.chromeos_data.emplace();
      finalize_options.chromeos_data->show_in_launcher =
          install_params_->add_to_applications_menu;
      finalize_options.chromeos_data->show_in_search =
          install_params_->add_to_search;
      finalize_options.chromeos_data->show_in_management =
          install_params_->add_to_management;
      finalize_options.chromeos_data->is_disabled =
          install_params_->is_disabled;
    }

    if (install_params_->user_display_mode != DisplayMode::kUndefined) {
      web_app_info_copy.open_as_window =
          install_params_->user_display_mode != DisplayMode::kBrowser;
    }
  }

  install_finalizer_->FinalizeInstall(
      web_app_info_copy, finalize_options,
      base::BindOnce(&WebAppInstallTask::OnInstallFinalizedCreateShortcuts,
                     weak_ptr_factory_.GetWeakPtr(), std::move(web_app_info)));

  // Check that the finalizer hasn't called OnInstallFinalizedCreateShortcuts
  // synchronously:
  DCHECK(install_callback_);
}

void WebAppInstallTask::OnInstallFinalized(const AppId& app_id,
                                           InstallResultCode code) {
  if (ShouldStopInstall())
    return;

  CallInstallCallback(app_id, code);
}

void WebAppInstallTask::OnInstallFinalizedCreateShortcuts(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    const AppId& app_id,
    InstallResultCode code) {
  if (ShouldStopInstall())
    return;

  if (code != InstallResultCode::kSuccessNewInstall) {
    CallInstallCallback(app_id, code);
    return;
  }

  RecordAppBanner(web_contents(), web_app_info->app_url);
  RecordWebAppInstallationTimestamp(profile_->GetPrefs(), app_id,
                                    install_source_);

  bool add_to_applications_menu = true;
  bool add_to_desktop = true;

  if (install_params_) {
    add_to_applications_menu = install_params_->add_to_applications_menu;
    add_to_desktop = install_params_->add_to_desktop;
  }

  auto create_shortcuts_callback = base::BindOnce(
      &WebAppInstallTask::OnShortcutsCreated, weak_ptr_factory_.GetWeakPtr(),
      std::move(web_app_info), app_id);

  if (add_to_applications_menu && shortcut_manager_->CanCreateShortcuts()) {
    // TODO(ortuno): Make adding a shortcut to the applications menu independent
    // from adding a shortcut to desktop.
    shortcut_manager_->CreateShortcuts(app_id, add_to_desktop,
                                       std::move(create_shortcuts_callback));
  } else {
    std::move(create_shortcuts_callback).Run(false /* created_shortcuts */);
  }
}

void WebAppInstallTask::OnShortcutsCreated(
    std::unique_ptr<WebApplicationInfo> web_app_info,
    const AppId& app_id,
    bool shortcut_created) {
  if (ShouldStopInstall())
    return;

  bool add_to_quick_launch_bar = kAddAppsToQuickLaunchBarByDefault;
  if (install_source_ == WebappInstallSource::SYNC)
    add_to_quick_launch_bar = false;

  if (install_params_)
    add_to_quick_launch_bar = install_params_->add_to_quick_launch_bar;

  if (add_to_quick_launch_bar &&
      install_finalizer_->CanAddAppToQuickLaunchBar()) {
    install_finalizer_->AddAppToQuickLaunchBar(app_id);
  }

  if (!background_installation_) {
    const bool can_reparent_tab =
        install_finalizer_->CanReparentTab(app_id, shortcut_created);

    if (can_reparent_tab && web_app_info->open_as_window)
      install_finalizer_->ReparentTab(app_id, shortcut_created, web_contents());
  }

  // Enable file handlers, if the app is locally installed.
  if (registrar_->IsLocallyInstalled(app_id))
    file_handler_manager_->EnableAndRegisterOsFileHandlers(app_id);

  // TODO(https://crbug.com/1069298) - Also create App Icon Shortcuts Menu when
  // synced apps are locally installed from the chrome://apps page.
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenu) &&
      !web_app_info->shortcut_infos.empty()) {
    shortcut_manager_->RegisterShortcutsMenuWithOs(web_app_info->shortcut_infos,
                                                   app_id);
  }
  CallInstallCallback(app_id, InstallResultCode::kSuccessNewInstall);
}

}  // namespace web_app
