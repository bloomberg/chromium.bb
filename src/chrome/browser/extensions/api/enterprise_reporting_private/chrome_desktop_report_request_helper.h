// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CHROME_DESKTOP_REPORT_REQUEST_HELPER_H_
#define CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CHROME_DESKTOP_REPORT_REQUEST_HELPER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"

namespace extensions {

// Result of the retrieve operation
enum class RetrieveDeviceDataStatus {
  // The operation finished successfully.
  kSuccess,
  // The path for device data can not be identified.
  kDataDirectoryUnknown,
  // The requested device data record does not exist.
  kDataRecordNotFound,
  // The requested device data record can not be read.
  kDataRecordRetrievalError,
};

// Override the path where Endpoint Verification data is stored for tests.
void OverrideEndpointVerificationDirForTesting(const base::FilePath& path);

// Store the |data| associated with the identifier |id|. Calls |callback| on
// completion with true on success.
void StoreDeviceData(const std::string& id,
                     const std::unique_ptr<std::vector<uint8_t>> data,
                     base::OnceCallback<void(bool)> callback);

// Retrieves the data associated with the identifier |id|. Calls |callback| on
// completion with the data retrieved if the second parameter is true.
void RetrieveDeviceData(
    const std::string& id,
    base::OnceCallback<void(const std::string&, RetrieveDeviceDataStatus)>
        callback);

// Get the Endpoint Verification secret (symmetric key) for this system. If no
// password exists in the Registry then one is generated, stored in the
// Registry, and returned. If the |force_recreate| flag is set to true then
// the secret is also recreated on any error upon retrieval.
// If one exists then it is fetched from the Registry and returned.
// If an error occurs then the second parameter is false.
void RetrieveDeviceSecret(
    bool force_recreate,
    base::OnceCallback<void(const std::string&, long int)> callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_ENTERPRISE_REPORTING_PRIVATE_CHROME_DESKTOP_REPORT_REQUEST_HELPER_H_
