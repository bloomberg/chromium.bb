(async function(testRunner) {
  const {page, session, dp} = await testRunner.startBlank(
      `Test that permissions could be granted`);

  // Reset all permissions initially.
  await dp.Browser.resetPermissions();

  await page.navigate('http://devtools.test:8000/inspector-protocol/resources/empty.html');

  // Start listening for geolocation changes.
  await session.evaluateAsync(async () => {
    window.subscriptionChanges = [];
    const result = await navigator.permissions.query({name: 'geolocation'});
    window.subscriptionChanges.push(`INITIAL '${name}': ${result.state}`);
    result.onchange = () => window.subscriptionChanges.push(`CHANGED 'geolocation': ${result.state}`);
  });

  await Promise.all([
    grant('geolocation'),
    waitPermission('geolocation', 'granted')
  ]);

  await Promise.all([
    grant('audioCapture'),
    waitPermission('geolocation', 'denied'),
    waitPermission('microphone', 'granted'),
  ]);

  await Promise.all([
    grant('geolocation', 'audioCapture'),
    waitPermission('geolocation', 'granted'),
    waitPermission('microphone', 'granted'),
  ]);

  await grant('eee');

  testRunner.log('- Resetting all permissions'),
  await Promise.all([
    dp.Browser.resetPermissions(),
    waitPermission('geolocation', 'denied'),
    waitPermission('microphone', 'denied'),
  ]);

  testRunner.log(await session.evaluate(() => window.subscriptionChanges));

  testRunner.completeTest();

  async function grant(...permissions) {
    const response = await dp.Browser.grantPermissions({
      origin: 'http://devtools.test:8000',
      permissions
    });
    if (response.error)
      testRunner.log('- Failed to grant: ' + JSON.stringify(permissions) + '  error: ' + response.error.message);
    else
      testRunner.log('- Granted: ' + JSON.stringify(permissions));
  }

  async function waitPermission(name, state) {
    await session.evaluateAsync(async (name, state) => {
      const result = await navigator.permissions.query({name});
      if (result.state === state)
        return;
      return new Promise(resolve => {
        result.onchange = () => {
          if (result.state === state)
            resolve();
        }
      });
    });
  }
})

