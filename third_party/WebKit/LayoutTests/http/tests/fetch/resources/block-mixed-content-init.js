if (window.testRunner) {
  // In Chromium we need to change the setting to disallow displaying insecure
  // contents.

  // By default, LayoutTest content_shell returns allowed for
  // "Should fetching request be blocked as mixed content?" at Step 5:
  // https://w3c.github.io/webappsec/specs/mixedcontent/#should-block-fetch
  // > If the user agent has been instructed to allow mixed content
  // Turning the switch below to false makes this condition above false, and
  // make content_shell to run into Step 7 to test mixed content blocking.
  testRunner.overridePreference('WebKitAllowRunningInsecureContent', false);
}

if (location.protocol != 'https:')
    location = 'https://127.0.0.1:8443' + location.pathname;
