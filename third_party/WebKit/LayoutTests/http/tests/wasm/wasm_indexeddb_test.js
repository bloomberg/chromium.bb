// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var db_name = 'db';
var obj_store = 'store';
var module_key = 'my_module';

function createAndSaveToIndexedDB() {
  return new Promise( (resolve, reject) => {
    createWasmModule()
      .then (mod => {
        var delete_request = indexedDB.deleteDatabase(db_name);
        delete_request.onsuccess = function() {
          var open_request = indexedDB.open(db_name);
          open_request.onupgradeneeded = function() {
            var db = open_request.result;
            db.createObjectStore(obj_store);
          };
          open_request.onsuccess = function() {
            var db = open_request.result;
            var tx = db.transaction(obj_store, 'readwrite');
            var store = tx.objectStore(obj_store);
            store.put(mod, module_key);
            tx.oncomplete = function() {
              resolve();
            };
          };
        };
      })
      .catch(error => reject(error));
  });
}

function loadFromIndexedDB(prev) {
  return new Promise(resolve => {
    prev.then(() => {
      var open_request = indexedDB.open(db_name);
      open_request.onsuccess = function() {
        var db = open_request.result;
        var tx = db.transaction(obj_store);
        var store = tx.objectStore(obj_store);
        var get_request = store.get(module_key);
        get_request.onsuccess = function() {
          var mod = get_request.result;
          var instance = new WebAssembly.Instance(get_request.result);
          resolve(instance.exports.increment(1));
        };
      };
    });
  });
}

function TestIndexedDBLoadStore() {
  return loadFromIndexedDB(createAndSaveToIndexedDB())
    .then((res) => assert_equals(res, 2))
    .catch(error => assert_unreached(error));
}
