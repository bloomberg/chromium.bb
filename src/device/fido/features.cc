// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/features.h"

#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "url/origin.h"

namespace device {

#if BUILDFLAG(IS_WIN)
const base::Feature kWebAuthUseNativeWinApi{"WebAuthenticationUseNativeWinApi",
                                            base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_WIN)

extern const base::Feature kWebAuthCableSecondFactor {
  "WebAuthenticationCableSecondFactor",
// TODO(crbug.com/1052397): Revisit the macro expression once build flag switch
// of lacros-chrome is complete.
// If updating this, also update kWebAuthCableServerLink.
#if BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_LINUX)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

extern const base::Feature kWebAuthPhoneSupport{
    "WebAuthenticationPhoneSupport", base::FEATURE_DISABLED_BY_DEFAULT};

extern const base::Feature kWebAuthCableExtensionAnywhere{
    "WebAuthenticationCableExtensionAnywhere",
    base::FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(IS_CHROMEOS_ASH)
const base::Feature kWebAuthCrosPlatformAuthenticator{
    "WebAuthenticationCrosPlatformAuthenticator",
    base::FEATURE_ENABLED_BY_DEFAULT};
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

extern const base::Feature kU2fPermissionPrompt{
    "U2fPermissionPrompt", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace device
