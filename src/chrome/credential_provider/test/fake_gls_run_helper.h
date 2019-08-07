// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_CREDENTIAL_PROVIDER_TEST_FAKE_GLS_RUN_HELPER_H_
#define CHROME_CREDENTIAL_PROVIDER_TEST_FAKE_GLS_RUN_HELPER_H_

#include <atlcomcli.h>

#include "base/strings/string16.h"
#include "chrome/credential_provider/common/gcp_strings.h"

struct ICredentialProviderCredential;

namespace base {
class CommandLine;
}

namespace credential_provider {

class FakeOSUserManager;

namespace testing {

extern const char kDefaultEmail[];
extern const char kDefaultGaiaId[];
extern const wchar_t kDefaultUsername[];

// Helper class used to run and wait for a fake GLS process to validate the
// functionality of a GCPW credential.
class FakeGlsRunHelper {
 public:
  explicit FakeGlsRunHelper(FakeOSUserManager* fake_os_user_manager);
  ~FakeGlsRunHelper();

  HRESULT StartLogonProcess(ICredentialProviderCredential* cred, bool succeeds);
  HRESULT WaitForLogonProcess(ICredentialProviderCredential* cred);
  HRESULT StartLogonProcessAndWait(ICredentialProviderCredential* cred);

  // Gets a command line that runs a fake GLS that produces the desired output.
  // |default_exit_code| is the default value that will be written unless the
  // other command line arguments require a specific error code to be returned.
  static HRESULT GetFakeGlsCommandline(
      UiExitCodes default_exit_code,
      const std::string& gls_email,
      const std::string& gaia_id_override,
      const base::string16& start_gls_event_name,
      base::CommandLine* command_line);
};

}  // namespace testing

}  // namespace credential_provider

#endif  // CHROME_CREDENTIAL_PROVIDER_TEST_FAKE_GLS_RUN_HELPER_H_
