<?php
header("Suborigin: foobar");
?>
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<title>Verifies that localStorage and sessionStorage are accessible from within a suborigin and are different from the physical origin's localStorage and sessionStorage</title>
<script src="/resources/testharness.js"></script>
<script src="/resources/testharnessreport.js"></script>
</head>
<body>
<script>
var iframeWindow;
localStorage.clear();
sessionStorage.clear();

function make_storage_accessibility_test(storage) {
  return function(test) {
      assert_equals(storage.getItem('FOO'), null);
      storage.setItem('FOO', 'BAR');
      assert_equals(storage.getItem('FOO'), 'BAR');
      storage.removeItem('FOO');
      assert_equals(storage.getItem('FOO'), null);
      test.done();
    };
}

async_test(make_storage_accessibility_test(localStorage),
  'localStorage is accessible from a Suborigin');
async_test(make_storage_accessibility_test(sessionStorage),
  'sessionStorage is accessible from a Suborigin');

function make_xorigin_test(storage_name, item_name) {
  var storage = window[storage_name];
  return function(test) {
      window.addEventListener('message', function(event) {
        if (event.data != 'ready' && event.data.type == storage_name) {
          assert_equals(event.data.value, null);
          assert_equals(storage.getItem(item_name), null);
          test.done();
        }
      });
  };
}

var localStorageXOriginTest = async_test(
  make_xorigin_test('localStorage', 'LOCAL_FOO2'),
  'localStorage is set in a Suborigin is not accessible in the physical ' +
    'origin, or vice versa');
var sessionStorageXOriginTest = async_test(
  make_xorigin_test('sessionStorage', 'SESSION_FOO2'),
  'sessionStorage is set in a Suborigin is not accessible in the physical ' +
    'origin, or vice versa');

window.addEventListener('message', function(event) {
    // When the iframe states that it is ready to accept messages, start the
    // localStorage and sessionStorage tests by setting up the appropriate
    // store and messaging the iframe that the store is ready.
    //
    // When the iframe responds again, the appropriate tests have setup
    // listeners to check that the values in our storage have not changed and
    // that the iframe's storage values have also not changed.
    if (event.data == 'ready') {
      iframeWindow = document.getElementById('iframe').contentWindow;
      window.localStorage.setItem('LOCAL_FOO1', 'BAR');
      window.sessionStorage.setItem('SESSION_FOO1', 'BAR');
      iframeWindow.postMessage({ 'type': 'localStorage' }, '*');
      iframeWindow.postMessage({ 'type': 'sessionStorage' }, '*');
    }
  });
</script>
<iframe id="iframe" src="resources/access-storage.php"></iframe>
</body>
</html>
