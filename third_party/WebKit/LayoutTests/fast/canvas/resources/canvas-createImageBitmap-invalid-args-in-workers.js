importScripts('../../js/resources/js-test-pre.js');

var IndexSizeError = "IndexSizeError: Index or size was negative, or greater than the allowed value.";

self.jsTestIsAsync = true;

description('Test createImageBitmap with invalid arguments in workers.');

var data;

self.addEventListener('message', function(e) {
  data = e.data;
  shouldThrow("createImageBitmap(data, 0, 0, 10, 0)", "IndexSizeError");
  shouldThrow("createImageBitmap(data, 0, 0, 0, 10)", "IndexSizeError");
  finishJSTest();
});