if (window.testRunner) {
  // In Chromium we need to change the setting to disallow displaying
  // insecure contents.
  testRunner.overridePreference('WebKitAllowRunningInsecureContent', false);
  //FIXME: to be removed.
  //testRunner.overridePreference('WebKitAllowDisplayingInsecureContent', true);

  // Accept all cookies.
  testRunner.setAlwaysAcceptCookies(true);
}

// How tests starts:
// 0. http://127.0.0.1:8000/.../X.html is loaded.
// 1. Login to HTTP pages.
//    This is done first from HTTP origin to avoid mixed content blocking.
// 2. Login to HTTPS pages.
// If the test is not base-https:
//   3a. Start tests.
// Otherwise:
//   3b. Redirect to https://127.0.0.1:8443/.../X.html.
//   4b. Start tests.

if (location.protocol != 'https:') {
  var test = async_test('Login');
  login(test) // Step 1. Login to HTTP page
    .then(function() {return login_https(test);}) // Step 2. Login to HTTPS page
    .then(function() {
        // Login done.
        if (location.pathname.includes('base-https')) {
          // Step 3b. For base-https tests, redirect to HTTPS page here.
          location = 'https://127.0.0.1:8443' + location.pathname;
        } else {
          // Step 3a. For non-base-https tests, start tests here.
          start();
          test.done();
        }
      });
} else {
  // Step 4b. Already redirected to the HTTPS page.
  // Start tests for base-https tests here.
  start();
}
