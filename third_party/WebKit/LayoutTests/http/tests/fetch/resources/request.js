var global = this;
if (global.importScripts) {
    // Worker case
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

if (global.importScripts) {
    // Worker case
    done();
}
