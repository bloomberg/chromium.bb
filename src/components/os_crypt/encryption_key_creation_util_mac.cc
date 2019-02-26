// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/os_crypt/encryption_key_creation_util_mac.h"

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/single_thread_task_runner.h"
#include "components/os_crypt/os_crypt_pref_names_mac.h"
#include "components/prefs/pref_service.h"

namespace os_crypt {

using GetKeyAction = EncryptionKeyCreationUtil::GetKeyAction;

namespace {

void LogGetEncryptionKeyActionMetric(
    EncryptionKeyCreationUtil::GetKeyAction action) {
  UMA_HISTOGRAM_ENUMERATION("OSCrypt.GetEncryptionKeyAction", action);
}

}  // namespace

EncryptionKeyCreationUtilMac::EncryptionKeyCreationUtilMac(
    PrefService* local_state,
    scoped_refptr<base::SingleThreadTaskRunner> main_thread_task_runner)
    : local_state_(local_state),
      main_thread_task_runner_(main_thread_task_runner),
      key_already_created_(local_state_->GetBoolean(prefs::kKeyCreated)) {}

EncryptionKeyCreationUtilMac::~EncryptionKeyCreationUtilMac() = default;

void EncryptionKeyCreationUtilMac::OnKeyWasFound() {
  if (key_already_created_) {
    LogGetEncryptionKeyActionMetric(GetKeyAction::kKeyFound);
  } else {
    LogGetEncryptionKeyActionMetric(GetKeyAction::kKeyFoundFirstTime);
  }

  UpdateKeyCreationPreference();
}

void EncryptionKeyCreationUtilMac::OnKeyNotFound(bool new_key_stored) {
  if (key_already_created_) {
    if (new_key_stored)
      LogGetEncryptionKeyActionMetric(GetKeyAction::kKeyPotentiallyOverwritten);
    else
      LogGetEncryptionKeyActionMetric(GetKeyAction::kKeyOverwriteFailed);
  } else {
    if (new_key_stored) {
      LogGetEncryptionKeyActionMetric(GetKeyAction::kNewKeyAddedToKeychain);
      UpdateKeyCreationPreference();
    } else {
      LogGetEncryptionKeyActionMetric(GetKeyAction::kNewKeyAddError);
    }
  }
}

void EncryptionKeyCreationUtilMac::OnKeychainLookupFailed() {
  LogGetEncryptionKeyActionMetric(GetKeyAction::kKeychainLookupFailed);
}

void EncryptionKeyCreationUtilMac::UpdateKeyCreationPreference() {
  if (key_already_created_)
    return;
  key_already_created_ = true;
  main_thread_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(
                     [](PrefService* local_state) {
                       local_state->SetBoolean(prefs::kKeyCreated, true);
                     },
                     local_state_));
}

}  // namespace os_crypt
