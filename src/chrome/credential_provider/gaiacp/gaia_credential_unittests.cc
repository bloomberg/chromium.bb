// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atlbase.h>
#include <atlcom.h>
#include <atlcomcli.h>

#include "chrome/credential_provider/gaiacp/gaia_credential.h"
#include "chrome/credential_provider/gaiacp/gaia_credential_provider_i.h"
#include "chrome/credential_provider/test/gcp_fakes.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace credential_provider {

class GcpGaiaCredentialTest : public ::testing::Test {
 private:
  FakeOSUserManager fake_os_user_manager_;
  FakeScopedLsaPolicyFactory fake_scoped_lsa_policy_factory_;
};

TEST_F(GcpGaiaCredentialTest, FinishAuthentication) {
  CComPtr<IGaiaCredential> cred;
  ASSERT_EQ(S_OK, CComCreator<CComObject<CGaiaCredential>>::CreateInstance(
                      nullptr, IID_IGaiaCredential, (void**)&cred));
  ASSERT_TRUE(!!cred);

  CComBSTR sid;
  CComBSTR error;
  ASSERT_EQ(S_OK, cred->FinishAuthentication(CComBSTR(W2COLE(L"username")),
                                             CComBSTR(W2COLE(L"password")),
                                             CComBSTR(W2COLE(L"Full Name")),
                                             &sid, &error));
  sid.Empty();
  error.Empty();

  // Finishing with the same username should fail.
  // TODO(rogerta): Will want to allow this at some point.
  ASSERT_EQ(HRESULT_FROM_WIN32(NERR_UserExists),
            cred->FinishAuthentication(
                CComBSTR(W2COLE(L"username")), CComBSTR(W2COLE(L"password")),
                CComBSTR(W2COLE(L"Full Name")), &sid, &error));
}

}  // namespace credential_provider
