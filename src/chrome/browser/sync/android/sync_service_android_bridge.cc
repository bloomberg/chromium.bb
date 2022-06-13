// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/android/sync_service_android_bridge.h"

#include <string>
#include <vector>

#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/android/jni_headers/SyncServiceImpl_jni.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/sync/driver/sync_service_impl.h"
#include "components/sync/driver/sync_service_utils.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ui/base/l10n/l10n_util.h"

using base::android::AttachCurrentThread;
using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace {

// Native callback for the JNI GetAllNodes method. When
// SyncServiceImpl::GetAllNodes completes, this method is called and the
// results are sent to the Java callback.
void NativeGetAllNodesCallback(
    JNIEnv* env,
    const base::android::ScopedJavaGlobalRef<jobject>& callback,
    std::unique_ptr<base::ListValue> result) {
  std::string json_string;
  if (!result.get() || !base::JSONWriter::Write(*result, &json_string)) {
    DVLOG(1) << "Writing as JSON failed. Passing empty string to Java code.";
    json_string = std::string();
  }

  Java_SyncServiceImpl_onGetAllNodesResult(
      env, callback, ConvertUTF8ToJavaString(env, json_string));
}

ScopedJavaLocalRef<jintArray> ModelTypeSetToJavaIntArray(
    JNIEnv* env,
    syncer::ModelTypeSet types) {
  std::vector<int> type_vector;
  for (syncer::ModelType type : types) {
    type_vector.push_back(type);
  }
  return base::android::ToJavaIntArray(env, type_vector);
}

}  // namespace

SyncServiceAndroidBridge::SyncServiceAndroidBridge(
    JNIEnv* env,
    syncer::SyncServiceImpl* native_sync_service,
    jobject java_sync_service)
    : native_sync_service_(native_sync_service),
      java_sync_service_(env, java_sync_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(native_sync_service_);
  native_sync_service_->AddObserver(this);
}

SyncServiceAndroidBridge::~SyncServiceAndroidBridge() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->RemoveObserver(this);
}

void SyncServiceAndroidBridge::OnStateChanged(syncer::SyncService* sync) {
  // Notify the java world that our sync state has changed.
  JNIEnv* env = AttachCurrentThread();
  Java_SyncServiceImpl_syncStateChanged(env, java_sync_service_.get(env));
}

jboolean SyncServiceAndroidBridge::IsSyncRequested(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsSyncRequested();
}

void SyncServiceAndroidBridge::SetSyncRequested(JNIEnv* env,
                                                jboolean requested) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->GetUserSettings()->SetSyncRequested(requested);
}

jboolean SyncServiceAndroidBridge::CanSyncFeatureStart(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->CanSyncFeatureStart();
}

jboolean SyncServiceAndroidBridge::IsSyncAllowedByPlatform(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return !native_sync_service_->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_PLATFORM_OVERRIDE);
}

void SyncServiceAndroidBridge::SetSyncAllowedByPlatform(JNIEnv* env,
                                                        jboolean allowed) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->SetSyncAllowedByPlatform(allowed);
}

jboolean SyncServiceAndroidBridge::IsSyncFeatureEnabled(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->IsSyncFeatureEnabled();
}

jboolean SyncServiceAndroidBridge::IsSyncFeatureActive(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->IsSyncFeatureActive();
}

jboolean SyncServiceAndroidBridge::IsSyncDisabledByEnterprisePolicy(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->HasDisableReason(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);
}

jboolean SyncServiceAndroidBridge::IsEngineInitialized(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->IsEngineInitialized();
}

jboolean SyncServiceAndroidBridge::IsTransportStateActive(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetTransportState() ==
         syncer::SyncService::TransportState::ACTIVE;
}

void SyncServiceAndroidBridge::SetSetupInProgress(JNIEnv* env,
                                                  jboolean in_progress) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!in_progress) {
    sync_blocker_.reset();
    return;
  }

  if (!sync_blocker_) {
    sync_blocker_ = native_sync_service_->GetSetupInProgressHandle();
  }
}

jboolean SyncServiceAndroidBridge::IsFirstSetupComplete(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsFirstSetupComplete();
}

void SyncServiceAndroidBridge::SetFirstSetupComplete(JNIEnv* env, jint source) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->GetUserSettings()->SetFirstSetupComplete(
      static_cast<syncer::SyncFirstSetupCompleteSource>(source));
}

ScopedJavaLocalRef<jintArray> SyncServiceAndroidBridge::GetActiveDataTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return ModelTypeSetToJavaIntArray(env,
                                    native_sync_service_->GetActiveDataTypes());
}

ScopedJavaLocalRef<jintArray> SyncServiceAndroidBridge::GetChosenDataTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  // TODO(crbug/950874): introduce UserSelectableType in java code, then remove
  // workaround here and in SetChosenDataTypes().
  syncer::ModelTypeSet model_types;
  for (syncer::UserSelectableType type :
       native_sync_service_->GetUserSettings()->GetSelectedTypes()) {
    model_types.Put(syncer::UserSelectableTypeToCanonicalModelType(type));
  }
  return ModelTypeSetToJavaIntArray(env, model_types);
}

void SyncServiceAndroidBridge::SetChosenDataTypes(
    JNIEnv* env,
    jboolean sync_everything,
    const JavaParamRef<jintArray>& model_type_array) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  std::vector<int> types_vector;
  base::android::JavaIntArrayToIntVector(env, model_type_array, &types_vector);

  syncer::ModelTypeSet model_types;
  for (int type : types_vector) {
    model_types.Put(static_cast<syncer::ModelType>(type));
  }

  syncer::UserSelectableTypeSet selected_types;
  for (syncer::UserSelectableType type : syncer::UserSelectableTypeSet::All()) {
    if (model_types.Has(syncer::UserSelectableTypeToCanonicalModelType(type))) {
      selected_types.Put(type);
    }
  }

  native_sync_service_->GetUserSettings()->SetSelectedTypes(sync_everything,
                                                            selected_types);
}

jboolean SyncServiceAndroidBridge::IsCustomPassphraseAllowed(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsCustomPassphraseAllowed();
}

jboolean SyncServiceAndroidBridge::IsEncryptEverythingEnabled(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsEncryptEverythingEnabled();
}

jboolean SyncServiceAndroidBridge::IsPassphraseRequiredForPreferredDataTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->IsPassphraseRequiredForPreferredDataTypes();
}

jboolean SyncServiceAndroidBridge::IsTrustedVaultKeyRequired(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsTrustedVaultKeyRequired();
}

jboolean
SyncServiceAndroidBridge::IsTrustedVaultKeyRequiredForPreferredDataTypes(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->IsTrustedVaultKeyRequiredForPreferredDataTypes();
}

jboolean SyncServiceAndroidBridge::IsTrustedVaultRecoverabilityDegraded(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->IsTrustedVaultRecoverabilityDegraded();
}

jboolean SyncServiceAndroidBridge::IsUsingExplicitPassphrase(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsUsingExplicitPassphrase();
}

jint SyncServiceAndroidBridge::GetPassphraseType(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return static_cast<unsigned>(
      native_sync_service_->GetUserSettings()->GetPassphraseType());
}

void SyncServiceAndroidBridge::SetEncryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->GetUserSettings()->SetEncryptionPassphrase(
      ConvertJavaStringToUTF8(env, passphrase));
}

jboolean SyncServiceAndroidBridge::SetDecryptionPassphrase(
    JNIEnv* env,
    const JavaParamRef<jstring>& passphrase) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->SetDecryptionPassphrase(
      ConvertJavaStringToUTF8(env, passphrase));
}

jlong SyncServiceAndroidBridge::GetExplicitPassphraseTime(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->GetExplicitPassphraseTime()
      .ToJavaTime();
}

void SyncServiceAndroidBridge::GetAllNodes(
    JNIEnv* env,
    const JavaParamRef<jobject>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::android::ScopedJavaGlobalRef<jobject> java_callback;
  java_callback.Reset(env, callback);
  native_sync_service_->GetAllNodesForDebugging(
      base::BindOnce(&NativeGetAllNodesCallback, env, java_callback));
}

jint SyncServiceAndroidBridge::GetAuthError(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetAuthError().state();
}

jboolean SyncServiceAndroidBridge::HasUnrecoverableError(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->HasUnrecoverableError();
}

jboolean SyncServiceAndroidBridge::RequiresClientUpgrade(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->RequiresClientUpgrade();
}

void SyncServiceAndroidBridge::SetDecoupledFromAndroidMasterSync(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->SetDecoupledFromAndroidMasterSync();
}

jboolean SyncServiceAndroidBridge::GetDecoupledFromAndroidMasterSync(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetDecoupledFromAndroidMasterSync();
}

base::android::ScopedJavaLocalRef<jobject>
SyncServiceAndroidBridge::GetAccountInfo(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  CoreAccountInfo account_info = native_sync_service_->GetAccountInfo();
  return account_info.IsEmpty()
             ? nullptr
             : ConvertToJavaCoreAccountInfo(env, account_info);
}

jboolean SyncServiceAndroidBridge::HasSyncConsent(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->HasSyncConsent();
}

jboolean
SyncServiceAndroidBridge::IsPassphrasePromptMutedForCurrentProductVersion(
    JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()
      ->IsPassphrasePromptMutedForCurrentProductVersion();
}

void SyncServiceAndroidBridge::
    MarkPassphrasePromptMutedForCurrentProductVersion(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->GetUserSettings()
      ->MarkPassphrasePromptMutedForCurrentProductVersion();
}

jboolean SyncServiceAndroidBridge::HasKeepEverythingSynced(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return native_sync_service_->GetUserSettings()->IsSyncEverythingEnabled();
}

jboolean SyncServiceAndroidBridge::ShouldOfferTrustedVaultOptIn(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return syncer::ShouldOfferTrustedVaultOptIn(native_sync_service_);
}

void SyncServiceAndroidBridge::TriggerRefresh(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  native_sync_service_->TriggerRefresh(syncer::ModelTypeSet::All());
}

jlong SyncServiceAndroidBridge::GetLastSyncedTimeForDebugging(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  base::Time last_sync_time =
      native_sync_service_->GetLastSyncedTimeForDebugging();
  return static_cast<jlong>(
      (last_sync_time - base::Time::UnixEpoch()).InMicroseconds());
}

jlong SyncServiceAndroidBridge::GetNativeSyncServiceImplForTest(JNIEnv* env) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  return reinterpret_cast<intptr_t>(native_sync_service_.get());
}

static jlong JNI_SyncServiceImpl_Init(
    JNIEnv* env,
    const JavaParamRef<jobject>& java_sync_service) {
  DCHECK(g_browser_process && g_browser_process->profile_manager());

  syncer::SyncServiceImpl* sync_service =
      SyncServiceFactory::GetAsSyncServiceImplForProfile(
          ProfileManager::GetLastUsedProfile());
  if (!sync_service) {
    return 0;
  }

  // Owned by the caller.
  return reinterpret_cast<intptr_t>(
      new SyncServiceAndroidBridge(env, sync_service, java_sync_service));
}
