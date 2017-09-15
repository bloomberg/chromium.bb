// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkUtils_h
#define NetworkUtils_h

#include "platform/PlatformExport.h"
#include "platform/wtf/Forward.h"

namespace blink {

class KURL;
class SharedBuffer;
class ResourceResponse;

namespace NetworkUtils {

enum PrivateRegistryFilter {
  kIncludePrivateRegistries,
  kExcludePrivateRegistries,
};

PLATFORM_EXPORT bool IsReservedIPAddress(const String& host);

PLATFORM_EXPORT bool IsLocalHostname(const String& host, bool* is_local6);

PLATFORM_EXPORT String GetDomainAndRegistry(const String& host,
                                            PrivateRegistryFilter);

// Returns the decoded data url as ResourceResponse and SharedBuffer
// if url had a supported mimetype and parsing was successful.
PLATFORM_EXPORT RefPtr<SharedBuffer> ParseDataURLAndPopulateResponse(
    const KURL&,
    ResourceResponse&);

// Returns true if the URL is a data URL and its MIME type is in the list of
// supported/recognized MIME types.
PLATFORM_EXPORT bool IsDataURLMimeTypeSupported(const KURL&);

PLATFORM_EXPORT bool IsRedirectResponseCode(int);

}  // NetworkUtils

}  // namespace blink

#endif  // NetworkUtils_h
