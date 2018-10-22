// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_COMMON_CLOUD_PRINT_UTILS_H_
#define CLOUD_PRINT_COMMON_CLOUD_PRINT_UTILS_H_

#include <wtypes.h>

#include "base/strings/string16.h"

namespace cloud_print {

// Similar to the Windows API call GetLastError but returns an HRESULT.
HRESULT GetLastHResult();

// Convert an HRESULT to a localized string.
base::string16 GetErrorMessage(HRESULT hr);

// Retrieves a string from the string table of the module that contains the
// calling code.
base::string16 LoadLocalString(DWORD id);

// Sets registry value to notify Google Update that product was used.
void SetGoogleUpdateUsage(const base::string16& product_id);

}  // namespace cloud_print

#endif  // CLOUD_PRINT_COMMON_CLOUD_PRINT_UTILS_H_
