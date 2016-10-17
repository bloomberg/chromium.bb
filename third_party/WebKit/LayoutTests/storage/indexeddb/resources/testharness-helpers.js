function indexeddb_test(upgrade_func, body_func, description) {
  async_test(function(t) {
    var dbName = 'db' + self.location.pathname + '-' + description;
    var delete_request = indexedDB.deleteDatabase(dbName);
    delete_request.onerror = t.unreached_func('deleteDatabase should not fail');
    delete_request.onsuccess = t.step_func(function(e) {
      var open_request = indexedDB.open(dbName);
      open_request.onerror = t.unreached_func('open should not fail');
      open_request.onupgradeneeded = t.step_func(function(e) {
        upgrade_func(t, open_request.result, open_request);
      });
      open_request.onsuccess = t.step_func(function(e) {
        body_func(t, open_request.result);
      });
    });
  }, description);
}

function assert_key_equals(a, b, message) {
  assert_equals(indexedDB.cmp(a, b), 0, message);
}

// Call with a Test and an array of expected results in order. Returns
// a function; call the function when a result arrives and when the
// expected number appear the order will be asserted and test
// completed.
function expect(t, expected) {
  var results = [];
  return result => {
    results.push(result);
    if (results.length === expected.length) {
      assert_array_equals(results, expected);
      t.done();
    }
  };
}
