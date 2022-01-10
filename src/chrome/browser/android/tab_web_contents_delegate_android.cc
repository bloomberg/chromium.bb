// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/tab_web_contents_delegate_android.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "chrome/android/chrome_jni_headers/TabWebContentsDelegateAndroidImpl_jni.h"
#include "chrome/browser/android/customtabs/client_data_header_web_contents_observer.h"
#include "chrome/browser/android/hung_renderer_infobar_delegate.h"
#include "chrome/browser/android/tab_android.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/content_settings/sound_content_setting_observer.h"
#include "chrome/browser/data_reduction_proxy/data_reduction_proxy_tab_helper.h"
#include "chrome/browser/file_select_helper.h"
#include "chrome/browser/flags/android/cached_feature_flags.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/history/history_tab_helper.h"
#include "chrome/browser/installable/installed_webapp_bridge.h"
#include "chrome/browser/installable/installed_webapp_geolocation_context.h"
#include "chrome/browser/media/protected_media_identifier_permission_context.h"
#include "chrome/browser/media/webrtc/media_capture_devices_dispatcher.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/prefetch/no_state_prefetch/no_state_prefetch_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/safe_browsing_navigation_observer_manager_factory.h"
#include "chrome/browser/safe_browsing/safe_browsing_service.h"
#include "chrome/browser/ui/android/infobars/framebust_block_infobar.h"
#include "chrome/browser/ui/android/tab_model/tab_model_list.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/browser/ui/blocked_content/chrome_popup_navigation_delegate.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/interventions/framebust_block_message_delegate.h"
#include "chrome/browser/ui/prefs/prefs_tab_helper.h"
#include "chrome/browser/ui/tab_helpers.h"
#include "chrome/browser/vr/vr_tab_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/blocked_content/popup_blocker.h"
#include "components/blocked_content/popup_tracker.h"
#include "components/browser_ui/sms/android/sms_infobar.h"
#include "components/browser_ui/util/android/url_constants.h"
#include "components/find_in_page/find_notification_details.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/javascript_dialogs/app_modal_dialog_manager.h"
#include "components/javascript_dialogs/tab_modal_dialog_manager.h"
#include "components/navigation_interception/intercept_navigation_delegate.h"
#include "components/no_state_prefetch/browser/no_state_prefetch_manager.h"
#include "components/paint_preview/buildflags/buildflags.h"
#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer.h"
#include "content/public/browser/file_select_listener.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/mediastream/media_stream_request.h"
#include "third_party/blink/public/mojom/frame/blocked_navigation_types.mojom.h"
#include "third_party/blink/public/mojom/window_features/window_features.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/origin.h"

#if BUILDFLAG(ENABLE_PRINTING)
#include "components/printing/browser/print_composite_client.h"
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
#include "components/paint_preview/browser/paint_preview_client.h"
#endif

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using blink::mojom::FileChooserParams;
using content::WebContents;

namespace {

ScopedJavaLocalRef<jobject>
JNI_TabWebContentsDelegateAndroidImpl_CreateJavaRectF(JNIEnv* env,
                                                      const gfx::RectF& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_TabWebContentsDelegateAndroidImpl_createRectF(
          env, rect.x(), rect.y(), rect.right(), rect.bottom()));
}

ScopedJavaLocalRef<jobject>
JNI_TabWebContentsDelegateAndroidImpl_CreateJavaRect(JNIEnv* env,
                                                     const gfx::Rect& rect) {
  return ScopedJavaLocalRef<jobject>(
      Java_TabWebContentsDelegateAndroidImpl_createRect(
          env, static_cast<int>(rect.x()), static_cast<int>(rect.y()),
          static_cast<int>(rect.right()), static_cast<int>(rect.bottom())));
}

infobars::InfoBar* FindHungRendererInfoBar(
    infobars::ContentInfoBarManager* infobar_manager) {
  DCHECK(infobar_manager);
  for (size_t i = 0; i < infobar_manager->infobar_count(); ++i) {
    infobars::InfoBar* infobar = infobar_manager->infobar_at(i);
    if (infobar->delegate()->AsHungRendererInfoBarDelegate())
      return infobar;
  }
  return nullptr;
}

void ShowFramebustBlockInfobarInternal(content::WebContents* web_contents,
                                       const GURL& url) {
  auto intervention_outcome =
      [](FramebustBlockMessageDelegate::InterventionOutcome outcome) {
        UMA_HISTOGRAM_ENUMERATION("WebCore.Framebust.InterventionOutcome",
                                  outcome);
      };
  FramebustBlockInfoBar::Show(
      web_contents,
      std::make_unique<FramebustBlockMessageDelegate>(
          web_contents, url, base::BindOnce(intervention_outcome)));
}

}  // anonymous namespace

namespace android {

TabWebContentsDelegateAndroid::TabWebContentsDelegateAndroid(JNIEnv* env,
                                                                   jobject obj)
    : WebContentsDelegateAndroid(env, obj) {
}

TabWebContentsDelegateAndroid::~TabWebContentsDelegateAndroid() = default;

void TabWebContentsDelegateAndroid::PortalWebContentsCreated(
    content::WebContents* portal_contents) {
  WebContentsDelegateAndroid::PortalWebContentsCreated(portal_contents);

  Profile* profile =
      Profile::FromBrowserContext(portal_contents->GetBrowserContext());

  // This is a subset of the tab helpers that would be attached by
  // TabAndroid::AttachTabHelpers.
  //
  // TODO(jbroman): This doesn't adequately handle all of the tab helpers that
  // might be wanted here, and there are also likely to be issues with tab
  // helpers that are unprepared for portal activation to transition them.
  // See https://crbug.com/1042323
  autofill::ChromeAutofillClient::CreateForWebContents(portal_contents);
  autofill::ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
      portal_contents,
      autofill::ChromeAutofillClient::FromWebContents(portal_contents),
      g_browser_process->GetApplicationLocale(),
      autofill::BrowserAutofillManager::ENABLE_AUTOFILL_DOWNLOAD_MANAGER);
  ChromePasswordManagerClient::CreateForWebContentsWithAutofillClient(
      portal_contents,
      autofill::ChromeAutofillClient::FromWebContents(portal_contents));
  HistoryTabHelper::CreateForWebContents(portal_contents);
  infobars::ContentInfoBarManager::CreateForWebContents(portal_contents);
  PrefsTabHelper::CreateForWebContents(portal_contents);
  DataReductionProxyTabHelper::CreateForWebContents(portal_contents);
  safe_browsing::SafeBrowsingNavigationObserver::MaybeCreateForWebContents(
      portal_contents, HostContentSettingsMapFactory::GetForProfile(profile),
      safe_browsing::SafeBrowsingNavigationObserverManagerFactory::
          GetForBrowserContext(profile),
      profile->GetPrefs(), g_browser_process->safe_browsing_service());
}

void TabWebContentsDelegateAndroid::RunFileChooser(
    content::RenderFrameHost* render_frame_host,
    scoped_refptr<content::FileSelectListener> listener,
    const FileChooserParams& params) {
  if (vr::VrTabHelper::IsUiSuppressedInVr(
          WebContents::FromRenderFrameHost(render_frame_host),
          vr::UiSuppressedElement::kFileChooser)) {
    listener->FileSelectionCanceled();
    return;
  }
  FileSelectHelper::RunFileChooser(render_frame_host, std::move(listener),
                                   params);
}

void TabWebContentsDelegateAndroid::CreateSmsPrompt(
    content::RenderFrameHost* host,
    const std::vector<url::Origin>& origin_list,
    const std::string& one_time_code,
    base::OnceClosure on_confirm,
    base::OnceClosure on_cancel) {
  DCHECK_EQ(host->GetLifecycleState(),
            content::RenderFrameHost::LifecycleState::kActive);

  auto* web_contents = content::WebContents::FromRenderFrameHost(host);
  sms::SmsInfoBar::Create(
      web_contents,
      infobars::ContentInfoBarManager::FromWebContents(web_contents),
      origin_list, one_time_code, std::move(on_confirm), std::move(on_cancel));
}

bool TabWebContentsDelegateAndroid::ShouldFocusLocationBarByDefault(
    WebContents* source) {
  content::NavigationEntry* entry = source->GetController().GetActiveEntry();
  if (entry) {
    GURL url = entry->GetURL();
    GURL virtual_url = entry->GetVirtualURL();
    if ((url.SchemeIs(browser_ui::kChromeUINativeScheme) &&
         url.host_piece() == chrome::kChromeUINewTabHost) ||
        (virtual_url.SchemeIs(browser_ui::kChromeUINativeScheme) &&
         virtual_url.host_piece() == chrome::kChromeUINewTabHost)) {
      return true;
    }
  }
  return false;
}

void TabWebContentsDelegateAndroid::FindReply(
    WebContents* web_contents,
    int request_id,
    int number_of_matches,
    const gfx::Rect& selection_rect,
    int active_match_ordinal,
    bool final_update) {
  find_in_page::FindTabHelper* find_tab_helper =
      find_in_page::FindTabHelper::FromWebContents(web_contents);
  if (!find_result_observations_.IsObservingSource(find_tab_helper))
    find_result_observations_.AddObservation(find_tab_helper);

  find_tab_helper->HandleFindReply(request_id,
                                   number_of_matches,
                                   selection_rect,
                                   active_match_ordinal,
                                   final_update);
}

void TabWebContentsDelegateAndroid::FindMatchRectsReply(
    WebContents* web_contents,
    int version,
    const std::vector<gfx::RectF>& rects,
    const gfx::RectF& active_rect) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  // Create the details object.
  ScopedJavaLocalRef<jobject> details_object =
      Java_TabWebContentsDelegateAndroidImpl_createFindMatchRectsDetails(
          env, version, rects.size(),
          JNI_TabWebContentsDelegateAndroidImpl_CreateJavaRectF(env,
                                                                active_rect));

  // Add the rects
  for (size_t i = 0; i < rects.size(); ++i) {
    Java_TabWebContentsDelegateAndroidImpl_setMatchRectByIndex(
        env, details_object, i,
        JNI_TabWebContentsDelegateAndroidImpl_CreateJavaRectF(env, rects[i]));
  }

  Java_TabWebContentsDelegateAndroidImpl_onFindMatchRectsAvailable(
      env, obj, details_object);
}

content::JavaScriptDialogManager*
TabWebContentsDelegateAndroid::GetJavaScriptDialogManager(
    WebContents* source) {
  // For VR, we use app modal since the dialog view will cover the location bar.
  if (!vr::VrTabHelper::IsInVr(source)) {
    return javascript_dialogs::TabModalDialogManager::FromWebContents(source);
  }
  return javascript_dialogs::AppModalDialogManager::GetInstance();
}

void TabWebContentsDelegateAndroid::AdjustPreviewsStateForNavigation(
    content::WebContents* web_contents,
    blink::PreviewsState* previews_state) {
  if (GetDisplayMode(web_contents) != blink::mojom::DisplayMode::kBrowser) {
    *previews_state = blink::PreviewsTypes::PREVIEWS_OFF;
  }
}

void TabWebContentsDelegateAndroid::RequestMediaAccessPermission(
    content::WebContents* web_contents,
    const content::MediaStreamRequest& request,
    content::MediaResponseCallback callback) {
  MediaCaptureDevicesDispatcher::GetInstance()->ProcessMediaAccessRequest(
      web_contents, request, std::move(callback), nullptr);
}

bool TabWebContentsDelegateAndroid::CheckMediaAccessPermission(
    content::RenderFrameHost* render_frame_host,
    const GURL& security_origin,
    blink::mojom::MediaStreamType type) {
  return MediaCaptureDevicesDispatcher::GetInstance()
      ->CheckMediaAccessPermission(render_frame_host, security_origin, type);
}

void TabWebContentsDelegateAndroid::SetOverlayMode(bool use_overlay_mode) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  Java_TabWebContentsDelegateAndroidImpl_setOverlayMode(env, obj,
                                                        use_overlay_mode);
}

WebContents* TabWebContentsDelegateAndroid::OpenURLFromTab(
    WebContents* source,
    const content::OpenURLParams& params) {
  WindowOpenDisposition disposition = params.disposition;
  if (!source || (disposition != WindowOpenDisposition::CURRENT_TAB &&
                  disposition != WindowOpenDisposition::NEW_FOREGROUND_TAB &&
                  disposition != WindowOpenDisposition::NEW_BACKGROUND_TAB &&
                  disposition != WindowOpenDisposition::OFF_THE_RECORD &&
                  disposition != WindowOpenDisposition::NEW_POPUP &&
                  disposition != WindowOpenDisposition::NEW_WINDOW)) {
    // We can't handle this here.  Give the parent a chance.
    return WebContentsDelegateAndroid::OpenURLFromTab(source, params);
  }

  Profile* profile = Profile::FromBrowserContext(source->GetBrowserContext());
  NavigateParams nav_params(profile, params.url, params.transition);
  nav_params.FillNavigateParamsFromOpenURLParams(params);
  nav_params.source_contents = source;
  nav_params.window_action = NavigateParams::SHOW_WINDOW;
  auto popup_delegate =
      std::make_unique<ChromePopupNavigationDelegate>(std::move(nav_params));
  if (blocked_content::ConsiderForPopupBlocking(params.disposition)) {
    popup_delegate.reset(static_cast<ChromePopupNavigationDelegate*>(
        blocked_content::MaybeBlockPopup(
            source, nullptr, std::move(popup_delegate), &params,
            blink::mojom::WindowFeatures(),
            HostContentSettingsMapFactory::GetForProfile(
                source->GetBrowserContext()))
            .release()));
    if (!popup_delegate)
      return nullptr;
  }

  if (disposition == WindowOpenDisposition::CURRENT_TAB) {
    // Ask the parent to handle in-place opening.
    return WebContentsDelegateAndroid::OpenURLFromTab(source, params);
  }

  popup_delegate->nav_params()->opened_by_another_window = true;
  TabModelList::HandlePopupNavigation(popup_delegate->nav_params());
  return nullptr;
}

bool TabWebContentsDelegateAndroid::ShouldResumeRequestsForCreatedWindow() {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return true;

  return Java_TabWebContentsDelegateAndroidImpl_shouldResumeRequestsForCreatedWindow(
      env, obj);
}

void TabWebContentsDelegateAndroid::AddNewContents(
    WebContents* source,
    std::unique_ptr<WebContents> new_contents,
    const GURL& target_url,
    WindowOpenDisposition disposition,
    const gfx::Rect& initial_rect,
    bool user_gesture,
    bool* was_blocked) {
  // No code for this yet.
  DCHECK_NE(disposition, WindowOpenDisposition::SAVE_TO_DISK);
  // Can't create a new contents for the current tab - invalid case.
  DCHECK_NE(disposition, WindowOpenDisposition::CURRENT_TAB);

  // At this point the |new_contents| is beyond the popup blocker, but we use
  // the same logic for determining if the popup tracker needs to be attached.
  if (source && blocked_content::ConsiderForPopupBlocking(disposition)) {
    blocked_content::PopupTracker::CreateForWebContents(new_contents.get(),
                                                        source, disposition);
  }

  // Add the CCT header observer if it was present on the source contents.
  if (source) {
    auto* source_observer =
        customtabs::ClientDataHeaderWebContentsObserver::FromWebContents(
            source);
    if (source_observer) {
      customtabs::ClientDataHeaderWebContentsObserver::CreateForWebContents(
          new_contents.get());
      customtabs::ClientDataHeaderWebContentsObserver::FromWebContents(
          new_contents.get())
          ->SetHeader(source_observer->header());
    }
  }

  TabHelpers::AttachTabHelpers(new_contents.get());

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  bool handled = false;
  if (!obj.is_null()) {
    ScopedJavaLocalRef<jobject> jsource;
    if (source)
      jsource = source->GetJavaWebContents();
    ScopedJavaLocalRef<jobject> jnew_contents;
    if (new_contents)
      jnew_contents = new_contents->GetJavaWebContents();

    handled = Java_TabWebContentsDelegateAndroidImpl_addNewContents(
        env, obj, jsource, jnew_contents, static_cast<jint>(disposition),
        nullptr, user_gesture);
  }

  if (was_blocked)
    *was_blocked = !handled;

  // When handled is |true|, ownership has been passed to java, which in turn
  // creates a new TabAndroid instance to own the WebContents.
  if (handled)
    new_contents.release();
}

void TabWebContentsDelegateAndroid::OnDidBlockNavigation(
    content::WebContents* web_contents,
    const GURL& blocked_url,
    const GURL& initiator_url,
    blink::mojom::NavigationBlockedReason reason) {
  ShowFramebustBlockInfobarInternal(web_contents, blocked_url);
}

void TabWebContentsDelegateAndroid::UpdateUserGestureCarryoverInfo(
    content::WebContents* web_contents) {
  auto* intercept_navigation_delegate =
      navigation_interception::InterceptNavigationDelegate::Get(web_contents);
  if (intercept_navigation_delegate)
    intercept_navigation_delegate->UpdateLastUserGestureCarryoverTimestamp();
}

content::PictureInPictureResult
TabWebContentsDelegateAndroid::EnterPictureInPicture(
    content::WebContents* web_contents,
    const viz::SurfaceId& surface_id,
    const gfx::Size& natural_size) {
  return PictureInPictureWindowManager::GetInstance()->EnterPictureInPicture(
      web_contents, surface_id, natural_size);
}

void TabWebContentsDelegateAndroid::ExitPictureInPicture() {
  PictureInPictureWindowManager::GetInstance()->ExitPictureInPicture();
}

bool TabWebContentsDelegateAndroid::IsBackForwardCacheSupported() {
  return true;
}

bool TabWebContentsDelegateAndroid::IsPrerender2Supported() {
  return true;
}

std::unique_ptr<content::WebContents>
TabWebContentsDelegateAndroid::ActivatePortalWebContents(
    content::WebContents* predecessor_contents,
    std::unique_ptr<content::WebContents> portal_contents) {
  const bool is_loading = portal_contents->IsLoading();
  return TabAndroid::FromWebContents(predecessor_contents)
      ->SwapWebContents(std::move(portal_contents),
                        /* did_start_load */ true,
                        /* did_finish_load */ !is_loading);
}

device::mojom::GeolocationContext*
TabWebContentsDelegateAndroid::GetInstalledWebappGeolocationContext() {
  if (!IsInstalledWebappDelegateGeolocation())
    return nullptr;

  if (!installed_webapp_geolocation_context_) {
    installed_webapp_geolocation_context_ =
        std::make_unique<InstalledWebappGeolocationContext>();
  }
  return installed_webapp_geolocation_context_.get();
}

#if BUILDFLAG(ENABLE_PRINTING)
void TabWebContentsDelegateAndroid::PrintCrossProcessSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    int document_cookie,
    content::RenderFrameHost* subframe_host) const {
  auto* client = printing::PrintCompositeClient::FromWebContents(web_contents);
  if (client)
    client->PrintCrossProcessSubframe(rect, document_cookie, subframe_host);
}
#endif

#if BUILDFLAG(ENABLE_PAINT_PREVIEW)
void TabWebContentsDelegateAndroid::CapturePaintPreviewOfSubframe(
    content::WebContents* web_contents,
    const gfx::Rect& rect,
    const base::UnguessableToken& guid,
    content::RenderFrameHost* render_frame_host) {
  auto* client =
      paint_preview::PaintPreviewClient::FromWebContents(web_contents);
  if (client)
    client->CaptureSubframePaintPreview(guid, rect, render_frame_host);
}
#endif

void TabWebContentsDelegateAndroid::OnFindResultAvailable(
    WebContents* web_contents) {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return;

  const find_in_page::FindNotificationDetails& find_result =
      find_in_page::FindTabHelper::FromWebContents(web_contents)->find_result();

  ScopedJavaLocalRef<jobject> selection_rect =
      JNI_TabWebContentsDelegateAndroidImpl_CreateJavaRect(
          env, find_result.selection_rect());

  // Create the details object.
  ScopedJavaLocalRef<jobject> details_object =
      Java_TabWebContentsDelegateAndroidImpl_createFindNotificationDetails(
          env, find_result.number_of_matches(), selection_rect,
          find_result.active_match_ordinal(), find_result.final_update());

  Java_TabWebContentsDelegateAndroidImpl_onFindResultAvailable(env, obj,
                                                               details_object);
}

void TabWebContentsDelegateAndroid::OnFindTabHelperDestroyed(
    find_in_page::FindTabHelper* helper) {
  find_result_observations_.RemoveObservation(helper);
}

bool TabWebContentsDelegateAndroid::ShouldEnableEmbeddedMediaExperience()
    const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_TabWebContentsDelegateAndroidImpl_shouldEnableEmbeddedMediaExperience(
      env, obj);
}

bool TabWebContentsDelegateAndroid::IsPictureInPictureEnabled() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_TabWebContentsDelegateAndroidImpl_isPictureInPictureEnabled(env,
                                                                          obj);
}

bool TabWebContentsDelegateAndroid::IsNightModeEnabled() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_TabWebContentsDelegateAndroidImpl_isNightModeEnabled(env, obj);
}

bool TabWebContentsDelegateAndroid::IsForceDarkWebContentEnabled() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_TabWebContentsDelegateAndroidImpl_isForceDarkWebContentEnabled(
      env, obj);
}

bool TabWebContentsDelegateAndroid::CanShowAppBanners() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_TabWebContentsDelegateAndroidImpl_canShowAppBanners(env, obj);
}

bool TabWebContentsDelegateAndroid::IsTabLargeEnoughForDesktopSite() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_TabWebContentsDelegateAndroidImpl_isTabLargeEnoughForDesktopSite(
      env, obj);
}

const GURL TabWebContentsDelegateAndroid::GetManifestScope() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return GURL();
  const JavaRef<jstring>& scope =
      Java_TabWebContentsDelegateAndroidImpl_getManifestScope(env, obj);
  return scope.is_null() ? GURL()
                         : GURL(base::android::ConvertJavaStringToUTF8(scope));
}

bool TabWebContentsDelegateAndroid::IsCustomTab() const {
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_TabWebContentsDelegateAndroidImpl_isCustomTab(env, obj);
}

bool TabWebContentsDelegateAndroid::IsInstalledWebappDelegateGeolocation()
    const {
  if (!base::FeatureList::IsEnabled(
          chrome::android::kTrustedWebActivityLocationDelegation)) {
    return false;
  }

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> obj = GetJavaDelegate(env);
  if (obj.is_null())
    return false;
  return Java_TabWebContentsDelegateAndroidImpl_isInstalledWebappDelegateGeolocation(
      env, obj);
}

}  // namespace android

void JNI_TabWebContentsDelegateAndroidImpl_OnRendererUnresponsive(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  // Rate limit the number of stack dumps so we don't overwhelm our crash
  // reports.
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  if (base::RandDouble() < 0.01)
    web_contents->GetMainFrame()->GetProcess()->DumpProcessStack();

  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableHungRendererInfoBar)) {
    return;
  }

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  DCHECK(!FindHungRendererInfoBar(infobar_manager));
  HungRendererInfoBarDelegate::Create(
      infobar_manager, web_contents->GetMainFrame()->GetProcess());
}

void JNI_TabWebContentsDelegateAndroidImpl_OnRendererResponsive(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents) {
  content::WebContents* web_contents =
          content::WebContents::FromJavaWebContents(java_web_contents);
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobars::InfoBar* hung_renderer_infobar =
      FindHungRendererInfoBar(infobar_manager);
  if (!hung_renderer_infobar)
    return;

  hung_renderer_infobar->delegate()
      ->AsHungRendererInfoBarDelegate()
      ->OnRendererResponsive();
  infobar_manager->RemoveInfoBar(hung_renderer_infobar);
}

void JNI_TabWebContentsDelegateAndroidImpl_ShowFramebustBlockInfoBar(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_web_contents,
    const JavaParamRef<jstring>& java_url) {
  GURL url(base::android::ConvertJavaStringToUTF16(env, java_url));
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(java_web_contents);
  ShowFramebustBlockInfobarInternal(web_contents, url);
}
