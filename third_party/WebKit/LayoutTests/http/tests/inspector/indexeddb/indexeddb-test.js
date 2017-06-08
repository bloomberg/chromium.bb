var initialize_IndexedDBTest = function() {
InspectorTest.preloadPanel("resources");

InspectorTest.dumpIndexedDBTree = function()
{
    InspectorTest.addResult("Dumping IndexedDB tree:");
    var indexedDBTreeElement = UI.panels.resources._sidebar.indexedDBListTreeElement;
    if (!indexedDBTreeElement.childCount()) {
        InspectorTest.addResult("    (empty)");
        return;
    }
    for (var i = 0; i < indexedDBTreeElement.childCount(); ++i) {
        var databaseTreeElement = indexedDBTreeElement.childAt(i);
        InspectorTest.addResult("    database: " + databaseTreeElement.title);
        if (!databaseTreeElement.childCount()) {
            InspectorTest.addResult("        (no object stores)");
            continue;
        }
        for (var j = 0; j < databaseTreeElement.childCount(); ++j) {
            var objectStoreTreeElement = databaseTreeElement.childAt(j);
            InspectorTest.addResult("        Object store: " + objectStoreTreeElement.title);
            if (!objectStoreTreeElement.childCount()) {
                InspectorTest.addResult("            (no indexes)");
                continue;
            }
            for (var k = 0; k < objectStoreTreeElement.childCount(); ++k) {
                var indexTreeElement = objectStoreTreeElement.childAt(k);
                InspectorTest.addResult("            Index: " + indexTreeElement.title);
            }
        }
    }
}

var lastCallbackId = 0;
var callbacks = {};
var callbackIdPrefix = "InspectorTest.IndexedDB_callback";
InspectorTest.evaluateWithCallback = function(frameId, methodName, parameters, callback)
{
    InspectorTest._installIndexedDBSniffer();
    var callbackId = ++lastCallbackId;
    callbacks[callbackId] = callback;
    var parametersString = "dispatchCallback.bind(this, \"" + callbackIdPrefix + callbackId + "\")";
    for (var i = 0; i < parameters.length; ++i)
        parametersString += ", " + JSON.stringify(parameters[i]);

    var requestString = methodName + "(" + parametersString + ")";
    InspectorTest.evaluateInPage(requestString);
};

InspectorTest._installIndexedDBSniffer = function()
{
    InspectorTest.addConsoleSniffer(consoleMessageOverride, false);

    function consoleMessageOverride(msg)
    {
        var text = msg.messageText;
        if (!text.startsWith(callbackIdPrefix)) {
            InspectorTest.addConsoleSniffer(consoleMessageOverride, false);
            return;
        }
        var callbackId = text.substring(callbackIdPrefix.length);
        callbacks[callbackId].call();
        delete callbacks[callbackId];
    }
};

InspectorTest.createDatabase = function(frameId, databaseName, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "createDatabase", [databaseName], callback)
};

InspectorTest.deleteDatabase = function(frameId, databaseName, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "deleteDatabase", [databaseName], callback)
};

InspectorTest.createObjectStore = function(frameId, databaseName, objectStoreName, keyPath, autoIncrement, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "createObjectStore", [databaseName, objectStoreName, keyPath, autoIncrement], callback)
};

InspectorTest.deleteObjectStore = function(frameId, databaseName, objectStoreName, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "deleteObjectStore", [databaseName, objectStoreName], callback)
};

InspectorTest.createObjectStoreIndex = function(frameId, databaseName, objectStoreName, objectStoreIndexName, keyPath, unique, multiEntry, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "createObjectStoreIndex", [databaseName, objectStoreName, objectStoreIndexName, keyPath, unique, multiEntry], callback)
};

InspectorTest.deleteObjectStoreIndex = function(frameId, databaseName, objectStoreName, objectStoreIndexName, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "deleteObjectStoreIndex", [databaseName, objectStoreName, objectStoreIndexName], callback)
};

InspectorTest.addIDBValue = function(frameId, databaseName, objectStoreName, value, key, callback)
{
    InspectorTest.evaluateWithCallback(frameId, "addIDBValue", [databaseName, objectStoreName, value, key], callback)
};

InspectorTest.createIndexedDBModel = function()
{
    var indexedDBModel = new Resources.IndexedDBModel(SDK.targetManager.mainTarget(), InspectorTest.securityOriginManager);
    indexedDBModel.enable();
    return indexedDBModel;
};

InspectorTest.createDatabaseAsync = function(databaseName) {
    return InspectorTest.evaluateInPageAsync("createDatabaseAsync('" + databaseName + "')");
};

InspectorTest.createObjectStoreAsync = function(databaseName, objectStoreName, indexName, keyPath) {
    return InspectorTest.evaluateInPageAsync("createObjectStoreAsync('" + databaseName + "', '" + objectStoreName + "', '" + indexName + "', '" + keyPath + "')");
};

InspectorTest.addIDBValueAsync = function(databaseName, objectStoreName, key, value) {
    return InspectorTest.evaluateInPageAsync("addIDBValueAsync('" + databaseName + "', '" + objectStoreName + "', '" + key + "', '" + value + "')");
};
};

function dispatchCallback(callbackId)
{
    console.log(callbackId);
}

function onIndexedDBError(e)
{
    console.error("IndexedDB error: " + e);
}

function onIndexedDBBlocked(e)
{
    console.error("IndexedDB blocked: " + e);
}

function doWithDatabase(databaseName, callback)
{
    function innerCallback()
    {
        var db = request.result;
        callback(db);
    }

    var request = indexedDB.open(databaseName);
    request.onblocked = onIndexedDBBlocked;
    request.onerror = onIndexedDBError;
    request.onsuccess = innerCallback;
}

function doWithVersionTransaction(databaseName, callback, commitCallback)
{
    doWithDatabase(databaseName, step2);

    function step2(db)
    {
        var version = db.version;
        db.close();
        request = indexedDB.open(databaseName, version + 1);
        request.onerror = onIndexedDBError;
        request.onupgradeneeded = onUpgradeNeeded;
        request.onsuccess = onOpened;

        function onUpgradeNeeded(e)
        {
            var db = e.target.result;
            var trans = e.target.transaction;
            callback(db, trans);
        }

        function onOpened(e)
        {
            var db = e.target.result;
            db.close();
            commitCallback();
        }
    }
}

function doWithReadWriteTransaction(databaseName, objectStoreName, callback, commitCallback)
{
    doWithDatabase(databaseName, step2);

    function step2(db)
    {
        var transaction = db.transaction([objectStoreName], 'readwrite');
        var objectStore = transaction.objectStore(objectStoreName);
        callback(objectStore, innerCommitCallback);

        function innerCommitCallback()
        {
            db.close();
            commitCallback();
        }
    }
}

function createDatabase(callback, databaseName)
{
    var request = indexedDB.open(databaseName);
    request.onerror = onIndexedDBError;
    request.onsuccess = closeDatabase;

    function closeDatabase()
    {
        request.result.close();
        callback();
    }
}

function deleteDatabase(callback, databaseName)
{
    var request = indexedDB.deleteDatabase(databaseName);
    request.onerror = onIndexedDBError;
    request.onsuccess = callback;
}

function createObjectStore(callback, databaseName, objectStoreName, keyPath, autoIncrement)
{
    doWithVersionTransaction(databaseName, withTransactionCallback, callback);

    function withTransactionCallback(db, transaction)
    {
        var store = db.createObjectStore(objectStoreName, { keyPath: keyPath, autoIncrement: autoIncrement });
    }
}

function deleteObjectStore(callback, databaseName, objectStoreName)
{
    doWithVersionTransaction(databaseName, withTransactionCallback, callback);

    function withTransactionCallback(db, transaction)
    {
        var store = db.deleteObjectStore(objectStoreName);
    }
}

function createObjectStoreIndex(callback, databaseName, objectStoreName, objectStoreIndexName, keyPath, unique, multiEntry)
{
    doWithVersionTransaction(databaseName, withTransactionCallback, callback);

    function withTransactionCallback(db, transaction)
    {
        var objectStore = transaction.objectStore(objectStoreName);
        objectStore.createIndex(objectStoreIndexName, keyPath, { unique: unique, multiEntry: multiEntry });
    }
}

function deleteObjectStoreIndex(callback, databaseName, objectStoreName, objectStoreIndexName)
{
    doWithVersionTransaction(databaseName, withTransactionCallback, callback);

    function withTransactionCallback(db, transaction)
    {
        var objectStore = transaction.objectStore(objectStoreName);
        objectStore.deleteIndex(objectStoreIndexName);
    }
}

function addIDBValue(callback, databaseName, objectStoreName, value, key)
{
    doWithReadWriteTransaction(databaseName, objectStoreName, withTransactionCallback, callback)

    function withTransactionCallback(objectStore, commitCallback)
    {
        var request;
        if (key)
            request = objectStore.add(value, key);
        else
            request = objectStore.add(value);
        request.onerror = onIndexedDBError;
        request.onsuccess = commitCallback;
    }
}

function createDatabaseAsync(databaseName) {
    var callback;
    var promise = new Promise((fulfill) => callback = fulfill);
    var request = indexedDB.open(databaseName);
    request.onerror = onIndexedDBError;
    request.onsuccess = function(event) {
        request.result.close();
        callback();
    }
    return promise;
}

function createObjectStoreAsync(databaseName, objectStoreName, indexName, keyPath) {
    var callback;
    var promise = new Promise((fulfill) => callback = fulfill);
    var request = indexedDB.open(databaseName);
    request.onerror = onIndexedDBError;
    request.onsuccess = function(event) {
        var db = request.result;
        var version = db.version;
        db.close();

        var upgradeRequest = indexedDB.open(databaseName, version + 1);

        upgradeRequest.onerror = onIndexedDBError;
        upgradeRequest.onupgradeneeded = function(e) {
            var upgradeDb = e.target.result;
            var store = upgradeDb.createObjectStore(objectStoreName, { keyPath: "test", autoIncrement: false });
            store.createIndex(indexName, "test", { unique: false, multiEntry: false });
            callback();
        }
        upgradeRequest.onsuccess = function(e) {
            var upgradeDb = e.target.result;
            upgradeDb.close();
            callback();
        }
    }
    return promise;
}

function addIDBValueAsync(databaseName, objectStoreName, key, value) {
    var callback;
    var promise = new Promise((fulfill) => callback = fulfill);
    var request = indexedDB.open(databaseName);
    request.onerror = onIndexedDBError;
    request.onsuccess = function(event) {
        var db = request.result;
        var transaction = db.transaction(objectStoreName, "readwrite");
        var store = transaction.objectStore(objectStoreName);
        store.put({ test: key, testValue: value });

        transaction.onerror = onIndexedDBError;
        transaction.oncomplete = function() {
            db.close();
            callback();
        };
    }
    return promise;
}
