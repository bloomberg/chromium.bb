// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {createStorageAreaAsyncIterator} from './async_iterator.js';
import {promiseForRequest, promiseForTransaction, throwForDisallowedKey} from './idb_utils.js';

// TODOs/spec-noncompliances:
// - Susceptible to tampering of built-in prototypes and globals. We want to
//   work on tooling to ameliorate that.

// TODO: Use private fields when those ship.
// In the meantime we use this hard-to-understand, but effective, pattern:
// http://2ality.com/2016/01/private-data-classes.html#keeping-private-data-in-weakmaps
// Of note, the weak map entries will live only as long as the corresponding StorageArea instances.
//
// Cheatsheet:
// x.#y      <--->  _y.get(x)
// x.#y = z  <--->  _y.set(x, z)

const _databaseName = new WeakMap();
const _databasePromise = new WeakMap();
const _backingStoreObject = new WeakMap();

const DEFAULT_STORAGE_AREA_NAME = 'default';
const DEFAULT_IDB_STORE_NAME = 'store';

if (!self.isSecureContext) {
  throw new DOMException(
      'KV Storage is only available in secure contexts',
      'SecurityError');
}

export class StorageArea {
  constructor(name) {
    _databasePromise.set(this, null);
    _databaseName.set(this, `kv-storage:${name}`);
  }

  async set(key, value) {
    throwForDisallowedKey(key);

    return performDatabaseOperation(this, 'readwrite', (transaction, store) => {
      if (value === undefined) {
        store.delete(key);
      } else {
        store.put(value, key);
      }

      return promiseForTransaction(transaction);
    });
  }

  async get(key) {
    throwForDisallowedKey(key);

    return performDatabaseOperation(this, 'readonly', (transaction, store) => {
      return promiseForRequest(store.get(key));
    });
  }

  async delete(key) {
    throwForDisallowedKey(key);

    return performDatabaseOperation(this, 'readwrite', (transaction, store) => {
      store.delete(key);
      return promiseForTransaction(transaction);
    });
  }

  async clear() {
    if (!_databasePromise.has(this)) {
      throw new TypeError('Invalid this value');
    }

    const databasePromise = _databasePromise.get(this);
    if (databasePromise !== null) {
      // Don't try to delete, and clear the promise, while we're opening the database; wait for that
      // first.
      try {
        await databasePromise;
      } catch {
        // If the database failed to initialize, then that's fine, we'll still try to delete it.
      }

      _databasePromise.set(this, null);
    }

    return promiseForRequest(self.indexedDB.deleteDatabase(_databaseName.get(this)));
  }

  keys() {
    if (!_databasePromise.has(this)) {
      throw new TypeError('Invalid this value');
    }

    return createStorageAreaAsyncIterator(
        'keys', steps => performDatabaseOperation(this, 'readonly', steps));
  }

  values() {
    if (!_databasePromise.has(this)) {
      throw new TypeError('Invalid this value');
    }

    return createStorageAreaAsyncIterator(
        'values', steps => performDatabaseOperation(this, 'readonly', steps));
  }

  entries() {
    if (!_databasePromise.has(this)) {
      throw new TypeError('Invalid this value');
    }

    return createStorageAreaAsyncIterator(
        'entries', steps => performDatabaseOperation(this, 'readonly', steps));
  }

  get backingStore() {
    if (!_databasePromise.has(this)) {
      throw new TypeError('Invalid this value');
    }

    if (!_backingStoreObject.has(this)) {
      _backingStoreObject.set(this, Object.freeze({
        database: _databaseName.get(this),
        store: DEFAULT_IDB_STORE_NAME,
        version: 1,
      }));
    }

    return _backingStoreObject.get(this);
  }
}

StorageArea.prototype[Symbol.asyncIterator] = StorageArea.prototype.entries;

export const storage = new StorageArea(DEFAULT_STORAGE_AREA_NAME);

async function performDatabaseOperation(area, mode, steps) {
  if (!_databasePromise.has(area)) {
    throw new TypeError('Invalid this value');
  }

  if (_databasePromise.get(area) === null) {
    initializeDatabasePromise(area);
  }

  const database = await _databasePromise.get(area);
  const transaction = database.transaction(DEFAULT_IDB_STORE_NAME, mode);
  const store = transaction.objectStore(DEFAULT_IDB_STORE_NAME);

  return steps(transaction, store);
}

function initializeDatabasePromise(area) {
  const databaseName = _databaseName.get(area);

  _databasePromise.set(
      area, new Promise((resolve, reject) => {
        const request = self.indexedDB.open(databaseName, 1);

        request.onsuccess = () => {
          const database = request.result;

          if (!checkDatabaseSchema(database, databaseName, reject)) {
            return;
          }

          database.onclose = () => _databasePromise.set(area, null);
          database.onversionchange = () => {
            database.close();
            _databasePromise.set(area, null);
          };
          resolve(database);
        };

        request.onerror = () => reject(request.error);

        request.onupgradeneeded = () => {
          try {
            request.result.createObjectStore(DEFAULT_IDB_STORE_NAME);
          } catch (e) {
            reject(e);
          }
        };
      }));
}

function checkDatabaseSchema(database, databaseName, reject) {
  if (database.objectStoreNames.length !== 1) {
    reject(new DOMException(
        `KV storage database "${databaseName}" corrupted: there are ` +
            `${database.objectStoreNames.length} object stores, instead of ` +
            `the expected 1.`,
        'InvalidStateError'));
    return false;
  }

  if (database.objectStoreNames[0] !== DEFAULT_IDB_STORE_NAME) {
    reject(new DOMException(
        `KV storage database "${databaseName}" corrupted: the object store ` +
            `is named "${database.objectStoreNames[0]}" instead of the ` +
            `expected "${DEFAULT_IDB_STORE_NAME}".`,
        'InvalidStateError'));
    return false;
  }

  const transaction = database.transaction(DEFAULT_IDB_STORE_NAME, 'readonly');
  const store = transaction.objectStore(DEFAULT_IDB_STORE_NAME);

  if (store.autoIncrement !== false || store.keyPath !== null ||
      store.indexNames.length !== 0) {
    reject(new DOMException(
        `KV storage database "${databaseName}" corrupted: the ` +
            `"${DEFAULT_IDB_STORE_NAME}" object store has a non-default ` +
            `schema.`,
        'InvalidStateError'));
    return false;
  }

  return true;
}
