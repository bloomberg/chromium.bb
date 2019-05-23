This test suite is for testing the CookiesWithoutSameSiteMustBeSecure feature,
which requires that cookies with SameSite=None also specify Secure, and rejects
any SameSite=None cookies that are not secure.
