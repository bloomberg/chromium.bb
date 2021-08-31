// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_IS_UVPAA_H_
#define CONTENT_BROWSER_WEBAUTH_IS_UVPAA_H_

#include "base/callback.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "content/common/content_export.h"

#if defined(OS_MAC)
class BrowserContext;
#endif

namespace content {

// This file contains platform-specific implementations of the
// IsUserVerifyingPlatformAuthenticatorAvailable interface.

using IsUVPlatformAuthenticatorAvailableCallback =
    base::OnceCallback<void(bool is_available)>;

#if defined(OS_MAC)
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    BrowserContext* browser_context,
    IsUVPlatformAuthenticatorAvailableCallback);
#elif defined(OS_WIN)
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback);
#elif BUILDFLAG(IS_CHROMEOS_ASH)
CONTENT_EXPORT void IsUVPlatformAuthenticatorAvailable(
    IsUVPlatformAuthenticatorAvailableCallback);
#endif

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_IS_UVPAA_H_
