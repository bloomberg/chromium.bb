// Copyright (c) 2013 The Chromium Embedded Framework Authors. All rights
// reserved. Use of this source code is governed by a BSD-style license that
// can be found in the LICENSE file.

#include "libcef/common/net/scheme_registration.h"

#include "libcef/common/app_manager.h"
#include "libcef/common/net/scheme_info.h"
#include "libcef/features/runtime.h"

#include "base/stl_util.h"
#include "content/public/common/url_constants.h"
#include "extensions/common/constants.h"
#include "net/net_buildflags.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace scheme {

void AddInternalSchemes(content::ContentClient::Schemes* schemes) {
  if (!cef::IsAlloyRuntimeEnabled())
    return;

  // chrome: and chrome-devtools: schemes are registered in
  // RenderThreadImpl::RegisterSchemes().
  // Access restrictions for chrome-extension: and chrome-extension-resource:
  // schemes will be applied in AlloyContentRendererClient::WillSendRequest().
  static CefSchemeInfo internal_schemes[] = {
      {
          extensions::kExtensionScheme, true, /* is_standard */
          false,                              /* is_local */
          false,                              /* is_display_isolated */
          true,                               /* is_secure */
          true,                               /* is_cors_enabled */
          true,                               /* is_csp_bypassing */
      },
  };

  // The |is_display_isolated| value is excluded here because it's registered
  // with Blink only.
  for (size_t i = 0; i < sizeof(internal_schemes) / sizeof(internal_schemes[0]);
       ++i) {
    if (internal_schemes[i].is_standard)
      schemes->standard_schemes.push_back(internal_schemes[i].scheme_name);
    if (internal_schemes[i].is_local)
      schemes->local_schemes.push_back(internal_schemes[i].scheme_name);
    if (internal_schemes[i].is_secure)
      schemes->secure_schemes.push_back(internal_schemes[i].scheme_name);
    if (internal_schemes[i].is_cors_enabled)
      schemes->cors_enabled_schemes.push_back(internal_schemes[i].scheme_name);
    if (internal_schemes[i].is_csp_bypassing)
      schemes->csp_bypassing_schemes.push_back(internal_schemes[i].scheme_name);
    CefAppManager::Get()->AddCustomScheme(&internal_schemes[i]);
  }
}

bool IsInternalHandledScheme(const std::string& scheme) {
  static const char* schemes[] = {
    url::kAboutScheme,
    url::kBlobScheme,
    content::kChromeDevToolsScheme,
    content::kChromeUIScheme,
    url::kDataScheme,
    extensions::kExtensionScheme,
    url::kFileScheme,
    url::kFileSystemScheme,
#if !BUILDFLAG(DISABLE_FTP_SUPPORT)
    url::kFtpScheme,
#endif
    url::kHttpScheme,
    url::kHttpsScheme,
    url::kJavaScriptScheme,
    url::kWsScheme,
    url::kWssScheme,
  };

  for (size_t i = 0; i < sizeof(schemes) / sizeof(schemes[0]); ++i) {
    if (scheme == schemes[i])
      return true;
  }

  return false;
}

bool IsStandardScheme(const std::string& scheme) {
  url::Component scheme_comp(0, scheme.length());
  return url::IsStandard(scheme.c_str(), scheme_comp);
}

// Should return the same value as SecurityOrigin::isLocal and
// SchemeRegistry::shouldTreatURLSchemeAsCorsEnabled.
bool IsCorsEnabledScheme(const std::string& scheme) {
  return base::Contains(url::GetCorsEnabledSchemes(), scheme);
}

}  // namespace scheme
