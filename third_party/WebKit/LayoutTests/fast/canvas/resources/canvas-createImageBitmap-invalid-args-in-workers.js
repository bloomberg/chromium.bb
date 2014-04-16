importScripts('../../../resources/js-test.js');

self.jsTestIsAsync = true;

description('Test createImageBitmap with invalid arguments in workers.');

var data;

self.addEventListener('message', function(e) {
  data = e.data;
  shouldThrow("createImageBitmap(null, 0, 0, 10, 10)");
  shouldThrow("createImageBitmap(data, 0, 0, 10, 0)");
  shouldThrow("createImageBitmap(data, 0, 0, 0, 10)");
  finishJSTest();
});
