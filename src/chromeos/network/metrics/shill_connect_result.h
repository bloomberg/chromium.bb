// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_NETWORK_METRICS_SHILL_CONNECT_RESULT_H_
#define CHROMEOS_NETWORK_METRICS_SHILL_CONNECT_RESULT_H_

#include <string>

#include "base/component_export.h"

namespace chromeos {

// Result of state changes to a network triggered by any connection
// attempt. With the exception of kSuccess and kUnknown, these enums are
// mapped directly to Shill errors at
// //third_party/cros_system_api/dbus/service_constants.h. These values are
// persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class ShillConnectResult {
  kUnknown = 0,
  // Includes the kErrorResultSuccess result code.
  kSuccess = 1,

  // Flimflam error options.
  kErrorAaaFailed = 2,
  kErrorActivationFailed = 3,
  kErrorBadPassphrase = 4,
  kErrorBadWEPKey = 5,
  kErrorConnectFailed = 6,
  kErrorDNSLookupFailed = 7,
  kErrorDhcpFailed = 8,
  kErrorHTTPGetFailed = 9,
  kErrorInternal = 10,
  kErrorInvalidFailure = 11,
  kErrorIpsecCertAuthFailed = 12,
  kErrorIpsecPskAuthFailed = 13,
  kErrorNeedEvdo = 14,
  kErrorNeedHomeNetwork = 15,
  kErrorNoFailure = 16,
  kErrorNotAssociated = 17,
  kErrorNotAuthenticated = 18,
  kErrorOtaspFailed = 19,
  kErrorOutOfRange = 20,
  kErrorPinMissing = 21,
  kErrorPppAuthFailed = 22,
  kErrorSimLocked = 23,
  kErrorNotRegistered = 24,
  kErrorTooManySTAs = 25,
  kErrorDisconnect = 26,
  kErrorUnknownFailure = 27,

  // Flimflam error result codes.
  kErrorResultFailure = 28,
  kErrorResultAlreadyConnected = 29,
  kErrorResultAlreadyExists = 30,
  kErrorResultIncorrectPin = 31,
  kErrorResultInProgress = 32,
  kErrorResultInternalError = 33,
  kErrorResultInvalidApn = 34,
  kErrorResultInvalidArguments = 35,
  kErrorResultInvalidNetworkName = 36,
  kErrorResultInvalidPassphrase = 37,
  kErrorResultInvalidProperty = 38,
  kErrorResultNoCarrier = 39,
  kErrorResultNotConnected = 40,
  kErrorResultNotFound = 41,
  kErrorResultNotImplemented = 42,
  kErrorResultNotOnHomeNetwork = 43,
  kErrorResultNotRegistered = 44,
  kErrorResultNotSupported = 45,
  kErrorResultOperationAborted = 46,
  kErrorResultOperationInitiated = 47,
  kErrorResultOperationTimeout = 48,
  kErrorResultPassphraseRequired = 49,
  kErrorResultPermissionDenied = 50,
  kErrorResultPinBlocked = 51,
  kErrorResultPinRequired = 52,
  kErrorResultWrongState = 53,

  // Error strings.
  kErrorEapAuthenticationFailed = 54,
  kErrorEapLocalTlsFailed = 55,
  kErrorEapRemoteTlsFailed = 56,

  kMaxValue = kErrorEapRemoteTlsFailed,
};

COMPONENT_EXPORT(CHROMEOS_NETWORK)
ShillConnectResult ShillErrorToConnectResult(const std::string& error_name);

}  // namespace chromeos

#endif  // CHROMEOS_NETWORK_METRICS_SHILL_CONNECT_RESULT_H_
