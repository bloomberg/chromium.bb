importScripts('worker-test-harness.js');
importScripts('test-helpers.js');

async_test(function(t) {
    fetch('http://')
      .then(
        unreached_rejection(t, 'fetch of invalid URL must fail'),
        function(e) {
          assert_equals(e.message, 'Invalid URL');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch invalid URL in ServiceWorkerGlobalScope');

async_test(function(t) {
    fetch('fetch-status.php?status=200')
      .then(function(response) {
          assert_equals(response.status, 200);
          assert_equals(response.statusText, 'OK');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch result of 200 response in ServiceWorkerGlobalScope');

async_test(function(t) {
    fetch('fetch-status.php?status=404')
      .then(function(response) {
          assert_equals(response.status, 404);
          assert_equals(response.statusText, 'Not Found');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch result of 404 response in ServiceWorkerGlobalScope');

test(function(t) {
    function runInfiniteFetchLoop() {
      fetch('dummy.html')
        .then(function() { runInfiniteFetchLoop(); });
    }
    runInfiniteFetchLoop();
  },
  'Destroying the execution context while fetch is happening should not ' +
      'cause a crash.');
