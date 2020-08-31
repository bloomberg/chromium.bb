// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/autofill_assistant/client_android.h"

#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/locale_utils.h"
#include "base/android/scoped_java_ref.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/metrics/field_trial_params.h"
#include "base/task/post_task.h"
#include "base/time/default_tick_clock.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AutofillAssistantClient_jni.h"
#include "chrome/android/features/autofill_assistant/jni_headers/AutofillAssistantDirectActionImpl_jni.h"
#include "chrome/browser/autofill/android/personal_data_manager_android.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/browser/password_manager/chrome_password_manager_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/common/channel_info.h"
#include "components/autofill_assistant/browser/access_token_fetcher.h"
#include "components/autofill_assistant/browser/controller.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/switches.h"
#include "components/autofill_assistant/browser/website_login_manager_impl.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/content/browser/content_password_manager_driver_factory.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/version_info/channel.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "url/gurl.h"

using base::android::AttachCurrentThread;
using base::android::JavaParamRef;
using base::android::JavaRef;

namespace autofill_assistant {
namespace {

// A direct action that corresponds to pressing the close or cancel button on
// the UI.
const char* const kCancelActionName = "cancel";

// Fills a map from two Java arrays of strings of the same length.
void FillStringMapFromJava(JNIEnv* env,
                           const JavaRef<jobjectArray>& names,
                           const JavaRef<jobjectArray>& values,
                           std::map<std::string, std::string>* parameters) {
  std::vector<std::string> names_vector;
  base::android::AppendJavaStringArrayToStringVector(env, names, &names_vector);
  std::vector<std::string> values_vector;
  base::android::AppendJavaStringArrayToStringVector(env, values,
                                                     &values_vector);
  DCHECK_EQ(names_vector.size(), values_vector.size());
  for (size_t i = 0; i < names_vector.size(); ++i) {
    parameters->insert(std::make_pair(names_vector[i], values_vector[i]));
  }
}

std::unique_ptr<TriggerContextImpl> CreateTriggerContext(
    JNIEnv* env,
    const base::android::JavaParamRef<jstring>& jexperiment_ids,
    const base::android::JavaParamRef<jobjectArray>& jparameter_names,
    const base::android::JavaParamRef<jobjectArray>& jparameter_values) {
  std::map<std::string, std::string> parameters;
  FillStringMapFromJava(env, jparameter_names, jparameter_values, &parameters);
  return std::make_unique<TriggerContextImpl>(
      std::move(parameters),
      base::android::ConvertJavaStringToUTF8(env, jexperiment_ids));
}

}  // namespace

static base::android::ScopedJavaLocalRef<jobject>
JNI_AutofillAssistantClient_FromWebContents(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jweb_contents) {
  auto* web_contents = content::WebContents::FromJavaWebContents(jweb_contents);
  ClientAndroid::CreateForWebContents(web_contents);
  return ClientAndroid::FromWebContents(web_contents)->GetJavaObject();
}

ClientAndroid::ClientAndroid(content::WebContents* web_contents)
    : web_contents_(web_contents),
      java_object_(Java_AutofillAssistantClient_create(
          AttachCurrentThread(),
          reinterpret_cast<intptr_t>(this))) {}

ClientAndroid::~ClientAndroid() {
  if (controller_ != nullptr && started_) {
    // In the case of an unexpected closing of the activity or tab, controller_
    // will not yet have been cleaned up (since that happens when a web
    // contents object gets destroyed).
    Metrics::RecordDropOut(Metrics::DropOutReason::CONTENT_DESTROYED);
  }
  Java_AutofillAssistantClient_clearNativePtr(AttachCurrentThread(),
                                              java_object_);
}

base::android::ScopedJavaLocalRef<jobject> ClientAndroid::GetJavaObject() {
  return base::android::ScopedJavaLocalRef<jobject>(java_object_);
}

bool ClientAndroid::Start(JNIEnv* env,
                          const JavaParamRef<jobject>& jcaller,
                          const JavaParamRef<jstring>& jinitial_url,
                          const JavaParamRef<jstring>& jexperiment_ids,
                          const JavaParamRef<jstring>& jcaller_account,
                          const JavaParamRef<jobjectArray>& jparameter_names,
                          const JavaParamRef<jobjectArray>& jparameter_values,
                          jboolean jis_cct,
                          const JavaParamRef<jobject>& jonboarding_coordinator,
                          jboolean jonboarding_shown,
                          jlong jservice) {
  // When Start() is called, AA_START should have been measured. From now on,
  // the client is responsible for keeping track of dropouts, so that for each
  // AA_START there's a corresponding dropout.
  started_ = true;

  std::unique_ptr<Service> service = nullptr;
  if (jservice) {
    service.reset(static_cast<Service*>(reinterpret_cast<void*>(jservice)));
  }
  CreateController(std::move(service));

  // If an overlay is already shown, then show the rest of the UI.
  if (jonboarding_coordinator) {
    AttachUI(jonboarding_coordinator);
  }

  GURL initial_url(base::android::ConvertJavaStringToUTF8(env, jinitial_url));
  auto trigger_context = CreateTriggerContext(
      env, jexperiment_ids, jparameter_names, jparameter_values);
  trigger_context->SetCCT(jis_cct);
  trigger_context->SetOnboardingShown(jonboarding_shown);
  if (jcaller_account) {
    trigger_context->SetCallerAccountHash(
        base::android::ConvertJavaStringToUTF8(env, jcaller_account));
  }

  if (VLOG_IS_ON(2)) {
    std::string experiment_ids =
        base::android::ConvertJavaStringToUTF8(env, jexperiment_ids);
    std::map<std::string, std::string> parameters;
    FillStringMapFromJava(env, jparameter_names, jparameter_values,
                          &parameters);

    DVLOG(2) << "Starting autofill assistant with parameters:";
    DVLOG(2) << "\tinitial_url: " << initial_url;
    DVLOG(2) << "\texperiment_ids: " << experiment_ids;
    DVLOG(2) << "\tparameters:";
    for (const auto& param : parameters) {
      DVLOG(2) << "\t\t" << param.first << ": " << param.second;
    }
  }
  return controller_->Start(initial_url, std::move(trigger_context));
}

void ClientAndroid::DestroyUI(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  DestroyUI();
}

void ClientAndroid::TransferUITo(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jobject>& jother_web_contents) {
  if (!ui_controller_android_)
    return;

  auto ui_ptr = std::move(ui_controller_android_);
  // From this point on, the UIController, in ui_ptr, is either transferred or
  // deleted.

  GetPasswordManagerClient()->GetPasswordManager()->SetAutofillAssistantMode(
      password_manager::AutofillAssistantMode::kNotRunning);

  if (!jother_web_contents)
    return;

  auto* other_web_contents =
      content::WebContents::FromJavaWebContents(jother_web_contents);
  DCHECK_NE(other_web_contents, web_contents_);

  ClientAndroid* other_client =
      ClientAndroid::FromWebContents(other_web_contents);
  if (!other_client || !other_client->NeedsUI())
    return;

  other_client->ui_controller_android_ = std::move(ui_ptr);
  other_client->AttachUI();
}

base::android::ScopedJavaLocalRef<jstring> ClientAndroid::GetPrimaryAccountName(
    JNIEnv* env,
    const JavaParamRef<jobject>& jcaller) {
  return base::android::ConvertUTF8ToJavaString(
      env, GetChromeSignedInEmailAddress());
}

void ClientAndroid::OnAccessToken(JNIEnv* env,
                                  const JavaParamRef<jobject>& jcaller,
                                  jboolean success,
                                  const JavaParamRef<jstring>& access_token) {
  if (fetch_access_token_callback_) {
    std::move(fetch_access_token_callback_)
        .Run(success, base::android::ConvertJavaStringToUTF8(access_token));
  }
}

void ClientAndroid::FetchWebsiteActions(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jexperiment_ids,
    const base::android::JavaParamRef<jobjectArray>& jparameter_names,
    const base::android::JavaParamRef<jobjectArray>& jparameter_values,
    const base::android::JavaParamRef<jobject>& jcallback) {
  if (!controller_)
    CreateController(nullptr);

  base::android::ScopedJavaGlobalRef<jobject> scoped_jcallback(env, jcallback);
  controller_->Track(
      CreateTriggerContext(env, jexperiment_ids, jparameter_names,
                           jparameter_values),
      base::BindOnce(&ClientAndroid::OnFetchWebsiteActions,
                     weak_ptr_factory_.GetWeakPtr(), scoped_jcallback));
}

bool ClientAndroid::HasRunFirstCheck(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) const {
  return controller_ != nullptr && controller_->HasRunFirstCheck();
}

base::android::ScopedJavaLocalRef<jobjectArray>
ClientAndroid::GetDirectActionsAsJavaArrayOfStrings(JNIEnv* env) const {
  // Using a set here helps remove duplicates.
  std::set<std::string> names;

  if (!controller_) {
    return base::android::ToJavaArrayOfStrings(
        env, std::vector<std::string>(names.begin(), names.end()));
  }

  for (const UserAction& user_action : controller_->GetUserActions()) {
    if (!user_action.enabled())
      continue;

    for (const std::string& name : user_action.direct_action().names) {
      names.insert(name);
    }
  }

  // Cancel is always available when the UI is up.
  if (ui_controller_android_)
    names.insert(kCancelActionName);

  return base::android::ToJavaArrayOfStrings(
      env, std::vector<std::string>(names.begin(), names.end()));
}

base::android::ScopedJavaLocalRef<jobject>
ClientAndroid::ToJavaAutofillAssistantDirectAction(
    JNIEnv* env,
    const DirectAction& direct_action) const {
  std::set<std::string> names;
  for (const std::string& name : direct_action.names)
    names.insert(name);
  auto jnames = base::android::ToJavaArrayOfStrings(
      env, std::vector<std::string>(names.begin(), names.end()));

  std::vector<std::string> required_arguments;
  for (const std::string& arg : direct_action.required_arguments)
    required_arguments.emplace_back(arg);
  auto jrequired_arguments =
      base::android::ToJavaArrayOfStrings(env, required_arguments);

  std::vector<std::string> optional_arguments;
  for (const std::string& arg : direct_action.optional_arguments)
    optional_arguments.emplace_back(arg);
  auto joptional_arguments =
      base::android::ToJavaArrayOfStrings(env, std::move(optional_arguments));

  return Java_AutofillAssistantDirectActionImpl_Constructor(
      env, jnames, jrequired_arguments, joptional_arguments);
}

base::android::ScopedJavaLocalRef<jobjectArray> ClientAndroid::GetDirectActions(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller) {
  DCHECK(controller_ != nullptr);
  size_t actions_count = 0;
  for (const UserAction& user_action : controller_->GetUserActions()) {
    if (!user_action.enabled())
      continue;
    ++actions_count;
  }

  // Prepare the java array to hold the direct actions.
  base::android::ScopedJavaLocalRef<jclass> directaction_array_class =
      base::android::GetClass(env,
                              "org/chromium/chrome/browser/autofill_assistant/"
                              "AutofillAssistantDirectActionImpl");

  jobjectArray joa = env->NewObjectArray(
      actions_count, directaction_array_class.obj(), nullptr);
  jni_generator::CheckException(env);

  actions_count = 0;
  for (const UserAction& user_action : controller_->GetUserActions()) {
    if (!user_action.enabled())
      continue;

    auto jdirect_action =
        ToJavaAutofillAssistantDirectAction(env, user_action.direct_action());
    env->SetObjectArrayElement(joa, actions_count++, jdirect_action.obj());
  }
  return base::android::ScopedJavaLocalRef<jobjectArray>(env, joa);
}

void ClientAndroid::OnFetchWebsiteActions(
    const base::android::JavaRef<jobject>& jcallback) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_onFetchWebsiteActions(
      env, java_object_, jcallback, controller_ != nullptr);
}

bool ClientAndroid::PerformDirectAction(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& jcaller,
    const base::android::JavaParamRef<jstring>& jaction_name,
    const base::android::JavaParamRef<jstring>& jexperiment_ids,
    const base::android::JavaParamRef<jobjectArray>& jparameter_names,
    const base::android::JavaParamRef<jobjectArray>& jparameter_values,
    const base::android::JavaParamRef<jobject>& jonboarding_coordinator) {
  std::string action_name =
      base::android::ConvertJavaStringToUTF8(env, jaction_name);

  int action_index = FindDirectAction(action_name);

  auto trigger_context = CreateTriggerContext(
      env, jexperiment_ids, jparameter_names, jparameter_values);
  trigger_context->SetDirectAction(true);

  // Cancel through the UI if it is up. This allows the user to undo. This is
  // always available, even if no action was found and action_index == -1.
  if (action_name == kCancelActionName && ui_controller_android_) {
    ui_controller_android_->CloseOrCancel(action_index,
                                          std::move(trigger_context));
    return true;
  }

  if (action_index == -1)
    return false;

  // If an overlay is already shown, then show the rest of the UI immediately.
  if (jonboarding_coordinator) {
    AttachUI(jonboarding_coordinator);
  }

  return controller_->PerformUserActionWithContext(action_index,
                                                   std::move(trigger_context));
}

int ClientAndroid::FindDirectAction(const std::string& action_name) {
  // It's too late to create a controller. This should have been done in
  // FetchWebsiteActions.
  if (!controller_)
    return -1;

  const std::vector<UserAction>& user_actions = controller_->GetUserActions();
  int user_action_count = user_actions.size();
  for (int i = 0; i < user_action_count; i++) {
    const UserAction& user_action = user_actions[i];
    if (!user_action.enabled())
      continue;

    const std::set<std::string>& action_names =
        user_action.direct_action().names;
    if (action_names.count(action_name) != 0)
      return i;
  }

  return -1;
}

void ClientAndroid::AttachUI() {
  AttachUI(nullptr);
}

void ClientAndroid::AttachUI(
    const JavaParamRef<jobject>& jonboarding_coordinator) {
  if (!ui_controller_android_) {
    ui_controller_android_ = UiControllerAndroid::CreateFromWebContents(
        web_contents_, jonboarding_coordinator);
    if (!ui_controller_android_) {
      // The activity is not or not yet in a mode where attaching the UI is
      // possible.
      return;
    }
  }

  if (!ui_controller_android_->IsAttached() ||
      (controller_ != nullptr &&
       !ui_controller_android_->IsAttachedTo(controller_.get()))) {
    if (!controller_)
      CreateController(nullptr);
    ui_controller_android_->Attach(web_contents_, this, controller_.get());

    // Suppress password manager's prompts while running a password change
    // script.
    auto* password_manager_client = GetPasswordManagerClient();
    if (password_manager_client &&
        password_manager_client->WasCredentialLeakDialogShown()) {
      password_manager_client->GetPasswordManager()->SetAutofillAssistantMode(
          password_manager::AutofillAssistantMode::kRunning);
    }
  }
}

void ClientAndroid::DestroyUI() {
  ui_controller_android_.reset();
}

version_info::Channel ClientAndroid::GetChannel() const {
  return chrome::GetChannel();
}

std::string ClientAndroid::GetEmailAddressForAccessTokenAccount() const {
  JNIEnv* env = AttachCurrentThread();
  return base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getEmailAddressForAccessTokenAccount(
          env, java_object_));
}

std::string ClientAndroid::GetChromeSignedInEmailAddress() const {
  CoreAccountInfo account_info =
      IdentityManagerFactory::GetForProfile(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))
          ->GetPrimaryAccountInfo();
  return account_info.email;
}

AccessTokenFetcher* ClientAndroid::GetAccessTokenFetcher() {
  return this;
}

autofill::PersonalDataManager* ClientAndroid::GetPersonalDataManager() const {
  return autofill::PersonalDataManagerFactory::GetForProfile(
      ProfileManager::GetLastUsedProfile());
}

password_manager::PasswordManagerClient*
ClientAndroid::GetPasswordManagerClient() const {
  if (!password_manager_client_) {
    password_manager_client_ =
        ChromePasswordManagerClient::FromWebContents(web_contents_);
  }
  return password_manager_client_;
}

WebsiteLoginManager* ClientAndroid::GetWebsiteLoginManager() const {
  if (!website_login_manager_) {
    auto* client = GetPasswordManagerClient();
    auto* factory =
        password_manager::ContentPasswordManagerDriverFactory::FromWebContents(
            web_contents_);
    // TODO(crbug.com/1043132): Add support for non-main frames. If another
    // frame has a different origin than the main frame, passwords-related
    // features may not work.
    auto* driver = factory->GetDriverForFrame(web_contents_->GetMainFrame());
    website_login_manager_ =
        std::make_unique<WebsiteLoginManagerImpl>(client, driver);
  }
  return website_login_manager_.get();
}

std::string ClientAndroid::GetLocale() const {
  return base::android::GetDefaultLocaleString();
}

std::string ClientAndroid::GetCountryCode() const {
  JNIEnv* env = AttachCurrentThread();
  auto code = Java_AutofillAssistantClient_getCountryCode(env, java_object_);
  // Use fallback "ZZ". It is an unused country code.
  if (!code)
    return "ZZ";
  return base::android::ConvertJavaStringToUTF8(env, code);
}

DeviceContext ClientAndroid::GetDeviceContext() const {
  DeviceContext context;
  Version version;
  version.sdk_int = Java_AutofillAssistantClient_getSdkInt(
      AttachCurrentThread(), java_object_);

  context.version = version;
  context.manufacturer = base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getDeviceManufacturer(AttachCurrentThread(),
                                                         java_object_));
  context.model = base::android::ConvertJavaStringToUTF8(
      Java_AutofillAssistantClient_getDeviceModel(AttachCurrentThread(),
                                                  java_object_));
  return context;
}

content::WebContents* ClientAndroid::GetWebContents() const {
  return web_contents_;
}

void ClientAndroid::Shutdown(Metrics::DropOutReason reason) {
  if (!controller_)
    return;

  GetPasswordManagerClient()->GetPasswordManager()->SetAutofillAssistantMode(
      password_manager::AutofillAssistantMode::kNotRunning);

  if (ui_controller_android_ && ui_controller_android_->IsAttached())
    DestroyUI();

  if (started_)
    Metrics::RecordDropOut(reason);

  // Delete the controller in a separate task. This avoids tricky ordering
  // issues when Shutdown is called from the controller.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&ClientAndroid::DestroyController,
                                weak_ptr_factory_.GetWeakPtr()));
}

void ClientAndroid::FetchAccessToken(
    base::OnceCallback<void(bool, const std::string&)> callback) {
  DCHECK(!fetch_access_token_callback_);

  fetch_access_token_callback_ = std::move(callback);
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_fetchAccessToken(env, java_object_);
}

void ClientAndroid::InvalidateAccessToken(const std::string& access_token) {
  JNIEnv* env = AttachCurrentThread();
  Java_AutofillAssistantClient_invalidateAccessToken(
      env, java_object_,
      base::android::ConvertUTF8ToJavaString(env, access_token));
}

void ClientAndroid::CreateController(std::unique_ptr<Service> service) {
  if (controller_) {
    return;
  }
  controller_ = std::make_unique<Controller>(
      web_contents_, /* client= */ this, base::DefaultTickClock::GetInstance(),
      std::move(service));
}

void ClientAndroid::DestroyController() {
  controller_.reset();
  started_ = false;
}

bool ClientAndroid::NeedsUI() {
  return !ui_controller_android_ && controller_ && controller_->NeedsUI();
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(ClientAndroid)

}  // namespace autofill_assistant
