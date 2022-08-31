// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELEVATION_SERVICE_ELEVATOR_H_
#define CHROME_ELEVATION_SERVICE_ELEVATOR_H_

#include <string>

#include <windows.h>

#include <wrl/implements.h>
#include <wrl/module.h>

#include "base/gtest_prod_util.h"
#include "chrome/elevation_service/elevation_service_idl.h"

namespace elevation_service {

class Elevator
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IElevator,
          IElevatorChromium,
          IElevatorChrome,
          IElevatorChromeBeta,
          IElevatorChromeDev,
          IElevatorChromeCanary> {
 public:
  const HRESULT kErrorCouldNotObtainCallingProcess =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA001);
  const HRESULT kErrorCouldNotGenerateValidationData =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA002);
  const HRESULT kErrorCouldNotDecryptWithUserContext =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA003);
  const HRESULT kErrorCouldNotDecryptWithSystemContext =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA004);
  const HRESULT kErrorCouldNotEncryptWithUserContext =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA005);
  const HRESULT kErrorCouldNotEncryptWithSystemContext =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA006);
  const HRESULT kValidationDidNotPass =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA007);

  Elevator() = default;

  Elevator(const Elevator&) = delete;
  Elevator& operator=(const Elevator&) = delete;

  // Securely validates and runs the provided Chrome Recovery CRX elevated, by
  // first copying the CRX to a secure directory under %ProgramFiles% to
  // validate and unpack the CRX.
  IFACEMETHODIMP RunRecoveryCRXElevated(const wchar_t* crx_path,
                                        const wchar_t* browser_appid,
                                        const wchar_t* browser_version,
                                        const wchar_t* session_id,
                                        DWORD caller_proc_id,
                                        ULONG_PTR* proc_handle) override;
  IFACEMETHODIMP EncryptData(ProtectionLevel protection_level,
                             const BSTR plaintext,
                             BSTR* ciphertext,
                             DWORD* last_error) override;
  IFACEMETHODIMP DecryptData(const BSTR ciphertext,
                             BSTR* plaintext,
                             DWORD* last_error) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ElevatorTest, StringHandlingTest);
  ~Elevator() override = default;

  // Appends a string `to_append` to an existing string `base`, first the four
  // byte length then the string.
  static void AppendStringWithLength(const std::string& to_append,
                                     std::string& base);

  // Pulls a string from the start of the string, `str` is shortened to the
  // remainder of the string. `str` should have been a string previously passed
  // to AppendStringWithLength, so that it contains the four byte prefix of
  // length.
  static std::string PopFromStringFront(std::string& str);
};

}  // namespace elevation_service

#endif  // CHROME_ELEVATION_SERVICE_ELEVATOR_H_
