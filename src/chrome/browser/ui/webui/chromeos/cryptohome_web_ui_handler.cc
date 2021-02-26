// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/cryptohome_web_ui_handler.h"

#include "base/bind.h"
#include "base/values.h"
#include "chromeos/dbus/cryptohome/cryptohome_client.h"
#include "chromeos/dbus/cryptohome/rpc.pb.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_ui.h"
#include "crypto/nss_util.h"

using content::BrowserThread;

namespace chromeos {

CryptohomeWebUIHandler::CryptohomeWebUIHandler() {}

CryptohomeWebUIHandler::~CryptohomeWebUIHandler() {}

void CryptohomeWebUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "pageLoaded", base::BindRepeating(&CryptohomeWebUIHandler::OnPageLoaded,
                                        weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeWebUIHandler::OnPageLoaded(const base::ListValue* args) {
  CryptohomeClient* cryptohome_client = CryptohomeClient::Get();

  cryptohome_client->IsMounted(GetCryptohomeBoolCallback("is-mounted"));
  cryptohome_client->TpmIsReady(GetCryptohomeBoolCallback("tpm-is-ready"));
  cryptohome_client->TpmIsEnabled(GetCryptohomeBoolCallback("tpm-is-enabled"));
  cryptohome_client->TpmIsOwned(GetCryptohomeBoolCallback("tpm-is-owned"));
  cryptohome_client->Pkcs11IsTpmTokenReady(
      GetCryptohomeBoolCallback("pkcs11-is-tpm-token-ready"));

  cryptohome_client->GetTpmStatus(
      cryptohome::GetTpmStatusRequest(),
      base::BindOnce(&CryptohomeWebUIHandler::OnCryptohomeTpmStatus,
                     weak_ptr_factory_.GetWeakPtr()));

  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(&crypto::IsTPMTokenReady, base::Closure()),
      base::BindOnce(&CryptohomeWebUIHandler::DidGetNSSUtilInfoOnUIThread,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CryptohomeWebUIHandler::DidGetNSSUtilInfoOnUIThread(
    bool is_tpm_token_ready) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  base::Value is_tpm_token_ready_value(is_tpm_token_ready);
  SetCryptohomeProperty("is-tpm-token-ready", is_tpm_token_ready_value);
}

DBusMethodCallback<bool> CryptohomeWebUIHandler::GetCryptohomeBoolCallback(
    const std::string& destination_id) {
  return base::BindOnce(&CryptohomeWebUIHandler::OnCryptohomeBoolProperty,
                        weak_ptr_factory_.GetWeakPtr(), destination_id);
}

void CryptohomeWebUIHandler::OnCryptohomeTpmStatus(
    base::Optional<cryptohome::BaseReply> reply) {
  if (!reply.has_value() || reply->has_error() ||
      !reply->HasExtension(cryptohome::GetTpmStatusReply::reply)) {
    LOG(ERROR) << "TPM status request failed, error: "
               << (reply.has_value() && reply->has_error() ? reply->error()
                                                           : 0);
    return;
  }
  auto reply_proto = reply->GetExtension(cryptohome::GetTpmStatusReply::reply);

  SetCryptohomeProperty("has-reset-lock-permissions",
                        base::Value(reply_proto.has_reset_lock_permissions()));
}

void CryptohomeWebUIHandler::OnCryptohomeBoolProperty(
    const std::string& destination_id,
    base::Optional<bool> result) {
  SetCryptohomeProperty(destination_id, base::Value(result.value_or(false)));
}

void CryptohomeWebUIHandler::SetCryptohomeProperty(
    const std::string& destination_id,
    const base::Value& value) {
  base::Value destination_id_value(destination_id);
  web_ui()->CallJavascriptFunctionUnsafe("SetCryptohomeProperty",
                                         destination_id_value, value);
}

}  // namespace chromeos
