'use strict';
if (self.importScripts) {
  importScripts('/fetch/resources/fetch-test-helpers.js');
}

var BASE_URL = BASE_ORIGIN + '/fetch/resources/referrer.php';
var OTHER_URL = OTHER_ORIGIN + '/fetch/resources/referrer.php';
var referrer_source = location.href;

// Currently we cannot set request's referrer policy and the default policy
// is used if no CSP is set to the context.
// But the default policy is equivalent to "no referrer when downgrade" and
// we cannot test the case with fetch() (without serviceworker).
var TESTS = [
  [BASE_URL, 'about:client', referrer_source],
  [BASE_URL, '', '[no-referrer]'],
  [BASE_URL, '/foo', BASE_ORIGIN + '/foo'],
  [OTHER_URL, 'about:client', referrer_source],
  [OTHER_URL, '', '[no-referrer]'],
  [OTHER_URL, '/foo', BASE_ORIGIN + '/foo'],
  [BASE_URL,
    (BASE_URL + '/path#fragment?query#hash').replace('//', '//user:pass@'),
    BASE_URL + '/path'],
];

add_referrer_tests(TESTS);
done();
