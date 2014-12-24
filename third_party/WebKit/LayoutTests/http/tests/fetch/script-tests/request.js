if ('ServiceWorkerGlobalScope' in self &&
    self instanceof ServiceWorkerGlobalScope) {
  // ServiceWorker case
  importScripts('/serviceworker/resources/worker-testharness.js');
} else if (self.importScripts) {
  // Other workers cases
  importScripts('/resources/testharness.js');
}

test(function(test) {
    var request = new Request('');
    assert_equals(request.constructor, Request);
}, 'Create request');

test(function(test) {
    var response = new Response;
    assert_equals(response.constructor, Response);
}, 'Create response');

test(function(test) {
    var headers = new Headers;
    assert_equals(headers.constructor, Headers);
}, 'Create headers');

done();
