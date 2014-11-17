importScripts('../../resources/worker-testharness.js');
importScripts('../../resources/test-helpers.js');

async_test(function(t) {
    var url = get_host_info()['HTTP_REMOTE_ORIGIN'] + '/dummy.html';
    fetch(new Request(url, {mode: 'same-origin'}))
      .then(
        t.unreached_func('Fetching must fail.'),
        function(e) {
          assert_equals(
            e.message,
            'Fetch API cannot load ' + url + '. ' +
            'Request mode is "same-origin" but the URL\'s origin is not same ' +
            'as the request origin ' + get_host_info()['HTTP_ORIGIN']  + '.');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch API error message - not same origin request');

async_test(function(t) {
    var url = 'ftp://example.com/dummy.html';
    fetch(new Request(url, {mode: 'cors'}))
      .then(
        t.unreached_func('Fetching must fail.'),
        function(e) {
          assert_equals(
            e.message,
            'Fetch API cannot load ' + url + '. ' +
            'URL scheme must be "http" or "https" for CORS request.');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch API error message - non http cors request');

async_test(function(t) {
    var url = 'about://blank';
    fetch(new Request(url))
      .then(
        t.unreached_func('Fetching must fail.'),
        function(e) {
          assert_equals(
            e.message,
            'Fetch API cannot load ' + url + '. ' +
            'URL scheme "about" is not supported.');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch API error message - unsupported scheme.');

async_test(function(t) {
    var url =
        new URL(get_host_info()['HTTP_ORIGIN'] + base_path() +
                '../../resources/invalid-chunked-encoding.php').toString();
    fetch(new Request(url))
      .then(
        t.unreached_func('Fetching must fail.'),
        function(e) {
          assert_equals(
            e.message,
            'Fetch API cannot load ' + url + '. ' +
            'net::ERR_INVALID_CHUNKED_ENCODING');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch API error message - invalid chunked encoding.');

async_test(function(t) {
    var url =
        new URL(get_host_info()['HTTP_REMOTE_ORIGIN'] + base_path() +
                '../../resources/fetch-access-control.php').toString();
    fetch(new Request(url))
      .then(
        t.unreached_func('Fetching must fail.'),
        function(e) {
          assert_equals(
            e.message,
            'Fetch API cannot load ' + url + '. ' +
            'No \'Access-Control-Allow-Origin\' header is present on the ' +
            'requested resource. Origin \'' + get_host_info()['HTTP_ORIGIN'] +
            '\' is therefore not allowed access.');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch API error message - cors error.');

async_test(function(t) {
    // FIXME: When we support the redirection in Fech API, we have to change
    // this test case.
    var redirect = 'http://www.example.com';
    var url =
        new URL(get_host_info()['HTTP_REMOTE_ORIGIN'] + base_path() +
                '../../resources/redirect.php?Redirect=' + redirect).toString();
    fetch(new Request(url))
      .then(
        t.unreached_func('Fetching must fail.'),
        function(e) {
          assert_equals(
            e.message,
            'Fetch API cannot load ' + url + '. ' +
            'Redirects are not yet supported.');
          t.done();
        })
      .catch(unreached_rejection(t));
  }, 'Fetch API error message - redirect error.');
