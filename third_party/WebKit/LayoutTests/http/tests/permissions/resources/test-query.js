if (self.importScripts) {
    importScripts('helpers.js');

    if (get_current_scope() == 'ServiceWorker')
        importScripts('../../serviceworker/resources/worker-testharness.js');
    else
        importScripts('../../resources/testharness.js');
}

// FIXME: re-enable when WebPermissionClient is implemented in Chromium.
//async_test(function(test) {
//    Permissions.query('geolocation').then(function() {
//        assert_unreached('Permissions.query() should not succeed (for now).')
//    }, function(e) {
//        assert_true(e instanceof DOMException);
//        assert_equals('NotSupportedError', e.name);
//    }).then(function() {
//        test.done();
//    });
//}, 'Check the Permissions.query() normal behavior in ' + get_current_scope() + ' scope.');

async_test(function(test) {
    Permissions.query('unknown-keyword').then(function() {
        assert_unreached('Permissions.query() should not succeed (for now).')
    }, function(e) {
        assert_true(e instanceof TypeError);
        assert_equals('TypeError', e.name);
    }).then(function() {
        test.done();
    });
}, 'Check the Permissions.query() with wrong keyword in ' + get_current_scope() + ' scope.');

done();
