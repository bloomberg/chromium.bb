var port;
self.addEventListener('message', function(e) {
    var message = e.data;
    if ('port' in message) {
        port = message.port;
        doIndexedDBTest();
    }
});
function send(action, text) {
    if (port) port.postMessage({action: action, text: text});
}

function evalAndLog(s) {
    send('log', s);
    try {
        return eval(s);
    } catch (ex) {
        send('fail', 'EXCEPTION: ' + ex);
        throw ex;
    }
}

function debug(s) {
    send('log', s);
}

function doIndexedDBTest() {
    debug('Preparing the database in the service worker');
    var request = evalAndLog("indexedDB.deleteDatabase('db')");
    request.onsuccess = function() {
        request = evalAndLog("indexedDB.open('db')");
        request.onupgradeneeded = function() {
            db = request.result;
            evalAndLog("db.createObjectStore('store')");
        };
        request.onsuccess = function() {
            db = request.result;
            evalAndLog("tx = db.transaction('store', 'readwrite')");
            evalAndLog("store = tx.objectStore('store')");
            evalAndLog("store.put('value', 'key')");
            tx.oncomplete = function() {
                send('quit');
            };
        };
    };
}
