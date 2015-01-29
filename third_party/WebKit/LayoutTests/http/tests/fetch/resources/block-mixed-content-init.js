if (window.testRunner) {
  // In Chromium we need to change the setting to disallow displaying insecure
  // contents.
  testRunner.overridePreference('WebKitAllowRunningInsecureContent', false);
}

if (location.protocol != 'https:')
    location = 'https://127.0.0.1:8443/' + location.pathname;
