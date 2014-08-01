importScripts('worker-test-harness.js');

test(function() {
    var response = new Response(new Blob());
    assert_equals(response.type, 'default', 'Default Response.type should be \'default\'');
    assert_equals(response.url, '', 'Response.url should be the empty string');
    assert_equals(response.status, 200, 'Default Response.status should be 200');
    assert_equals(response.statusText, 'OK', 'Default Response.statusText should be \'OK\'');
    assert_equals(response.headers.size, 1, 'Default Response should have one header.');
    assert_equals(response.headers.get('content-type'), '', 'Default Response should have one header which name is \'content-type\' and value is an empty string.');

    response.status = 394;
    response.statusText = 'Sesame Street';
    assert_equals(response.status, 200, 'Response.status should be readonly');
    assert_equals(response.statusText, 'OK', 'Response.statusText should be readonly');
}, 'Response default value test in ServiceWorkerGlobalScope');

test(function() {
    var headers = new Headers;
    headers.set('Content-Language', 'ja');
    headers.set('Content-Type', 'text/html; charset=UTF-8');
    headers.set('X-ServiceWorker-Test', 'response test field');
    headers.set('Set-Cookie', 'response test set-cookie');
    headers.set('Set-Cookie2', 'response test set-cookie2');

    var responses =
        [new Response(new Blob(),
                      {status: 303, statusText: 'See Other', headers: headers}),
         new Response(new Blob(),
                      {
                          status: 303,
                          statusText: 'See Other',
                          headers: {'Content-Language': 'ja',
                                    'Content-Type': 'text/html; charset=UTF-8',
                                    'X-ServiceWorker-Test': 'response test field',
                                    'Set-Cookie': 'response test set-cookie',
                                    'Set-Cookie2': 'response test set-cookie2'}
                      }),
         new Response(new Blob(),
                      {
                          status: 303,
                          statusText: 'See Other',
                          headers: [['Content-Language', 'ja'],
                                    ['Content-Type', 'text/html; charset=UTF-8'],
                                    ['X-ServiceWorker-Test', 'response test field'],
                                    ['Set-Cookie', 'response test set-cookie'],
                                    ['Set-Cookie2', 'response test set-cookie2']]
                      })];
    responses.forEach(function(response) {
        assert_equals(response.status, 303, 'Response.status should match');
        assert_equals(response.statusText, 'See Other', 'Response.statusText should match');
        assert_true(response.headers instanceof Headers, 'Response.headers should be Headers');
        assert_equals(response.headers.size, 3, 'Response.headers.size should match');
        assert_equals(response.headers.get('Content-Language'), 'ja',
                      'Content-Language of Response.headers should match');
        assert_equals(response.headers.get('Content-Type'), 'text/html; charset=UTF-8',
                      'Content-Type of Response.headers should match');
        assert_equals(response.headers.get('X-ServiceWorker-Test'), 'response test field',
                      'X-ServiceWorker-Test of Response.headers should match');
        response.headers.set('X-ServiceWorker-Test2', 'response test field2');
        assert_equals(response.headers.size, 4, 'Response.headers.size should increase by 1.');
        assert_equals(response.headers.get('X-ServiceWorker-Test2'), 'response test field2',
                      'Response.headers should be added');
        response.headers.set('set-cookie', 'dummy');
        response.headers.set('sEt-cookie', 'dummy');
        response.headers.set('set-cookie2', 'dummy');
        response.headers.set('set-cOokie2', 'dummy');
        response.headers.append('set-cookie', 'dummy');
        response.headers.append('sEt-cookie', 'dummy');
        response.headers.append('set-cookie2', 'dummy');
        response.headers.append('set-cOokie2', 'dummy');
        assert_equals(response.headers.size, 4,
                      'Response.headers should not accept Set-Cookie nor Set-Cookie2');
        response.headers.delete('X-ServiceWorker-Test');
        assert_equals(response.headers.size, 3, 'Response.headers.size should decrease by 1.');
    });
    // Note: detailed behavioral tests for Headers are in another test,
    // http/tests/serviceworker/headers.html.
}, 'Response constructor test in ServiceWorkerGlobalScope');

test(function() {
    var response = new Response(new Blob(['dummy'], {type :'audio/wav'}));
    assert_equals(response.headers.size, 1, 'Response.headers should have Content-Type');
    assert_equals(response.headers.get('Content-Type'), 'audio/wav',
                  'Content-Type of Response.headers should be set');

    response = new Response(new Blob(['dummy'], {type :'audio/wav'}),
                            {headers:{'Content-Type': 'text/html; charset=UTF-8'}});
    assert_equals(response.headers.size, 1, 'Response.headers should have Content-Type');
    assert_equals(response.headers.get('Content-Type'), 'text/html; charset=UTF-8',
                  'Content-Type of Response.headers should be overridden');
}, 'Response content type test in ServiceWorkerGlobalScope');

test(function() {
    [0, 1, 100, 199, 600, 700].forEach(function(status) {
        assert_throws({name:'RangeError'},
                      function() { new Response(new Blob(), {status: status}); },
                      'new Response with status = ' + status + ' should throw');
    });
    [200, 300, 400, 500, 599].forEach(function(status) {
        var response = new Response(new Blob(), {status: status});
        assert_equals(response.status, status, 'Response.status should match');
    });

    var invalidNames = ['', '(', ')', '<', '>', '@', ',', ';', ':', '\\', '"',
                        '/', '[', ']', '?', '=', '{', '}', '\u3042', 'a(b'];
    invalidNames.forEach(function(name) {
        assert_throws({name:'TypeError'},
                      function() {
                          var obj = {};
                          obj[name] = 'a';
                          new Response(new Blob(), {headers: obj});
                      },
                      'new Response with headers with an invalid name (' + name +') should throw');
        assert_throws({name:'TypeError'},
                      function() {
                          new Response(new Blob(), {headers: [[name, 'a']]});
                      },
                      'new Response with headers with an invalid name (' + name +') should throw');
    });
    var invalidValues = ['test \r data', 'test \n data'];
    invalidValues.forEach(function(value) {
        assert_throws({name:'TypeError'},
                      function() {
                          new Response(new Blob(),
                                       {headers: {'X-ServiceWorker-Test': value}});
                      },
                      'new Response with headers with an invalid value should throw');
        assert_throws({name:'TypeError'},
                      function() {
                          new Response(new Blob(),
                                       {headers: [['X-ServiceWorker-Test', value]]});
                      },
                      'new Response with headers with an invalid value should throw');
    });
}, 'Response throw error test in ServiceWorkerGlobalScope');
