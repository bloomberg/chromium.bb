<?php
  header('Cache-Control: no-store');
  header('Content-Security-Policy: referrer origin-when-cross-origin');
  header('Content-Type: application/javascript');
?>

importScripts('/fetch/resources/fetch-test-helpers.js');

var BASE_URL = BASE_ORIGIN + '/fetch/resources/referrer.php';
var OTHER_URL = OTHER_ORIGIN + '/fetch/resources/referrer.php';
var REFERRER_SOURCE = location.href;

var TESTS = [
  [BASE_URL, 'about:client', REFERRER_SOURCE],
  [BASE_URL, '', '[no-referrer]'],
  [BASE_URL, '/foo', BASE_ORIGIN + '/foo'],
  [OTHER_URL, 'about:client', BASE_ORIGIN + '/'],
  [OTHER_URL, '', '[no-referrer]'],
  [OTHER_URL, '/foo', BASE_ORIGIN + '/'],
];

add_referrer_tests(TESTS);
done();

