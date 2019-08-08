// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/gaiacp/mdm_utils.h"

#include <windows.h>

#define _NTDEF_  // Prevent redefition errors, must come after <winternl.h>
#include <MDMRegistration.h>  // For RegisterDeviceWithManagement()

#include <atlconv.h>

#include "base/base64.h"
#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/win_util.h"
#include "base/win/wmi.h"
#include "chrome/credential_provider/common/gcp_strings.h"
#include "chrome/credential_provider/gaiacp/gcp_utils.h"
#include "chrome/credential_provider/gaiacp/logging.h"
#include "chrome/credential_provider/gaiacp/reg_utils.h"

namespace credential_provider {

constexpr wchar_t kRegMdmUrl[] = L"mdm";
constexpr wchar_t kRegMdmSupportsMultiUser[] = L"mdm_mu";
constexpr wchar_t kRegMdmAllowConsumerAccounts[] = L"mdm_aca";

// Overridden in tests to force the MDM enrollment to either succeed or fail.
enum class EnrollmentStatus {
  kForceSuccess,
  kForceFailure,
  kDontForce,
};
EnrollmentStatus g_enrollment_status = EnrollmentStatus::kDontForce;

// Overridden in tests to force the MDM enrollment check to either return true
// or false.
enum class EnrolledStatus {
  kForceTrue,
  kForceFalse,
  kDontForce,
};
EnrolledStatus g_enrolled_status = EnrolledStatus::kDontForce;

namespace {

template <typename T>
T GetMdmFunctionPointer(const base::ScopedNativeLibrary& library,
                        const char* function_name) {
  if (!library.is_valid())
    return nullptr;

  return reinterpret_cast<T>(library.GetFunctionPointer(function_name));
}

#define GET_MDM_FUNCTION_POINTER(library, name) \
  GetMdmFunctionPointer<decltype(&::name)>(library, #name)

base::string16 GetMdmUrl() {
  wchar_t mdm_url[256];
  ULONG length = base::size(mdm_url);
  HRESULT hr = GetGlobalFlag(kRegMdmUrl, mdm_url, &length);
  if (FAILED(hr))
    return L"https://deviceenrollmentforwindows.googleapis.com/v1/discovery";

  return mdm_url;
}

bool IsEnrolledWithGoogleMdm(const base::string16& mdm_url) {
  switch (g_enrolled_status) {
    case EnrolledStatus::kForceTrue:
      return true;
    case EnrolledStatus::kForceFalse:
      return false;
    case EnrolledStatus::kDontForce:
      break;
  }

  base::ScopedNativeLibrary library(
      base::FilePath(FILE_PATH_LITERAL("MDMRegistration.dll")));
  auto get_device_registration_info_function =
      GET_MDM_FUNCTION_POINTER(library, GetDeviceRegistrationInfo);
  if (!get_device_registration_info_function) {
    // On Windows < 1803 the function GetDeviceRegistrationInfo does not exist
    // in MDMRegistration.dll so we have to fallback to the less accurate
    // IsDeviceRegisteredWithManagement. This can return false positives if the
    // machine is registered to MDM but to a different server.
    LOGFN(ERROR) << "GET_MDM_FUNCTION_POINTER(GetDeviceRegistrationInfo)";
    auto is_device_registered_with_management_function =
        GET_MDM_FUNCTION_POINTER(library, IsDeviceRegisteredWithManagement);
    if (!is_device_registered_with_management_function) {
      LOGFN(ERROR)
          << "GET_MDM_FUNCTION_POINTER(IsDeviceRegisteredWithManagement)";
      return false;
    } else {
      BOOL is_managed = FALSE;
      HRESULT hr = is_device_registered_with_management_function(&is_managed, 0,
                                                                 nullptr);
      return SUCCEEDED(hr) && is_managed;
    }
  }

  MANAGEMENT_REGISTRATION_INFO* info;
  HRESULT hr = get_device_registration_info_function(
      DeviceRegistrationBasicInfo, reinterpret_cast<void**>(&info));

  bool is_enrolled = SUCCEEDED(hr) && info->fDeviceRegisteredWithManagement &&
                     GURL(mdm_url) == GURL(info->pszMDMServiceUri);

  if (SUCCEEDED(hr))
    ::HeapFree(::GetProcessHeap(), 0, info);
  return is_enrolled;
}

HRESULT ExtractRegistrationData(const base::Value& registration_data,
                                base::string16* out_email,
                                base::string16* out_id_token,
                                base::string16* out_access_token,
                                base::string16* out_sid,
                                base::string16* out_username,
                                base::string16* out_domain) {
  DCHECK(out_email);
  DCHECK(out_id_token);
  DCHECK(out_access_token);
  DCHECK(out_sid);
  DCHECK(out_username);
  DCHECK(out_domain);
  if (!registration_data.is_dict()) {
    LOGFN(ERROR) << "Registration data is not a dictionary";
    return E_INVALIDARG;
  }

  *out_email = GetDictString(registration_data, kKeyEmail);
  *out_id_token = GetDictString(registration_data, kKeyMdmIdToken);
  *out_access_token = GetDictString(registration_data, kKeyAccessToken);
  *out_sid = GetDictString(registration_data, kKeySID);
  *out_username = GetDictString(registration_data, kKeyUsername);
  *out_domain = GetDictString(registration_data, kKeyDomain);

  if (out_email->empty()) {
    LOGFN(ERROR) << "Email is empty";
    return E_INVALIDARG;
  }

  if (out_id_token->empty()) {
    LOGFN(ERROR) << "MDM id token is empty";
    return E_INVALIDARG;
  }

  if (out_access_token->empty()) {
    LOGFN(ERROR) << "Access token is empty";
    return E_INVALIDARG;
  }

  if (out_sid->empty()) {
    LOGFN(ERROR) << "SID is empty";
    return E_INVALIDARG;
  }

  if (out_username->empty()) {
    LOGFN(ERROR) << "username is empty";
    return E_INVALIDARG;
  }

  if (out_domain->empty()) {
    LOGFN(ERROR) << "domain is empty";
    return E_INVALIDARG;
  }

  return S_OK;
}

HRESULT RegisterWithGoogleDeviceManagement(const base::string16& mdm_url,
                                           const base::Value& properties) {
  // Make sure all the needed data is present in the dictionary.
  base::string16 email;
  base::string16 id_token;
  base::string16 access_token;
  base::string16 sid;
  base::string16 username;
  base::string16 domain;

  HRESULT hr = ExtractRegistrationData(properties, &email, &id_token,
                                       &access_token, &sid, &username, &domain);

  if (FAILED(hr)) {
    LOGFN(ERROR) << "ExtractRegistrationData hr=" << putHR(hr);
    return E_INVALIDARG;
  }

  LOGFN(INFO) << "MDM_URL=" << mdm_url
              << " token=" << base::string16(id_token.c_str(), 10);

  switch (g_enrollment_status) {
    case EnrollmentStatus::kForceSuccess:
      return S_OK;
    case EnrollmentStatus::kForceFailure:
      return E_FAIL;
    case EnrollmentStatus::kDontForce:
      break;
  }

  // Add the serial number to the registration data dictionary.
  base::string16 serial_number =
      base::win::WmiComputerSystemInfo::Get().serial_number();

  if (serial_number.empty()) {
    LOGFN(ERROR) << "Failed to get serial number.";
    return E_FAIL;
  }

  // Build the json data needed by the server.
  base::Value registration_data(base::Value::Type::DICTIONARY);
  registration_data.SetStringKey("id_token", id_token);
  registration_data.SetStringKey("access_token", access_token);
  registration_data.SetStringKey("sid", sid);
  registration_data.SetStringKey("username", username);
  registration_data.SetStringKey("domain", domain);
  registration_data.SetStringKey("serial_number", serial_number);
  std::string registration_data_str;
  if (!base::JSONWriter::Write(registration_data, &registration_data_str)) {
    LOGFN(ERROR) << "JSONWriter::Write(registration_data)";
    return E_FAIL;
  }

  base::ScopedNativeLibrary library(
      base::FilePath(FILE_PATH_LITERAL("MDMRegistration.dll")));
  auto register_device_with_management_function =
      GET_MDM_FUNCTION_POINTER(library, RegisterDeviceWithManagement);
  if (!register_device_with_management_function) {
    LOGFN(ERROR) << "GET_MDM_FUNCTION_POINTER(RegisterDeviceWithManagement)";
    return false;
  }

  std::string data_encoded;
  base::Base64Encode(registration_data_str, &data_encoded);

  // This register call is blocking.  It won't return until the machine is
  // properly registered with the MDM server.
  return register_device_with_management_function(
      email.c_str(), mdm_url.c_str(), base::UTF8ToWide(data_encoded).c_str());
}

}  // namespace

bool NeedsToEnrollWithMdm() {
  base::string16 mdm_url = GetMdmUrl();
  return !mdm_url.empty() && !IsEnrolledWithGoogleMdm(mdm_url);
}

bool MdmEnrollmentEnabled() {
  base::string16 mdm_url = GetMdmUrl();
  return !mdm_url.empty();
}

HRESULT EnrollToGoogleMdmIfNeeded(const base::Value& properties) {
  LOGFN(INFO);

  // Only enroll with MDM if configured.
  base::string16 mdm_url = GetMdmUrl();
  if (mdm_url.empty())
    return S_OK;

  // TODO(crbug.com/935577): Check if machine is already enrolled because
  // attempting to enroll when already enrolled causes a crash.
  if (IsEnrolledWithGoogleMdm(mdm_url)) {
    LOGFN(INFO) << "Already enrolled to Google MDM";
    return S_OK;
  }

  HRESULT hr = RegisterWithGoogleDeviceManagement(mdm_url, properties);
  if (FAILED(hr))
    LOGFN(ERROR) << "RegisterWithGoogleDeviceManagement hr=" << putHR(hr);

  return hr;
}

// GoogleMdmEnrollmentStatusForTesting ////////////////////////////////////////

GoogleMdmEnrollmentStatusForTesting::GoogleMdmEnrollmentStatusForTesting(
    bool success) {
  g_enrollment_status = success ? EnrollmentStatus::kForceSuccess
                                : EnrollmentStatus::kForceFailure;
}

GoogleMdmEnrollmentStatusForTesting::~GoogleMdmEnrollmentStatusForTesting() {
  g_enrollment_status = EnrollmentStatus::kDontForce;
}

// GoogleMdmEnrolledStatusForTesting //////////////////////////////////////////

GoogleMdmEnrolledStatusForTesting::GoogleMdmEnrolledStatusForTesting(
    bool success) {
  g_enrolled_status =
      success ? EnrolledStatus::kForceTrue : EnrolledStatus::kForceFalse;
}

GoogleMdmEnrolledStatusForTesting::~GoogleMdmEnrolledStatusForTesting() {
  g_enrolled_status = EnrolledStatus::kDontForce;
}

}  // namespace credential_provider
