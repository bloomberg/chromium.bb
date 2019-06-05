(async function(testRunner) {
  var {page, session, dp} =
      await testRunner.startURL(
          "https://devtools.test:8443/inspector-protocol/webauthn/resources/create-credential-test.https.html",
          "Check that calling WebAuthn.enable starts the WebAuthn virtual " +
          "authenticator environment.");

  await dp.WebAuthn.enable();

  const result = JSON.parse(await session.evaluateAsync("registerCredential()"));
  testRunner.log(result.status);
  testRunner.completeTest();
})
