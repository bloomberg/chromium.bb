importScripts('../../resources/testharness.js');

let echo_output = null;

// Tests importing a script that sets |echo_output| to the query string.
function test_import(str) {
  importScripts('echo.php?' + str);
  assert_equals(echo_output, str);
}

test_import('root');
test_import('root-and-message');

self.addEventListener('install', () => {
    test_import('install');
    test_import('install-and-message');
  });

self.addEventListener('message', e => {
    test_import('root-and-message');
    test_import('install-and-message');
    // TODO(falken): This should fail. The spec disallows importing a non-cached
    // script like this but currently Chrome and Firefox allow it.
    test_import('message');
    e.source.postMessage('OK');
  });
