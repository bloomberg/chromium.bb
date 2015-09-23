importScripts('../../../resources/js-test.js');

self.jsTestIsAsync = true;

description('Test createImageBitmap with data in workers.');

self.addEventListener('message', function(e) {
  createImageBitmap(e.data).then(function() {
    testPassed('Promise fulfilled.');
    finishJSTest();
  }, function() {
    testFailed('Promise rejected.');
    finishJSTest();
  });
});