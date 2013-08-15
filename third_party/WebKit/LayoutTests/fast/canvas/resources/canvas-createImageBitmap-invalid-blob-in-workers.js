importScripts('../../js/resources/js-test-pre.js');

self.jsTestIsAsync = true;

description('Test createImageBitmap with invalid blobs in workers.');

self.addEventListener('message', function(e) {
  createImageBitmap(e.data).then(function() {
    testFailed('Promise fulfuilled.');
    finishJSTest();
  }, function() {
    testPassed('Promise rejected.');
    finishJSTest();
  });
});