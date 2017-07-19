(async function(testRunner) {
  let {page, session, dp} = await testRunner.startBlank(
      `Tests interception blocking, modification of network fetches.`);

  var InterceptionHelper = await testRunner.loadScript('../resources/interception-test.js');
  var helper = new InterceptionHelper(testRunner, session);

  var numRequests = 0;
  var maybeDisableRequestInterception = function(event) {
    numRequests++;
    // To make this test non-flaky wait until the first three requests have
    // been made before disabling.  We can't wait for all for because the
    // scripts are blocking.
    if (numRequests === 3)
      helper.disableRequestInterception(event);
  };

  var requestInterceptedDict = {
      'resource-iframe.html': event => helper.allowRequest(event),
      'i-dont-exist.css': maybeDisableRequestInterception,
      'script.js': maybeDisableRequestInterception,
      'script2.js': maybeDisableRequestInterception,
      'post-echo.pl': maybeDisableRequestInterception,
  };

  await helper.startInterceptionTest(requestInterceptedDict, 1);
  session.evaluate(`
    var iframe = document.createElement('iframe');
    iframe.src = '${testRunner.url('./resources/resource-iframe.html')}';
    document.body.appendChild(iframe);
  `);
})
