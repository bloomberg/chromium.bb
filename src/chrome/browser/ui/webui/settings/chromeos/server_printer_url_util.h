// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SERVER_PRINTER_URL_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SERVER_PRINTER_URL_UTIL_H_

#include <string>

#include "base/optional.h"

class GURL;

namespace chromeos {
namespace settings {

// Returns true if |gurl| has any the following scheme: HTTP, HTTPS, IPP, or
// IPPS. Returns false for an empty or any other scheme.
bool HasValidServerPrinterScheme(const GURL& gurl);

// Returns a GURL from the input |url|. Returns base::nullopt if
// either |url| is invalid or constructing the GURL failed. This will also
// default the server printer URI to use HTTPS if it detects a missing scheme.
base::Optional<GURL> GenerateServerPrinterUrlWithValidScheme(
    const std::string& url);

}  // namespace settings
}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_SETTINGS_CHROMEOS_SERVER_PRINTER_URL_UTIL_H_
