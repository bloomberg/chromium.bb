// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/password_store_android_backend_bridge_impl.h"

#include <jni.h>
#include <cstdint>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "chrome/browser/password_manager/android/jni_headers/PasswordStoreAndroidBackendBridgeImpl_jni.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/sync/password_proto_utils.h"
#include "components/sync/protocol/list_passwords_result.pb.h"

using JobId = PasswordStoreAndroidBackendBridgeImpl::JobId;

namespace {

std::vector<password_manager::PasswordForm> CreateFormsVector(
    const base::android::JavaRef<jbyteArray>& passwords) {
  std::vector<uint8_t> serialized_result;
  base::android::JavaByteArrayToByteVector(base::android::AttachCurrentThread(),
                                           passwords, &serialized_result);
  sync_pb::ListPasswordsResult list_passwords_result;
  // TODO(crbug.com/1229655): Check that parsing executes successfully.
  list_passwords_result.ParseFromArray(serialized_result.data(),
                                       serialized_result.size());
  auto forms =
      password_manager::PasswordVectorFromListResult(list_passwords_result);
  for (auto& form : forms) {
    // Set proper in_store value for GMS Core storage.
    form.in_store = password_manager::PasswordForm::Store::kProfileStore;
  }
  return forms;
}

sync_pb::PasswordSpecificsData CreatePasswordSpecificsData(
    const base::android::JavaRef<jbyteArray>& login) {
  std::vector<uint8_t> serialized_result;
  base::android::JavaByteArrayToByteVector(base::android::AttachCurrentThread(),
                                           login, &serialized_result);
  sync_pb::PasswordSpecificsData result;
  // TODO(crbug.com/1229655): Check that parsing executes successfully.
  result.ParseFromArray(serialized_result.data(), serialized_result.size());
  return result;
}

sync_pb::PasswordWithLocalData CreatePasswordWithLocalData(
    const base::android::JavaRef<jbyteArray>& login) {
  std::vector<uint8_t> serialized_result;
  base::android::JavaByteArrayToByteVector(base::android::AttachCurrentThread(),
                                           login, &serialized_result);
  sync_pb::PasswordWithLocalData result;
  // TODO(crbug.com/1229655): Check that parsing executes successfully.
  result.ParseFromArray(serialized_result.data(), serialized_result.size());
  return result;
}

}  // namespace

namespace password_manager {

std::unique_ptr<PasswordStoreAndroidBackendBridge>
PasswordStoreAndroidBackendBridge::Create() {
  return std::make_unique<PasswordStoreAndroidBackendBridgeImpl>();
}

bool PasswordStoreAndroidBackendBridge::CanCreateBackend() {
  return Java_PasswordStoreAndroidBackendBridgeImpl_canCreateBackend(
      base::android::AttachCurrentThread());
}

}  // namespace password_manager

PasswordStoreAndroidBackendBridgeImpl::PasswordStoreAndroidBackendBridgeImpl() {
  java_object_ = Java_PasswordStoreAndroidBackendBridgeImpl_create(
      base::android::AttachCurrentThread(), reinterpret_cast<intptr_t>(this));
}

PasswordStoreAndroidBackendBridgeImpl::
    ~PasswordStoreAndroidBackendBridgeImpl() {
  Java_PasswordStoreAndroidBackendBridgeImpl_destroy(
      base::android::AttachCurrentThread(), java_object_);
}
void PasswordStoreAndroidBackendBridgeImpl::SetConsumer(
    base::WeakPtr<Consumer> consumer) {
  consumer_ = consumer;
}

void PasswordStoreAndroidBackendBridgeImpl::OnCompleteWithLogins(
    JNIEnv* env,
    jint job_id,
    const base::android::JavaParamRef<jbyteArray>& passwords) {
  DCHECK(consumer_);
  consumer_->OnCompleteWithLogins(JobId(job_id), CreateFormsVector(passwords));
}

void PasswordStoreAndroidBackendBridgeImpl::OnError(JNIEnv* env,
                                                    jint job_id,
                                                    jint error_type,
                                                    jint api_error_code) {
  DCHECK(consumer_);
  // Posting the tasks to the same sequence prevents that synchronous responses
  // try to finish tasks before their registration was completed.
  password_manager::AndroidBackendError error{
      static_cast<password_manager::AndroidBackendErrorType>(error_type)};

  if (error.type == password_manager::AndroidBackendErrorType::kExternalError) {
    error.api_error_code = static_cast<int>(api_error_code);
  }

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&PasswordStoreAndroidBackendBridge::Consumer::OnError,
                     consumer_, JobId(job_id), std::move(error)));
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetAllLogins(
    password_manager::PasswordStoreOperationTarget target) {
  JobId job_id = GetNextJobId();
  Java_PasswordStoreAndroidBackendBridgeImpl_getAllLogins(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      static_cast<int>(target));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetAutofillableLogins() {
  JobId job_id = GetNextJobId();
  Java_PasswordStoreAndroidBackendBridgeImpl_getAutofillableLogins(
      base::android::AttachCurrentThread(), java_object_, job_id.value());
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetLoginsForSignonRealm(
    const std::string& signon_realm) {
  JobId job_id = GetNextJobId();
  Java_PasswordStoreAndroidBackendBridgeImpl_getLoginsForSignonRealm(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ConvertUTF8ToJavaString(
          base::android::AttachCurrentThread(), signon_realm));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::AddLogin(
    const password_manager::PasswordForm& form) {
  JobId job_id = GetNextJobId();
  sync_pb::PasswordWithLocalData data = PasswordWithLocalDataFromPassword(form);
  Java_PasswordStoreAndroidBackendBridgeImpl_addLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::UpdateLogin(
    const password_manager::PasswordForm& form) {
  JobId job_id = GetNextJobId();
  sync_pb::PasswordWithLocalData data = PasswordWithLocalDataFromPassword(form);
  Java_PasswordStoreAndroidBackendBridgeImpl_updateLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()));
  return job_id;
}

JobId PasswordStoreAndroidBackendBridgeImpl::RemoveLogin(
    const password_manager::PasswordForm& form,
    password_manager::PasswordStoreOperationTarget target) {
  JobId job_id = GetNextJobId();
  sync_pb::PasswordSpecificsData data = SpecificsDataFromPassword(form);
  Java_PasswordStoreAndroidBackendBridgeImpl_removeLogin(
      base::android::AttachCurrentThread(), java_object_, job_id.value(),
      base::android::ToJavaByteArray(base::android::AttachCurrentThread(),
                                     data.SerializeAsString()),
      static_cast<int>(target));
  return job_id;
}

void PasswordStoreAndroidBackendBridgeImpl::OnLoginAdded(
    JNIEnv* env,
    jint job_id,
    const base::android::JavaParamRef<jbyteArray>& login) {
  DCHECK(consumer_);

  // TODO(crbug.com/1229655): This is a temporary workaround, while store
  // change deltas are not received to confirm the correct operation execution.
  sync_pb::PasswordWithLocalData login_data =
      CreatePasswordWithLocalData(login);
  password_manager::PasswordStoreChangeList changelist;
  password_manager::PasswordForm added_form =
      password_manager::PasswordFromProtoWithLocalData(login_data);
  changelist.emplace_back(password_manager::PasswordStoreChange::ADD,
                          added_form);

  consumer_->OnLoginsChanged(JobId(job_id), changelist);
}

void PasswordStoreAndroidBackendBridgeImpl::OnLoginUpdated(
    JNIEnv* env,
    jint job_id,
    const base::android::JavaParamRef<jbyteArray>& login) {
  DCHECK(consumer_);

  // TODO(crbug.com/1229655): This is a temporary workaround, while store
  // change deltas are not received to confirm the correct operation execution.
  sync_pb::PasswordWithLocalData login_data =
      CreatePasswordWithLocalData(login);
  password_manager::PasswordStoreChangeList changelist;
  password_manager::PasswordForm updated_form =
      password_manager::PasswordFromProtoWithLocalData(login_data);
  changelist.emplace_back(password_manager::PasswordStoreChange::UPDATE,
                          updated_form);

  consumer_->OnLoginsChanged(JobId(job_id), changelist);
}

void PasswordStoreAndroidBackendBridgeImpl::OnLoginDeleted(
    JNIEnv* env,
    jint job_id,
    const base::android::JavaParamRef<jbyteArray>& login) {
  DCHECK(consumer_);

  // TODO(crbug.com/1229655): This is a temporary workaround, while store
  // change deltas are not received to confirm the correct operation execution.
  sync_pb::PasswordSpecificsData login_data =
      CreatePasswordSpecificsData(login);
  password_manager::PasswordStoreChangeList changelist;
  password_manager::PasswordForm removed_form =
      password_manager::PasswordFromSpecifics(login_data);
  changelist.emplace_back(password_manager::PasswordStoreChange::REMOVE,
                          removed_form);

  consumer_->OnLoginsChanged(JobId(job_id), changelist);
}

JobId PasswordStoreAndroidBackendBridgeImpl::GetNextJobId() {
  last_job_id_ = JobId(last_job_id_.value() + 1);
  return last_job_id_;
}
