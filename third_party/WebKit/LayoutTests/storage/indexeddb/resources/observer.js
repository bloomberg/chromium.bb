if (this.importScripts) {
    importScripts('../../../resources/testharness.js');
    importScripts('generic-idb-operations.js');
}

async_test(function(t) {
  var dbname = location.pathname + ' - ' + 'empty transaction';
  var openRequest = indexedDB.open(dbname);
  var callback_count = 0;
  var obs = new IDBObserver(t.step_func(function() { callback_count++; }), {operationTypes: ['put']});

  openRequest.onupgradeneeded = t.step_func(function() {
      openRequest.result.createObjectStore('store');
  });
  openRequest.onsuccess = t.step_func(function() {
    var db = openRequest.result;
    var tx1 = db.transaction('store', 'readwrite');
    var tx2 = db.transaction('store', 'readwrite');
    obs.observe(db, tx1);
    tx2.objectStore('store').put(1, 1);
    tx1.oncomplete = t.step_func(function() {
      countCallbacks(callback_count, 0);
    });
    tx2.oncomplete = t.step_func(function() {
      countCallbacks(callback_count, 1);
      t.done();
    });
    tx1.onerror = t.unreached_func('transaction should not fail');
    tx2.onerror = t.unreached_func('transaction should not fail');
  });
}, 'Registering observe call with empty transaction');

async_test(function(t) {
  var dbname = location.pathname + ' - ' + 'observer in version change';
  var openRequest = indexedDB.open(dbname);
  var callback_count = 0;
  var obs;
  openRequest.onupgradeneeded = t.step_func(function() {
      openRequest.result.createObjectStore('store');
      obs = new IDBObserver(t.step_func(function(changes) { callback_count++; }), { operationTypes: ['put'] });
  });
  openRequest.onsuccess = t.step_func(function() {
    var db = openRequest.result;
    var tx1 = db.transaction('store', 'readwrite');
    var tx2 = db.transaction('store', 'readwrite');
    tx1.objectStore('store').get(1);
    tx2.objectStore('store').put(1, 1);
    obs.observe(db, tx1);
    tx1.oncomplete = t.step_func(function() {
      countCallbacks(callback_count, 0);
    });
    tx2.oncomplete = t.step_func(function() {
      countCallbacks(callback_count, 1);
      t.done();
    });
    tx1.onerror = t.unreached_func('transaction should not fail');
    tx2.onerror = t.unreached_func('transaction should not fail');
   });
  }, 'Create IDBObserver during version change');

async_test(function(t) {
  var dbname = location.pathname + ' - ' + 'ignore observe call';
  var openRequest = indexedDB.open(dbname);
  var callback_count = 0;
  var obs = new IDBObserver(t.step_func(function() { callback_count++; }), { operationTypes: ['put'] });
  openRequest.onupgradeneeded = t.step_func(function() {
      var db = openRequest.result;
      db.createObjectStore('store');
      obs.observe(db, openRequest.transaction);
  });
  openRequest.onsuccess = t.step_func(function() {
    var db = openRequest.result;
    var tx = db.transaction('store', 'readwrite');
    tx.objectStore('store').put(1, 1);
    tx.oncomplete = t.step_func(function() {
      countCallbacks(callback_count, 0);
      t.done();
    });
    tx.onerror = t.unreached_func('transaction should not fail');
  });
}, 'Observe call during version change ignored');

async_test(function(t) {
  var dbname = location.pathname + ' - ' + 'abort associated transaction';
  var openRequest = indexedDB.open(dbname);
  var callback_count = 0;
  var obs = new IDBObserver(t.step_func(function() { callback_count++; }), { operationTypes: ['put'] });
  openRequest.onupgradeneeded = t.step_func(function() {
    openRequest.result.createObjectStore('store');
  });
  openRequest.onsuccess = t.step_func(function() {
    var db = openRequest.result;
    var tx1 = db.transaction('store', 'readwrite');
    var tx2 = db.transaction('store', 'readwrite');
    tx1.objectStore('store').get(1);
    tx2.objectStore('store').put(1, 1);
    obs.observe(db, tx1);
    tx1.abort();

    tx1.onabort = t.step_func(function(){
      countCallbacks(callback_count, 0);
    });
    tx1.oncomplete = t.unreached_func('transaction should not complete');
    tx2.oncomplete = t.step_func(function() {
      countCallbacks(callback_count, 0);
      t.done();
    });
    tx2.onerror = t.unreached_func('transaction error should not fail');
  });
}, 'Abort transaction associated with observer');

async_test(function(t) {
  var dbname = location.pathname + ' - ' + 'abort transaction';
  var openRequest = indexedDB.open(dbname);
  var callback_count = 0;
  var obs = new IDBObserver(t.step_func(function() { callback_count++; }), { operationTypes: ['put'] });
  openRequest.onupgradeneeded = t.step_func(function() {
    openRequest.result.createObjectStore('store');
  });
  openRequest.onsuccess = t.step_func(function() {
    var db = openRequest.result;
    var tx1 = db.transaction('store', 'readwrite');
    var tx2 = db.transaction('store', 'readwrite');
    var tx3 = db.transaction('store', 'readwrite');
    tx1.objectStore('store').get(1);
    tx2.objectStore('store').put(1, 1);
    tx3.objectStore('store').put(1, 1);
    obs.observe(db, tx1);
    tx2.abort();
    tx1.oncomplete = t.step_func(function() {
      countCallbacks(callback_count, 0);
    });
    tx2.oncomplete = t.unreached_func('transaction should not complete');
    tx2.onabort = t.step_func(function() {
      countCallbacks(callback_count, 0);
    });
    tx3.oncomplete = t.step_func(function() {
      countCallbacks(callback_count, 1);
      t.done();
    });
    tx1.onerror = t.unreached_func('transaction should not fail');
    tx3.onerror = t.unreached_func('transaction should not fail');
  });
}, 'Abort transaction recorded by observer');

done();