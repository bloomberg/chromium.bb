importScripts('worker-test-helpers.js');

test(function() {
    var headers = new HeaderMap;
    headers.set('Content-Language', 'ja');
    headers.set('Content-Type', 'text/html; charset=UTF-8');
    headers.set('X-ServiceWorker-Test', 'response test field');

    var response = new Response(new Blob(), {
        status: 303,
        statusText: 'See Other',
        headers: headers
    });

    assert_equals(response.status, 303, 'Response.status should match');
    assert_equals(response.statusText, 'See Other', 'Response.statusText should match');
    assert_true(response.headers instanceof HeaderMap, 'Response.headers should be HeaderMap');
    assert_equals(response.headers.size, 3, 'Response.headers.size should match');
    // Note: detailed behavioral tests for HeaderMap are in another test,
    // http/tests/serviceworker/headermap.html.

    response.status = 123;
    response.statusText = 'Sesame Street';
    assert_equals(response.status, 123, 'Response.status should be writable');
    assert_equals(response.statusText, 'Sesame Street', 'Response.statusText should be writable');

    assert_throws({name:'TypeError'}, function() { response.statusText = 'invalid \u0100'; },
                  'Response.statusText should throw on invalid ByteString');

}, 'Response in ServiceWorkerGlobalScope');
