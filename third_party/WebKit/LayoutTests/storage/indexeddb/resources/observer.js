if (this.importScripts) {
    importScripts('../../../resources/testharness.js');
}

function callback(){};

async_test(function(t) {
  var description = 'observer addition and removal test';
  var dbname = location.pathname + ' - ' + description;
  var openRequest = indexedDB.open(dbname);
  var obs1 = new IDBObserver(callback, {transaction: true, values: true});
  var obs2 = new IDBObserver(callback, {transaction: true, values: true});

  openRequest.onupgradeneeded = t.step_func(function() {
      var db = openRequest.result;
      db.createObjectStore('store');
  });
  openRequest.onsuccess = t.step_func(function() {
    var db = openRequest.result;
    var tx = db.transaction('store', 'readwrite');
    var store = tx.objectStore('store');
        var put_request = store.put(1,1);
        obs1.observe(db, tx);
        obs1.unobserve(db);
        obs1.observe(db, tx);
        obs2.observe(db, tx);
        tx.oncomplete = t.step_func(function(){
          obs1.unobserve(db);
          t.done();
        });
      });
}, 'observer addition and removal test');

done();

