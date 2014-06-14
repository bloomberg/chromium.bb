importScripts('worker-test-helpers.js');

test(function() {
    var headers = new HeaderMap;
    headers.set('User-Agent', 'Mozilla/5.0');
    headers.set('Accept', 'text/html');
    headers.set('X-ServiceWorker-Test', 'request test field');

    var request = new Request({
        url: 'https://www.example.com/test.html',
        method: 'GET',
        headers: headers
    });

    assert_equals(request.url, 'https://www.example.com/test.html', 'Request.url should match');
    assert_equals(request.method, 'GET', 'Request.method should match');
    assert_equals(request.origin, 'https://www.example.com', 'Request.origin should match');
    assert_true(request.headers instanceof HeaderMap, 'Request.headers should be HeaderMap');
    assert_equals(request.headers.size, 3, 'Request.headers.size should match');
    // Note: detailed behavioral tests for HeaderMap are in another test,
    // http/tests/serviceworker/headermap.html.

    request.url = 'http://localhost/';
    assert_equals(request.url, 'http://localhost/', 'Request.url should be writable');
    request.method = 'POST';
    assert_equals(request.method, 'POST', 'Request.method should be writable');
    assert_throws({name: 'TypeError'}, function() { request.method = 'invalid \u0100'; },
                  'Request.method should throw on invalid ByteString');

}, 'Request in ServiceWorkerGlobalScope');
