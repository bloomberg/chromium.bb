importScripts('worker-test-harness.js');

var URL = 'https://www.example.com/test.html';

test(function() {
    var headers = new Headers;
    headers.set('User-Agent', 'Mozilla/5.0');
    headers.set('Accept', 'text/html');
    headers.set('X-ServiceWorker-Test', 'request test field');

    var request = new Request(URL, {method: 'GET', headers: headers});

    assert_equals(request.url, URL, 'Request.url should match');
    assert_equals(request.method, 'GET', 'Request.method should match');
    assert_equals(request.referrer, location.href, 'Request.referrer should match');
    assert_true(request.headers instanceof Headers, 'Request.headers should be Headers');

    // 'User-Agent' is a forbidden header.
    assert_equals(request.headers.size, 2, 'Request.headers.size should match');
    // Note: detailed behavioral tests for Headers are in another test,
    // http/tests/serviceworker/headers.html.

    request.url = 'http://localhost/';
    assert_equals(request.url, 'https://www.example.com/test.html', 'Request.url should be readonly');
    request = new Request('http://localhost/\uD800'); // Unmatched lead surrogate.
    assert_equals(request.url,
                  'http://localhost/' + encodeURIComponent('\uFFFD'),
                  'Request.url should have unmatched surrogates replaced.');
    request.method = 'POST';
    assert_equals(request.method, 'GET', 'Request.method should be readonly');
}, 'Request basic test in ServiceWorkerGlobalScope');

test(function() {
    [new Request(URL),
     new Request(URL, {method: ''}),
     new Request(URL, {mode: ''}),
     new Request(URL, {mode: 'invalid mode'}),
     new Request(URL, {credentials: ''}),
     new Request(URL, {credentials: 'invalid credentials'})].forEach(function(request) {
        assert_equals(request.url, URL, 'Request.url should match');
        assert_equals(request.method, 'GET', 'Default Request.method should be GET');
        assert_equals(request.mode, 'cors', 'Default Request.mode should be cors');
        assert_equals(request.credentials, 'omit', 'Default Request.credentials should be omit');
    });
}, 'Request default value test in ServiceWorkerGlobalScope');

test(function() {
    var request = new Request(URL);
    request.headers.append('X-ServiceWorker-Foo', 'foo1');
    request.headers.append('X-ServiceWorker-Foo', 'foo2');
    request.headers.append('X-ServiceWorker-Bar', 'bar');
    var request2 = new Request(request);
    assert_equals(request2.url, URL, 'Request.url should match');
    assert_equals(request2.method, 'GET', 'Request.method should match');
    assert_equals(request2.mode, 'cors', 'Request.mode should match');
    assert_equals(request2.credentials, 'omit', 'Request.credentials should match');
    assert_equals(request2.headers.getAll('X-ServiceWorker-Foo')[0], 'foo1',
                  'Request.headers should match');
    assert_equals(request2.headers.getAll('X-ServiceWorker-Foo')[1], 'foo2',
                  'Request.headers should match');
    assert_equals(request2.headers.getAll('X-ServiceWorker-Bar')[0], 'bar',
                  'Request.headers should match');
}, 'Request header test in ServiceWorkerGlobalScope');

test(function() {
    var request1 = {};
    var request2 = {};
    var METHODS = ['GET', 'HEAD', 'POST', 'PUT', 'DELETE', 'OPTIONS', '', undefined];
    var MODES = ['same-origin', 'no-cors', 'cors', '', undefined];
    function isSimpleMethod(method) {
      return ['GET', 'HEAD', 'POST', '', undefined].indexOf(method) != -1;
    };
    function effectiveMethod(method1, method2) {
      return method2 ? method2 : (method1 ? method1 : 'GET');
    };
    function effectiveMode(mode1, mode2) {
      return mode2 ? mode2 : (mode1 ? mode1 : 'cors');
    };
    METHODS.forEach(function(method1) {
        MODES.forEach(function(mode1) {
            var init1 = {};
            if (method1 != undefined) { init1['method'] = method1; }
            if (mode1 != undefined) { init1['mode'] = mode1; }
            if (!isSimpleMethod(method1) && mode1 == 'no-cors') {
                assert_throws(
                    {name:'TypeError'},
                    function() { request1 = new Request(URL, init1); },
                    'new no-cors Request with non simple method (' + method1 +') should throw');
                return;
            }
            request1 = new Request(URL, init1);
            assert_equals(request1.method, method1 ? method1 : 'GET', 'Request.method should match');
            assert_equals(request1.mode, mode1 ? mode1 : 'cors', 'Request.mode should match');
            request1 = new Request(request1);
            assert_equals(request1.method, method1 ? method1 : 'GET', 'Request.method should match');
            assert_equals(request1.mode, mode1 ? mode1 : 'cors', 'Request.mode should match');
            METHODS.forEach(function(method2) {
                MODES.forEach(function(mode2) {
                    var init2 = {};
                    if (method2 != undefined) { init2['method'] = method2; }
                    if (mode2 != undefined) { init2['mode'] = mode2; }
                    if (!isSimpleMethod(effectiveMethod(method1, method2)) && effectiveMode(mode1, mode2) == 'no-cors') {
                        assert_throws(
                            {name:'TypeError'},
                            function() { request2 = new Request(request1, init2); },
                            'new no-cors Request with non simple method should throw');
                        return;
                    }
                    request2 = new Request(request1, init2);
                    assert_equals(request2.method,
                                  method2 ? method2 : request1.method,
                                  'Request.method should be overridden');
                    assert_equals(request2.mode,
                                  mode2 ? mode2 : request1.mode,
                                  'Request.mode should be overridden');
                });
            });
        });
    });
}, 'Request header test in ServiceWorkerGlobalScope');

test(function() {
    var request1 = {};
    var request2 = {};
    var CREDENTIALS = ['omit', 'same-origin', 'include', '', undefined];
    CREDENTIALS.forEach(function(credentials1) {
        var init1 = {};
        if (credentials1 != undefined) { init1['credentials'] = credentials1; }
        request1 = new Request(URL, init1);
        assert_equals(request1.credentials, credentials1 ? credentials1 : 'omit', 'Request.credentials should match');
        request1 = new Request(request1);
        assert_equals(request1.credentials, credentials1 ? credentials1 : 'omit', 'Request.credentials should match');
        CREDENTIALS.forEach(function(credentials2) {
            var init2 = {};
            if (credentials2 != undefined) { init2['credentials'] = credentials2; }
            request2 = new Request(request1, init2);
            assert_equals(request2.credentials,
                          credentials2 ? credentials2 : request1.credentials,
                          'Request.credentials should be overridden');
        });
    });
}, 'Request credentials test in ServiceWorkerGlobalScope');

test(function() {
    ['same-origin', 'cors', 'no-cors'].forEach(function(mode) {
        var forbiddenMethods = ['TRACE', 'TRACK', 'CONNECT'];
        forbiddenMethods.forEach(function(method) {
            assert_throws(
                {name:'TypeError'},
                function() { var request = new Request(URL, {mode: mode, method: method}); },
                'new Request with a forbidden method (' + method +') should throw');
        });
        var invalidNames = ['(', ')', '<', '>', '@', ',', ';', ':', '\\', '"',
                            '/', '[', ']', '?', '=', '{', '}', '\u3042', 'a(b',
                            'invalid name'];
        invalidNames.forEach(function(name) {
            assert_throws(
                {name:'TypeError'},
                function() { var request = new Request(URL, {mode: mode, method: name}); },
                'new Request with an invalid method (' + name +') should throw');
        });
    });
}, 'Request method name test in ServiceWorkerGlobalScope');

test(function() {
    var FORBIDDEN_HEADERS =
        ['Accept-Charset', 'Accept-Encoding', 'Access-Control-Request-Headers',
         'Access-Control-Request-Method', 'Connection', 'Content-Length', 'Cookie',
         'Cookie2', 'Date', 'DNT', 'Expect', 'Host', 'Keep-Alive', 'Origin',
         'Referer', 'TE', 'Trailer', 'Transfer-Encoding', 'Upgrade', 'User-Agent',
         'Via', 'Proxy-', 'Sec-', 'Proxy-FooBar', 'Sec-FooBar'];
    var SIMPLE_HEADERS =
        [['Accept', '*'], ['Accept-Language', 'ru'], ['Content-Language', 'ru'],
         ['Content-Type', 'application/x-www-form-urlencoded'],
         ['Content-Type', 'multipart/form-data'],
         ['Content-Type', 'text/plain']];
    var NON_SIMPLE_HEADERS =
        [['X-ServiceWorker-Test', 'test'],
         ['X-ServiceWorker-Test2', 'test2'],
         ['Content-Type', 'foo/bar']];

    ['same-origin', 'cors'].forEach(function(mode) {
        var request = new Request(URL, {mode: mode});
        FORBIDDEN_HEADERS.forEach(function(header) {
            request.headers.append(header, 'test');
            assert_equals(request.headers.size, 0,
                          'Request.headers.append should ignore the forbidden headers');
            request.headers.set(header, 'test');
            assert_equals(request.headers.size, 0,
                          'Request.headers.set should ignore the forbidden headers');
        });
        var request = new Request(URL, {mode: mode});
        assert_equals(request.headers.size, 0);
        NON_SIMPLE_HEADERS.forEach(function(header) {
            request.headers.append(header[0], header[1]);
        });
        assert_equals(request.headers.size, NON_SIMPLE_HEADERS.length);
        NON_SIMPLE_HEADERS.forEach(function(header) {
            assert_equals(request.headers.get(header[0]), header[1]);
        });
        request = new Request(URL, {mode: mode});
        assert_equals(request.headers.size, 0);
        NON_SIMPLE_HEADERS.forEach(function(header) {
            request.headers.set(header[0], header[1]);
        });
        assert_equals(request.headers.size, NON_SIMPLE_HEADERS.length);
        NON_SIMPLE_HEADERS.forEach(function(header) {
            assert_equals(request.headers.get(header[0]), header[1]);
        });
    });
    request = new Request(URL, {mode: 'no-cors'});
    FORBIDDEN_HEADERS.forEach(function(header) {
        request.headers.set(header, 'test');
        request.headers.append(header, 'test');
    });
    NON_SIMPLE_HEADERS.forEach(function(header) {
        request.headers.set(header[0], header[1]);
        request.headers.append(header[0], header[1]);
    });
    assert_equals(request.headers.size, 0,
                  'no-cors request should only accept simple headers');

    SIMPLE_HEADERS.forEach(function(header) {
        request = new Request(URL, {mode: 'no-cors'});
        request.headers.append(header[0], header[1]);
        assert_equals(request.headers.size, 1,
                      'no-cors request should accept simple headers');
        request = new Request(URL, {mode: 'no-cors'});
        request.headers.set(header[0], header[1]);
        assert_equals(request.headers.size, 1,
                      'no-cors request should accept simple headers');
        request.headers.delete(header[0]);
        if (header[0] == 'Content-Type') {
            assert_equals(
                request.headers.size, 1,
                'Content-Type header of no-cors request shouldn\'t be deleted');
        } else {
            assert_equals(request.headers.size, 0);
        }
    });

    SIMPLE_HEADERS.forEach(function(header) {
        var headers = {};
        NON_SIMPLE_HEADERS.forEach(function(header2) {
            headers[header2[0]] = header2[1];
        });
        FORBIDDEN_HEADERS.forEach(function(header) { headers[header] = 'foo'; });
        headers[header[0]] = header[1];
        var expectedSize = NON_SIMPLE_HEADERS.length;
        if (header[0] != 'Content-Type') {
            ++expectedSize;
        }
        ['same-origin', 'cors'].forEach(function(mode) {
          request = new Request(URL, {mode: mode, headers: headers});
          assert_equals(request.headers.size, expectedSize,
                        'Request should not support the forbidden headers');
        });
        request = new Request(URL, {mode: 'no-cors', headers: headers});
        assert_equals(request.headers.size, 1,
                      'No-CORS Request.headers should only support simple headers');
        ['same-origin', 'cors', 'no-cors'].forEach(function(mode) {
            request = new Request(new Request(URL, {mode: mode, headers: headers}), {mode: 'no-cors'});
            assert_equals(request.headers.size, 1,
                          'No-CORS Request.headers should only support simple headers');
        });
    });
}, 'Request headers test in ServiceWorkerGlobalScope');
