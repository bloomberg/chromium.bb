if (this.importScripts) {
    importScripts('../../../resources/testharness.js');
}

function callback(){};

async_test(function(t) {
    var description = 'observers constructor test';
    var dbname = location.pathname + ' - ' + description;
    var openRequest = indexedDB.open(dbname);
    var obs1 = new IDBObserver(callback, {transaction: true, values: true});
    openRequest.onupgradeneeded = t.step_func(function() {
        var db = openRequest.result;
        db.createObjectStore('store');
    });
    openRequest.onsuccess = t.step_func(function() {
        var db = openRequest.result;
        var tx = db.transaction('store', 'readwrite');
        obs1.observe(db, tx);
        t.done();
    });

}, 'observers constructor test');

done();

