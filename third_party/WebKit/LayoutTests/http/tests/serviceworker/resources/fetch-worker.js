importScripts('worker-testharness.js');
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

function evalJsonp(text) {
  return new Promise(function(resolve) {
      var report = resolve;
      // text must contain report() call.
      eval(text);
    });
}

async_test(function(t) {
    var request =
      new Request('fetch-access-control.php',
                  {
                    method: 'POST',
                    body: new Blob(['Test Blob'], {type: 'test/type'})
                  });
    fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.body, 'Test Blob');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch with Blob body test in ServiceWorkerGlobalScope');

async_test(function(t) {
    var request = new Request('fetch-access-control.php',
                              {method: 'POST', body: 'Test String'});
    fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.body, 'Test String');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch with string body test in ServiceWorkerGlobalScope');

async_test(function(t) {
    var text = "Test ArrayBuffer";
    var array = new Uint8Array(text.length);
    for (var i = 0; i < text.length; ++i)
      array[i] = text.charCodeAt(i);
    var request = new Request('fetch-access-control.php',
                              {method: 'POST', body: array.buffer});
    fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.body, 'Test ArrayBuffer');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch with ArrayBuffer body test in ServiceWorkerGlobalScope');

async_test(function(t) {
    var text = "Test ArrayBufferView";
    var array = new Uint8Array(text.length);
    for (var i = 0; i < text.length; ++i)
      array[i] = text.charCodeAt(i);
    var request = new Request('fetch-access-control.php',
                              {method: 'POST', body: array});
    fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.body, 'Test ArrayBufferView');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch with ArrayBufferView body test in ServiceWorkerGlobalScope');

async_test(function(t) {
    var formData = new FormData();
    formData.append('StringKey1', '1234567890');
    formData.append('StringKey2', 'ABCDEFGHIJ');
    formData.append('BlobKey', new Blob(['blob content']));
    formData.append('FileKey',
                    new File(['file content'], 'file.dat'));
    var request = new Request('fetch-access-control.php',
                          {method: 'POST', body: formData});
    fetch(request)
      .then(function(response) { return response.text(); })
      .then(evalJsonp)
      .then(function(result) {
          assert_equals(result.method, 'POST');
          assert_equals(result.post['StringKey1'], '1234567890');
          assert_equals(result.post['StringKey2'], 'ABCDEFGHIJ');
          var files = [];
          for (var i = 0; i < result.files.length; ++i) {
            files[result.files[i].key] = result.files[i];
          }
          assert_equals(files['BlobKey'].content, 'blob content');
          assert_equals(files['BlobKey'].name, 'blob');
          assert_equals(files['BlobKey'].size, 12);
          assert_equals(files['FileKey'].content, 'file content');
          assert_equals(files['FileKey'].name, 'file.dat');
          assert_equals(files['FileKey'].size, 12);
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch with FormData body test in ServiceWorkerGlobalScope');

test(function(t) {
    function runInfiniteFetchLoop() {
      fetch('dummy.html')
        .then(function() { runInfiniteFetchLoop(); });
    }
    runInfiniteFetchLoop();
  },
  'Destroying the execution context while fetch is happening should not ' +
      'cause a crash.');
