if (this.importScripts) {
    importScripts('../../../resources/testharness.js');
}

function compareChanges(actual, expected) {
  assert_equals(actual.database.name, expected.dbName, 'The change record database should be the same as the database being acted on');
  assert_equals(actual.records.size, expected.records.size, 'Incorrect number of objectStores recorded by observer');
  for (var key in expected.records) {
    assert_true(actual.records.has(key));
    var actual_observation = actual.records.get(key);
    var expected_observation = expected.records[key];
    assert_equals(actual_observation.length, expected_observation.length, 'Number of observations recorded for objectStore '+key+ ' should match observed operations');
    for (var i in expected_observation)
      compareObservations(actual_observation[i], expected_observation[i]);
  }
}

function compareObservations(actual, expected) {
  assert_equals(actual.type, expected.type);
  if (actual.type == 'clear') {
    assert_equals(actual.key, undefined, 'clear operation has no key');
    assert_equals(actual.value, null, 'clear operation has no value');
    return;
  }
  // TODO(palakj): Type should return 'delete' instead of 'kDelete', once fixed. Issue crbug.com/609934.
  if (actual.type == 'kDelete') {
    assert_equals(actual.key.lower, expected.key.lower, 'Observed operation key lower bound should match operation performed');
    assert_equals(actual.key.upper, expected.key.upper, 'Observed operation key upper bound should match operation performed');
    assert_equals(actual.key.lower_open, expected.key.lower_open, 'Observed operation key lower open should match operation performed');
    assert_equals(actual.key.upper_open, expected.key.upper_open, 'Observed operation key upper open should match operation performed');
    // TODO(palakj): Value needs to be updated, once returned correctly. Issue crbug.com/609934.
    assert_equals(actual.value, null, 'Delete operation has no value');
    return;
  }
  assert_equals(actual.key.lower, expected.key, 'Observed operation key lower bound should match operation performed');
  assert_equals(actual.key.upper, expected.key, 'Observed operation key upper bound should match operation performed');
  // TODO(palakj): Value needs to be updated, once returned correctly. Issue crbug.com/609934.
  assert_equals(actual.value, null, 'Put/Add operation value should be nil');
}

function countCallbacks(actual, expected) {
  assert_equals(actual, expected, 'Number of callbacks fired for observer should match number of transactions it observed')
}

function createDatabase(db, stores) {
  for (var i in stores)
    db.createObjectStore(stores[i]);
}

function operateOnStore(store, operations) {
  for (var i in operations ) {
    var op = operations[i];
    assert_in_array(op.type, ['put', 'add', 'delete', 'clear', 'get'], 'Operation type not defined');
    if (op.type == 'put')
      store.put(op.value, op.key);
    else if (op.type == 'add')
      store.add(op.value, op.key);
    else if (op.type == 'delete')
      store.delete(IDBKeyRange.bound(op.key.lower, op.key.upper));
    else if (op.type == 'clear')
      store.clear();
    else
      store.get(op.key)
  }
}
