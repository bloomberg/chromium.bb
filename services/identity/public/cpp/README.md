IdentityManager is the next-generation C++ API for interacting with Google
identity. It is backed by //components/signin.

Documentation on the mapping between usage of legacy signin
classes (notably SigninManager(Base) and ProfileOAuth2TokenService) and usage of
IdentityManager is available here:

https://docs.google.com/document/d/14f3qqkDM9IE4Ff_l6wuXvCMeHfSC9TxKezXTCyeaPUY/edit#

A quick inline cheat sheet for developers migrating from usage of //components/
signin and //google_apis/gaia:

- "Primary account" in IdentityManager refers to what is called the
  "authenticated account" in SigninManager, i.e., the account that has been
  blessed for sync by the user.
- PrimaryAccountTokenFetcher is the primary client-side interface for obtaining
  access tokens for the primary account. In particular, it can take care of 
  waiting until the primary account is available.
- AccessTokenFetcher is the client-side interface for obtaining access tokens
  for arbitrary accounts.
- IdentityTestEnvironment is the preferred test infrastructure for unittests
  of production code that interacts with IdentityManager. It is suitable for
  use in cases where neither the production code nor the unittest is interacting
  with Profile (e.g., //components-level unittests).
- identity_test_utils.h provides lower-level test facilities for interacting
  explicitly with IdentityManager and its dependencies (SigninManager,
  ProfileOAuth2TokenService). These facilities are the way to interact with
  IdentityManager in unittest contexts where the production code and/or the
  unittest are interacting with Profile (in particular, where the
  IdentityManager instance with which the test is interacting must be
  IdentityManagerFactory::GetForProfile(profile)).
