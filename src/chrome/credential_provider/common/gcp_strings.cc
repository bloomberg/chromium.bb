// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/credential_provider/common/gcp_strings.h"

namespace credential_provider {

// Names of keys returned on json data from UI process.
const char kKeyEmail[] = "email";
const char kKeyPicture[] = "picture";
const char kKeyFullname[] = "full_name";
const char kKeyId[] = "id";
const char kKeyMdmUrl[] = "mdm_url";
const char kKeyMdmIdToken[] = "mdm_id_token";
const char kKeyPassword[] = "password";
const char kKeyRefreshToken[] = "refresh_token";
const char kKeyAccessToken[] = "access_token";
const char kKeySID[] = "sid";
const char kKeyTokenHandle[] = "token_handle";
const char kKeyUsername[] = "user_name";
const char kKeyDomain[] = "domain";
const char kKeyExitCode[] = "exit_code";

// Name of registry value that holds user properties.
const wchar_t kUserTokenHandle[] = L"th";
const wchar_t kUserEmail[] = L"email";
const wchar_t kUserId[] = L"id";
const wchar_t kUserPictureUrl[] = L"pic";

// Username and password key for special GAIA account to run GLS.
const wchar_t kDefaultGaiaAccountName[] = L"gaia";
// L$ prefix means this secret can only be accessed locally.
const wchar_t kLsaKeyGaiaUsername[] = L"L$GAIA_USERNAME";
const wchar_t kLsaKeyGaiaPassword[] = L"L$GAIA_PASSWORD";

// These two variables need to remain consistent.
const wchar_t kDesktopName[] = L"Winlogon";
const wchar_t kDesktopFullName[] = L"WinSta0\\Winlogon";

// Google Update related registry paths.
#define GCPW_UPDATE_CLIENT_GUID L"{32987697-A14E-4B89-84D6-630D5431E831}"

const wchar_t kRegUpdaterClientStateAppPath[] =
    L"SOFTWARE\\Google\\Update\\ClientState\\" GCPW_UPDATE_CLIENT_GUID;
const wchar_t kRegUpdaterClientsAppPath[] =
    L"SOFTWARE\\Google\\Update\\Clients\\" GCPW_UPDATE_CLIENT_GUID;

// Chrome is being opened to show the credential provider logon page.  This
// page is always shown in incognito mode.
const char kGcpwSigninSwitch[] = "gcpw-signin";

// The email to use to prefill the Gaia signin page.
const char kPrefillEmailSwitch[] = "prefill-email";

// Comma separated list of valid Gaia signin domains. If email that is signed
// into gaia is not part of these domains no LST will be minted and an error
// will be reported.
const char kEmailDomainsSwitch[] = "email-domains";

// Expected gaia-id of user that will be signing into gaia. If the ids do not
// match after signin, no LST will be minted and an error will be reported.
const char kGaiaIdSwitch[] = "gaia-id";

// Allows specification of the gaia endpoint to use to display the signin page
// for GCPW.
const char kGcpwEndpointPathSwitch[] = "gcpw-endpoint-path";

// Parameter appended to sign in URL to pass valid signin domains to the inline
// login handler. These domains are separated by ','.
const char kEmailDomainsSigninPromoParameter[] = "emailDomains";
const char kEmailDomainsSeparator[] = ",";
const char kValidateGaiaIdSigninPromoParameter[] = "validate_gaia_id";
const char kGcpwEndpointPathPromoParameter[] = "gcpw_endpoint_path";

const wchar_t kRunAsCrashpadHandlerEntryPoint[] = L"RunAsCrashpadHandler";

#if defined(GOOGLE_CHROME_BUILD)
const wchar_t kRegHkcuAccountsPath[] = L"Software\\Google\\Accounts";
#else
const wchar_t kRegHkcuAccountsPath[] = L"Software\\Chromium\\Accounts";
#endif  // defined(GOOGLE_CHROME_BUILD)

}  // namespace credential_provider
