if (self.importScripts) {
    importScripts('helpers.js');

    if (get_current_scope() == 'ServiceWorker')
        importScripts('../../serviceworker/resources/worker-testharness.js');
    else
        importScripts('../../resources/testharness.js');
}

async_test(function(test) {
   navigator.permissions.query('geolocation').then(function(result) {
       // TODO(mlamouri): test values when some instrumentation are done.
       assert_true(result instanceof PermissionStatus);
       test.done();
   }).catch(function() {
        assert_unreached('navigator.permissions.query() should not fail.')
   });
}, 'Check the navigator.permissions.query() normal behavior in ' + get_current_scope() + ' scope.');

async_test(function(test) {
    navigator.permissions.query('unknown-keyword').then(function() {
        assert_unreached('navigator.permissions.query() should not succeed (for now).')
    }, function(e) {
        assert_true(e instanceof TypeError);
        assert_equals('TypeError', e.name);
    }).then(function() {
        test.done();
    });
}, 'Check the navigator.permissions.query() with wrong keyword in ' + get_current_scope() + ' scope.');

done();
