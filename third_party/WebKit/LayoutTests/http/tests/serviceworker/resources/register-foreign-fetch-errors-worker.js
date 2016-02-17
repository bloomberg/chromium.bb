self.addEventListener('install', function(event) {
    var scope = registration.scope;
    var scope_url = new URL(scope);

    test(function() {
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes(scope, '*');
          });
      }, 'Scopes not an array');

    test(function() {
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes([{}], '*');
          });
      }, 'Scopes not a string in array');

    test(function() {
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes(['/foo'], '*');
          });
      }, 'Relative url not under scope');

    test(function() {
        var url = new URL(scope_url);
        url.host = 'example.com';
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes([url.href], '*');
          });
      }, 'Absolute url not under scope');

    async_test(function(t) {
        self.setTimeout(t.step_func(function() {
            assert_throws('InvalidStateError', function() {
                event.registerForeignFetchScopes([scope], '*');
              });
            t.done();
          }), 1);
      }, 'Call after event returned');

    test(function() {
        event.registerForeignFetchScopes([], '*');
      }, 'Empty array with wildcard origin string');

    test(function() {
        event.registerForeignFetchScopes([scope, scope + '/foo'], '*');
      }, 'Absolute urls');

    test(function() {
        // Figure out scope relative to location of this script:
        var local_dir = location.pathname;
        local_dir = local_dir.substr(0, local_dir.lastIndexOf('/'));
        assert_true(scope_url.pathname.startsWith(local_dir));
        var relative_scope = scope_url.pathname.substr(local_dir.length + 1);

        event.registerForeignFetchScopes([
          scope_url.pathname,
          relative_scope,
          './' + relative_scope,
          relative_scope + '/foo'], '*');
      }, 'Relative urls');

    test(function() {
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes([scope]);
          });
      }, 'No origins specified');

    test(function() {
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes([scope], {});
          });
      }, 'Origins not a string or array');

    test(function() {
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes([scope], [{}]);
          });
      }, 'Origins contains something not a string');

    test(function() {
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes([scope], '/foo');
          });
      }, 'Origin not an absolute URL');

    test(function() {
        event.registerForeignFetchScopes([scope], ['*']);
      }, 'Wildcard origin string in array');

    test(function() {
        event.registerForeignFetchScopes([scope], 'https://example.com/');
      }, 'Origin string');

    test(function() {
        event.registerForeignFetchScopes([scope], ['https://example.com/']);
      }, 'Origin string in array');

    test(function() {
        event.registerForeignFetchScopes(
            [scope], ['https://example.com/', 'https://chromium.org']);
      }, 'Array with multiple origins');

    test(function() {
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes([scope],
                                             ['*', 'https://example.com/']);
          });
      }, 'Origins includes wildcard and other strings');

    test(function() {
        assert_throws(new TypeError(), function() {
            event.registerForeignFetchScopes([scope],
                                             ['https://example.com/', '*']);
          });
      }, 'Origins includes other strings and wildcard');
  });

// Import testharness after install handler to make sure our install handler
// runs first. Otherwise only one test will run.
importScripts('../../resources/testharness.js');
