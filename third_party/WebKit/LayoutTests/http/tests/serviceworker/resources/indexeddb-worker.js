self.addEventListener('message', function(e) {
    var message = e.data;
    if ('port' in message)
      e.waitUntil(doIndexedDBTest(message.port));
  });

function doIndexedDBTest(port) {
  return new Promise(function(resolve) {
    var delete_request = indexedDB.deleteDatabase('db');
    delete_request.onsuccess = function() {
      var open_request = indexedDB.open('db');
      open_request.onupgradeneeded = function() {
        var db = open_request.result;
        db.createObjectStore('store');
      };
      open_request.onsuccess = function() {
        var db = open_request.result;
        var tx = db.transaction('store', 'readwrite');
        var store = tx.objectStore('store');
        store.put('value', 'key');
        tx.oncomplete = function() {
          port.postMessage('done');
          resolve();
        };
      };
    };
  });
}
