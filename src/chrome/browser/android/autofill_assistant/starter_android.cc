// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/starter_android.h"

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/time/default_tick_clock.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AssistantDependenciesImpl_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers_public/Starter_jni.h"
#include "chrome/browser/android/autofill_assistant/client_android.h"
#include "chrome/browser/android/autofill_assistant/trigger_script_bridge_android.h"
#include "chrome/browser/android/autofill_assistant/ui_controller_android_utils.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/public/runtime_manager_impl.h"
#include "components/autofill_assistant/browser/script_parameters.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"
#include "components/version_info/channel.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "url/gurl.h"

using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;

namespace autofill_assistant {

static jlong JNI_Starter_FromWebContents(
    JNIEnv* env,
    const JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  CHECK(web_contents);
  StarterAndroid::CreateForWebContents(web_contents);
  auto* tab_helper_android = StarterAndroid::FromWebContents(web_contents);
  return reinterpret_cast<intptr_t>(tab_helper_android);
}

StarterAndroid::StarterAndroid(content::WebContents* web_contents)
    : web_contents_(web_contents),
      website_login_manager_(std::make_unique<WebsiteLoginManagerImpl>(
          ChromePasswordManagerClient::FromWebContents(web_contents),
          web_contents)) {}

StarterAndroid::~StarterAndroid() = default;

void StarterAndroid::Attach(JNIEnv* env, const JavaParamRef<jobject>& jcaller) {
  Detach(env, jcaller);
  java_object_ = base::android::ScopedJavaGlobalRef<jobject>(jcaller);

  starter_ = std::make_unique<Starter>(
      web_contents_, this, ukm::UkmRecorder::Get(),
      RuntimeManagerImpl::GetForWebContents(web_contents_)->GetWeakPtr(),
      base::DefaultTickClock::GetInstance());
}

void StarterAndroid::Detach(JNIEnv* env, const JavaParamRef<jobject>& jcaller) {
  java_object_ = nullptr;
  java_dependencies_ = nullptr;
  starter_.reset();
}

std::unique_ptr<TriggerScriptCoordinator::UiDelegate>
StarterAndroid::CreateTriggerScriptUiDelegate() {
  CreateJavaDependenciesIfNecessary();
  return std::make_unique<TriggerScriptBridgeAndroid>(
      base::android::AttachCurrentThread(), java_dependencies_);
}

std::unique_ptr<ServiceRequestSender>
StarterAndroid::GetTriggerScriptRequestSenderToInject() {
  DCHECK(GetFeatureModuleInstalled());
  return ui_controller_android_utils::GetServiceRequestSenderToInject(
      base::android::AttachCurrentThread());
}

WebsiteLoginManager* StarterAndroid::GetWebsiteLoginManager() const {
  return website_login_manager_.get();
}

version_info::Channel StarterAndroid::GetChannel() const {
  return chrome::GetChannel();
}

bool StarterAndroid::GetFeatureModuleInstalled() const {
  return Java_Starter_getFeatureModuleInstalled(
      base::android::AttachCurrentThread());
}

void StarterAndroid::InstallFeatureModule(
    bool show_ui,
    base::OnceCallback<void(Metrics::FeatureModuleInstallation result)>
        callback) {
  DCHECK(java_object_);
  feature_module_installation_finished_callback_ = std::move(callback);
  Java_Starter_installFeatureModule(base::android::AttachCurrentThread(),
                                    java_object_, show_ui);
}

void StarterAndroid::OnFeatureModuleInstalled(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jint result) {
  DCHECK(feature_module_installation_finished_callback_);
  std::move(feature_module_installation_finished_callback_)
      .Run(static_cast<Metrics::FeatureModuleInstallation>(result));
}

void StarterAndroid::OnInteractabilityChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean is_interactable) {
  if (!starter_) {
    return;
  }

  starter_->OnTabInteractabilityChanged(is_interactable);
}

void StarterAndroid::OnActivityAttachmentChanged(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  java_dependencies_ = nullptr;
  if (!starter_) {
    return;
  }

  // Notify the starter. Some flows are only available in CCT or in regular tab,
  // so we need to cancel ongoing flows if they are no longer supported.
  starter_->CheckSettings();
}

bool StarterAndroid::GetIsFirstTimeUser() const {
  return Java_Starter_getIsFirstTimeUser(base::android::AttachCurrentThread());
}

void StarterAndroid::SetIsFirstTimeUser(bool first_time_user) {
  Java_Starter_setIsFirstTimeUser(base::android::AttachCurrentThread(),
                                  first_time_user);
}

bool StarterAndroid::GetOnboardingAccepted() const {
  return Java_Starter_getOnboardingAccepted(
      base::android::AttachCurrentThread());
}

void StarterAndroid::SetOnboardingAccepted(bool accepted) {
  Java_Starter_setOnboardingAccepted(base::android::AttachCurrentThread(),
                                     accepted);
}

void StarterAndroid::ShowOnboarding(
    bool use_dialog_onboarding,
    const TriggerContext& trigger_context,
    base::OnceCallback<void(bool shown, OnboardingResult result)> callback) {
  CreateJavaDependenciesIfNecessary();
  if (onboarding_finished_callback_) {
    DCHECK(false) << "onboarding requested while already being shown";
    std::move(callback).Run(false, OnboardingResult::DISMISSED);
    return;
  }
  onboarding_finished_callback_ = std::move(callback);

  std::vector<std::string> keys;
  std::vector<std::string> values;
  for (const auto& param : trigger_context.GetScriptParameters().ToProto()) {
    keys.emplace_back(param.name());
    values.emplace_back(param.value());
  }
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Starter_showOnboarding(env, java_object_, java_dependencies_,
                              use_dialog_onboarding,
                              base::android::ConvertUTF8ToJavaString(
                                  env, trigger_context.GetExperimentIds()),
                              base::android::ToJavaArrayOfStrings(env, keys),
                              base::android::ToJavaArrayOfStrings(env, values));
}

void StarterAndroid::HideOnboarding() {
  if (!java_dependencies_) {
    return;
  }
  Java_Starter_hideOnboarding(base::android::AttachCurrentThread(),
                              java_object_, java_dependencies_);
}

void StarterAndroid::OnOnboardingFinished(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    jboolean shown,
    jint result) {
  // Currently, java may end up attempting to notify the native starter more
  // than once. The first notification is the user-triggered one, the others are
  // due to secondary effects, e.g., due to the dialog being hidden.
  // TODO(arbesser): fix this such that the callback is only called once.
  if (!onboarding_finished_callback_) {
    return;
  }
  std::move(onboarding_finished_callback_)
      .Run(shown, static_cast<OnboardingResult>(result));
}

bool StarterAndroid::GetProactiveHelpSettingEnabled() const {
  return Java_Starter_getProactiveHelpSettingEnabled(
      base::android::AttachCurrentThread());
}

void StarterAndroid::SetProactiveHelpSettingEnabled(bool enabled) {
  Java_Starter_setProactiveHelpSettingEnabled(
      base::android::AttachCurrentThread(), enabled);
}

bool StarterAndroid::GetMakeSearchesAndBrowsingBetterEnabled() const {
  return Java_Starter_getMakeSearchesAndBrowsingBetterSettingEnabled(
      base::android::AttachCurrentThread());
}

bool StarterAndroid::GetIsCustomTab() const {
  return ui_controller_android_utils::IsCustomTab(web_contents_);
}

bool StarterAndroid::GetIsTabCreatedByGSA() const {
  if (!java_object_) {
    // Failsafe, should never happen.
    DCHECK(false);
    return false;
  }
  return Java_Starter_getIsTabCreatedByGSA(base::android::AttachCurrentThread(),
                                           java_object_);
}

void StarterAndroid::CreateJavaDependenciesIfNecessary() {
  if (java_dependencies_) {
    return;
  }

  java_dependencies_ = base::android::ScopedJavaGlobalRef<jobject>(
      Java_Starter_getOrCreateDependencies(base::android::AttachCurrentThread(),
                                           java_object_));
}

void StarterAndroid::Start(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaRef<jstring>& jexperiment_ids,
    const base::android::JavaRef<jobjectArray>& jparameter_names,
    const base::android::JavaRef<jobjectArray>& jparameter_values,
    const base::android::JavaRef<jobjectArray>& jdevice_only_parameter_names,
    const base::android::JavaRef<jobjectArray>& jdevice_only_parameter_values,
    const base::android::JavaRef<jstring>& jinitial_url) {
  DCHECK(starter_);
  auto trigger_context = ui_controller_android_utils::CreateTriggerContext(
      env, web_contents_, jexperiment_ids, jparameter_names, jparameter_values,
      jdevice_only_parameter_names, jdevice_only_parameter_values,
      /* onboarding_shown = */ false, /* is_direct_action = */ false,
      jinitial_url);

  starter_->Start(std::move(trigger_context));
}

void StarterAndroid::StartRegularScript(
    GURL url,
    std::unique_ptr<TriggerContext> trigger_context,
    const absl::optional<TriggerScriptProto>& trigger_script) {
  ClientAndroid::CreateForWebContents(web_contents_);
  auto* client_android = ClientAndroid::FromWebContents(web_contents_);
  DCHECK(client_android);

  JNIEnv* env = base::android::AttachCurrentThread();
  CreateJavaDependenciesIfNecessary();
  client_android->Start(
      url, std::move(trigger_context),
      ui_controller_android_utils::GetServiceToInject(env, client_android),
      Java_AssistantDependenciesImpl_transferOnboardingOverlayCoordinator(
          env, java_dependencies_),
      trigger_script);
}

bool StarterAndroid::IsRegularScriptRunning() const {
  auto* client_android = ClientAndroid::FromWebContents(web_contents_);
  if (!client_android) {
    return false;
  }
  return client_android->IsRunning();
}

bool StarterAndroid::IsRegularScriptVisible() const {
  auto* client_android = ClientAndroid::FromWebContents(web_contents_);
  if (!client_android) {
    return false;
  }
  return client_android->IsVisible();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(StarterAndroid);

}  // namespace autofill_assistant
