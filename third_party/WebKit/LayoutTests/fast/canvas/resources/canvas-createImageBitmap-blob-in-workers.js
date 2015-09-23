importScripts('../../../resources/js-test.js');

self.jsTestIsAsync = true;

description('Test createImageBitmap with blob in workers.');

self.addEventListener('message', function(e) {
  createImageBitmap(e.data).then(function() {
    testPassed('Promise fulfuilled.');
    finishJSTest();
  }, function() {
    testFailed('Promise rejected.');
    finishJSTest();
  });
});