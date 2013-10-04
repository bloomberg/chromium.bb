if (this.importScripts) {
    importScripts('../../../fast/js/resources/js-test-pre.js');
    importScripts('shared.js');
}

description("Test the basics of IndexedDB's IDBCursor objects.");

indexedDBTest(prepareDatabase);
function prepareDatabase(evt)
{
    preamble(evt);
    db = event.target.result;

    evalAndLog("store = db.createObjectStore('storeName')");
    evalAndLog("index = store.createIndex('indexName', 'indexOn')");
    evalAndLog("store.put({indexOn: 'a'}, 0)");

    request = evalAndLog("store.openCursor()");
    request.onsuccess = onStoreOpenCursor;
    request.onerror = unexpectedErrorCallback;

    request = evalAndLog("store.openKeyCursor()");
    request.onsuccess = onStoreOpenKeyCursor;
    request.onerror = unexpectedErrorCallback;

    request = evalAndLog("index.openCursor()");
    request.onsuccess = onIndexOpenCursor;
    request.onerror = unexpectedErrorCallback;

    request = evalAndLog("index.openKeyCursor()");
    request.onsuccess = onIndexOpenKeyCursor;
    request.onerror = unexpectedErrorCallback;

    event.target.transaction.oncomplete = finishJSTest;
}

function onStoreOpenCursor(evt) {
    preamble(evt);
    evalAndLog("cursor = event.target.result");
    shouldBeNonNull("cursor");
    shouldBeTrue("cursor instanceof IDBCursor");
    shouldBeTrue("cursor instanceof IDBCursorWithValue");
    shouldBeTrue("'key' in cursor");
    shouldBe("cursor.key", "0");
    shouldBeTrue("'primaryKey' in cursor");
    shouldBe("cursor.primaryKey", "0");
    shouldBeTrue("'value' in cursor");
    shouldBeEqualToString("JSON.stringify(cursor.value)", '{"indexOn":"a"}');
}

function onStoreOpenKeyCursor(evt) {
    preamble(evt);
    evalAndLog("cursor = event.target.result");
    shouldBeNonNull("cursor");
    shouldBeTrue("cursor instanceof IDBCursor");
    shouldBeFalse("cursor instanceof IDBCursorWithValue");
    shouldBeTrue("'key' in cursor");
    shouldBe("cursor.key", "0");
    shouldBeTrue("'primaryKey' in cursor");
    shouldBe("cursor.primaryKey", "0");
    shouldBeFalse("'value' in cursor");
}

function onIndexOpenCursor(evt) {
    preamble(evt);
    evalAndLog("cursor = event.target.result");
    shouldBeNonNull("cursor");
    shouldBeTrue("cursor instanceof IDBCursor");
    shouldBeTrue("cursor instanceof IDBCursorWithValue");
    shouldBeTrue("'key' in cursor");
    shouldBeEqualToString("cursor.key", "a");
    shouldBeTrue("'primaryKey' in cursor");
    shouldBe("cursor.primaryKey", "0");
    shouldBeTrue("'value' in cursor");
    shouldBeEqualToString("JSON.stringify(cursor.value)", '{"indexOn":"a"}');
}

function onIndexOpenKeyCursor(evt) {
    preamble(evt);
    evalAndLog("cursor = event.target.result");
    shouldBeNonNull("cursor");
    shouldBeTrue("cursor instanceof IDBCursor");
    shouldBeFalse("cursor instanceof IDBCursorWithValue");
    shouldBeTrue("'key' in cursor");
    shouldBeEqualToString("cursor.key", "a");
    shouldBeTrue("'primaryKey' in cursor");
    shouldBe("cursor.primaryKey", "0");
    shouldBeFalse("'value' in cursor");
}

