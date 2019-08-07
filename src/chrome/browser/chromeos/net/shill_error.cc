// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/net/shill_error.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_state_handler.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

namespace {

const NetworkState* GetNetworkState(const std::string& network_id) {
  return NetworkHandler::Get()
      ->network_state_handler()
      ->GetNetworkStateFromGuid(network_id);
}

}  // namespace

namespace shill_error {

base::string16 GetShillErrorString(const std::string& error,
                                   const std::string& network_id) {
  if (error.empty())
    return base::string16();
  if (error == shill::kErrorOutOfRange)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OUT_OF_RANGE);
  if (error == shill::kErrorPinMissing)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_PIN_MISSING);
  if (error == shill::kErrorDhcpFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_DHCP_FAILED);
  if (error == shill::kErrorConnectFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_CONNECT_FAILED);
  if (error == shill::kErrorBadPassphrase ||
      error == shill::kErrorResultInvalidPassphrase) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_PASSPHRASE);
  }
  if (error == shill::kErrorBadWEPKey)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_BAD_WEPKEY);
  if (error == shill::kErrorActivationFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_ACTIVATION_FAILED);
  }
  if (error == shill::kErrorNeedEvdo)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_NEED_EVDO);
  if (error == shill::kErrorNeedHomeNetwork) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_NEED_HOME_NETWORK);
  }
  if (error == shill::kErrorOtaspFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_OTASP_FAILED);
  if (error == shill::kErrorAaaFailed)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_AAA_FAILED);
  if (error == shill::kErrorInternal)
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_INTERNAL);
  if (error == shill::kErrorDNSLookupFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_DNS_LOOKUP_FAILED);
  }
  if (error == shill::kErrorHTTPGetFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_HTTP_GET_FAILED);
  }
  if (error == shill::kErrorIpsecPskAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_IPSEC_PSK_AUTH_FAILED);
  }
  if (error == shill::kErrorIpsecCertAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_CERT_AUTH_FAILED);
  }
  if (error == shill::kErrorEapAuthenticationFailed) {
    const NetworkState* network =
        network_id.empty() ? nullptr : GetNetworkState(network_id);
    // TLS always requires a client certificate, so show a cert auth
    // failed message for TLS. Other EAP methods do not generally require
    // a client certicate.
    if (network && network->eap_method() == shill::kEapMethodTLS) {
      return l10n_util::GetStringUTF16(
          IDS_CHROMEOS_NETWORK_ERROR_CERT_AUTH_FAILED);
    } else {
      return l10n_util::GetStringUTF16(
          IDS_CHROMEOS_NETWORK_ERROR_EAP_AUTH_FAILED);
    }
  }
  if (error == shill::kErrorEapLocalTlsFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_EAP_LOCAL_TLS_FAILED);
  }
  if (error == shill::kErrorEapRemoteTlsFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_EAP_REMOTE_TLS_FAILED);
  }
  if (error == shill::kErrorPppAuthFailed) {
    return l10n_util::GetStringUTF16(
        IDS_CHROMEOS_NETWORK_ERROR_PPP_AUTH_FAILED);
  }

  if (base::ToLowerASCII(error) == base::ToLowerASCII(shill::kUnknownString)) {
    return l10n_util::GetStringUTF16(IDS_CHROMEOS_NETWORK_ERROR_UNKNOWN);
  }
  return l10n_util::GetStringFUTF16(IDS_NETWORK_UNRECOGNIZED_ERROR,
                                    base::UTF8ToUTF16(error));
}

bool IsConfigurationError(const std::string& error) {
  if (error.empty())
    return false;
  return error == shill::kErrorPinMissing ||
         error == shill::kErrorBadPassphrase ||
         error == shill::kErrorResultInvalidPassphrase ||
         error == shill::kErrorBadWEPKey;
}

}  // namespace shill_error

}  // namespace chromeos
