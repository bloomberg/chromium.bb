(async function(testRunner) {
  var {page, session, dp} =
      await testRunner.startBlank(
          "Check that calling WebAuthn.enable starts the WebAuthn virtual " +
          "authenticator environment.");

  await page.navigate(
      "https://devtools.test:8443/inspector-protocol/webauthn/resources/create-credential-test.https.html");

  await dp.WebAuthn.enable();

  const result = await session.evaluateAsync("registerCredential()");
  testRunner.log(result);
  testRunner.completeTest();
})
