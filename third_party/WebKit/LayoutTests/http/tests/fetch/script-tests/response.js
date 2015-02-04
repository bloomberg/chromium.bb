if (self.importScripts) {
  importScripts('../resources/fetch-test-helpers.js');
}

function size(headers) {
  var count = 0;
  for (var header of headers) {
    ++count;
  }
  return count;
}

test(function() {
    var response = new Response(new Blob());
    assert_equals(response.type, 'default',
                  'Default Response.type should be \'default\'');
    assert_equals(response.url, '', 'Response.url should be the empty string');
    assert_equals(response.status, 200,
                  'Default Response.status should be 200');
    assert_true(response.ok, 'Default Response.ok must be true');
    assert_equals(response.statusText, 'OK',
                  'Default Response.statusText should be \'OK\'');
    assert_equals(size(response.headers), 0,
                  'Default Response should not have any header.');

    response.status = 394;
    response.statusText = 'Sesame Street';
    assert_equals(response.status, 200, 'Response.status should be readonly');
    assert_true(response.ok, 'Response.ok must remain unchanged ' +
                             'when Response.status is attempted ' +
                             'unsuccessfully to change');
    assert_equals(response.statusText, 'OK',
                  'Response.statusText should be readonly');
    response.ok = false;
    assert_true(response.ok, 'Response.ok must be readonly');
  }, 'Response default value test');

test(function() {
    var headers = new Headers;
    headers.set('Content-Language', 'ja');
    headers.set('Content-Type', 'text/html; charset=UTF-8');
    headers.set('X-Fetch-Test', 'response test field');
    // Accept-Encoding is a forbidden header name but not a forbidden
    // response header name, and therefore included in the header.
    headers.set('Accept-Encoding', 'forbidden header name test');
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
                         'X-Fetch-Test': 'response test field',
                         'Accept-Encoding': 'forbidden header name test',
                         'Set-Cookie': 'response test set-cookie',
                         'Set-Cookie2': 'response test set-cookie2'}
                     }),
        new Response(new Blob(),
                     {
                       status: 303,
                       statusText: 'See Other',
                       headers: [['Content-Language', 'ja'],
                                 ['Content-Type', 'text/html; charset=UTF-8'],
                                 ['X-Fetch-Test',
                                  'response test field'],
                                 ['Accept-Encoding',
                                  'forbidden header name test'],
                                 ['Set-Cookie', 'response test set-cookie'],
                                 ['Set-Cookie2', 'response test set-cookie2']]
                     })];
    responses.forEach(function(response) {
        assert_equals(response.status, 303, 'Response.status should match');
        assert_false(response.ok, 'Response.ok must be false for 303');
        assert_equals(response.statusText, 'See Other',
                      'Response.statusText should match');
        assert_true(response.headers instanceof Headers,
                    'Response.headers should be Headers');
        assert_equals(size(response.headers), 4,
                      'Response.headers size should match');
        assert_equals(response.headers.get('Content-Language'), 'ja',
                      'Content-Language of Response.headers should match');
        assert_equals(response.headers.get('Content-Type'),
                      'text/html; charset=UTF-8',
                      'Content-Type of Response.headers should match');
        assert_equals(response.headers.get('X-Fetch-Test'),
                      'response test field',
                      'X-Fetch-Test of Response.headers should match');
        assert_equals(response.headers.get('Accept-Encoding'),
                      'forbidden header name test',
                      'Accept-Encoding of Response.headers should match');
        response.headers.set('X-Fetch-Test2', 'response test field2');
        assert_equals(size(response.headers), 5,
                      'Response.headers size should increase by 1.');
        assert_equals(response.headers.get('X-Fetch-Test2'),
                      'response test field2',
                      'Response.headers should be added');

        // set/append, Step 4:
        // Otherwise, if guard is response and name is a forbidden response
        // header name, return.
        response.headers.set('set-cookie', 'dummy');
        response.headers.set('sEt-cookie', 'dummy');
        response.headers.set('set-cookie2', 'dummy');
        response.headers.set('set-cOokie2', 'dummy');
        response.headers.append('set-cookie', 'dummy');
        response.headers.append('sEt-cookie', 'dummy');
        response.headers.append('set-cookie2', 'dummy');
        response.headers.append('set-cOokie2', 'dummy');
        assert_equals(size(response.headers), 5,
                      'Response.headers should not accept Set-Cookie nor ' +
                      'Set-Cookie2');
        // delete, Step 4:
        // Otherwise, if guard is response and name is a forbidden response
        // header name, return.
        response.headers.delete('set-cookie');
        response.headers.delete('set-cookie2');
        assert_equals(size(response.headers), 5,
                      'headers.delete should do nothing' +
                      'for forbidden response headers');

        response.headers.delete('Accept-Encoding');
        assert_equals(size(response.headers), 4,
                      'Response.headers size should decrease by 1.');
        response.headers.delete('X-Fetch-Test');
        assert_equals(size(response.headers), 3,
                      'Response.headers size should decrease by 1.');

        // Test set/append/delete for forbidden header names
        FORBIDDEN_HEADERS.forEach(function(header) {
          response.headers.append(header, 'test');
          assert_equals(size(response.headers), 4,
                        'Response.headers.append should accept ' +
                        'header name: ' + header);
          assert_equals(response.headers.get(header),
                        'test',
                        header + ' of Response.headers should match');
          response.headers.set(header, 'test2');
          assert_equals(response.headers.get(header),
                        'test2',
                        header + ' of Response.headers should match');
          response.headers.delete(header);
          assert_equals(size(response.headers), 3,
                        'Response.headers.delete should accept ' +
                        'header name: ' + header);
        });
      });
    // Note: detailed behavioral tests for Headers are in another test,
    // http/tests/fetch/*/headers.html.
  }, 'Response constructor test');

test(function() {
    var response = new Response(new Blob(['dummy'], {type: 'audio/wav'}));
    assert_equals(size(response.headers), 1,
                  'Response.headers should have Content-Type');
    assert_equals(response.headers.get('Content-Type'), 'audio/wav',
                  'Content-Type of Response.headers should be set');

    response = new Response(new Blob(['dummy'], {type: 'audio/wav'}),
                            {
                              headers: {
                                'Content-Type': 'text/html; charset=UTF-8'
                              }
                            });
    assert_equals(size(response.headers), 1,
                  'Response.headers should have Content-Type');
    assert_equals(response.headers.get('Content-Type'),
                  'text/html; charset=UTF-8',
                  'Content-Type of Response.headers should be overridden');
  }, 'Response content type test');

test(function() {
    [0, 1, 100, 199, 600, 700].forEach(function(status) {
        assert_throws({name: 'RangeError'},
                      function() {
                        new Response(new Blob(), {status: status});
                      },
                      'new Response with status = ' + status + ' should throw');
      });
    [200, 300, 400, 500, 599].forEach(function(status) {
        var response = new Response(new Blob(), {status: status});
        assert_equals(response.status, status, 'Response.status should match');
        if (200 <= status && status <= 299)
          assert_true(response.ok, 'Response.ok must be true for ' + status);
        else
          assert_false(response.ok, 'Response.ok must be false for ' + status);
      });

    var invalidNames = ['', '(', ')', '<', '>', '@', ',', ';', ':', '\\', '"',
                        '/', '[', ']', '?', '=', '{', '}', '\u3042', 'a(b'];
    invalidNames.forEach(function(name) {
        assert_throws(
          {name: 'TypeError'},
          function() {
            var obj = {};
            obj[name] = 'a';
            new Response(new Blob(), {headers: obj});
          },
          'new Response with headers with an invalid name (' + name +
          ') should throw');
        assert_throws(
          {name: 'TypeError'},
          function() {
            new Response(new Blob(), {headers: [[name, 'a']]});
          },
          'new Response with headers with an invalid name (' + name +
          ') should throw');
      });
    var invalidValues = ['test \r data', 'test \n data'];
    invalidValues.forEach(function(value) {
        assert_throws(
          {name: 'TypeError'},
          function() {
            new Response(new Blob(),
                         {headers: {'X-Fetch-Test': value}});
          },
          'new Response with headers with an invalid value should throw');
        assert_throws(
          {name: 'TypeError'},
          function() {
            new Response(new Blob(),
                         {headers: [['X-Fetch-Test', value]]});
          },
          'new Response with headers with an invalid value should throw');
      });
  }, 'Response throw error test');

done();
