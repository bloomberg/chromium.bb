/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {assert} from 'chai';
import {
  CacheRequestBase,
  READONLY,
  READWRITE,
  clearInProgressForTest,
  deleteDatabaseForTest,
  flushWriterForTest,
} from './cache-request-base.js';

suite('CacheRequestBase', function() {
  class TestCacheRequest extends CacheRequestBase {
    get databaseName() {
      return 'database';
    }

    get databaseVersion() {
      return 1;
    }

    async upgradeDatabase(db) {
      if (db.oldVersion < 1) {
        db.createObjectStore('store');
      }
    }

    async getResponse() {
      return 'response';
    }

    async writeDatabase({key, value}) {
      const transaction = await this.transaction(['store'], READWRITE);
      const dataStore = transaction.objectStore('store');
      dataStore.put(value, key);
      await transaction.complete;
    }
  }

  test('databasePromise is memoized', function() {
    const request = new TestCacheRequest(
        {request: {url: 'https://example.com/pathname'}});
    assert.strictEqual(request.databasePromise, request.databasePromise);
  });

  test('transaction recreates db', async function() {
    const request = new class extends TestCacheRequest {
      get databaseName() {
        return 'transaction recreates db';
      }
    }({request: {url: 'https://example.com/pathname'}});
    assert.isDefined(await request.transaction(['store'], READONLY));
    await deleteDatabaseForTest(request.databaseName);
    assert.isDefined(await request.transaction(['store'], READONLY));
  });

  test('responsePromise is memoized', function() {
    const request = new TestCacheRequest(
        {request: {url: 'https://example.com/pathname'}});
    assert.strictEqual(request.responsePromise, request.responsePromise);
  });

  test('findInProgressRequest', async() => {
    clearInProgressForTest();
    const requestA = new TestCacheRequest(
        {request: {url: 'https://example.com/pathname'}});
    const requestB = new TestCacheRequest(
        {request: {url: 'https://example.com/pathname'}});

    assert.strictEqual(undefined, await requestA.findInProgressRequest(
        r => false));
    assert.strictEqual(undefined, await requestB.findInProgressRequest(
        r => false));

    assert.strictEqual(requestB, await requestA.findInProgressRequest(
        r => true));
    assert.strictEqual(requestA, await requestB.findInProgressRequest(
        r => true));

    requestA.onComplete();
    assert.strictEqual(undefined, await requestB.findInProgressRequest(
        r => true));
    assert.strictEqual(requestB, await requestA.findInProgressRequest(
        r => true));

    requestB.onComplete();
    assert.strictEqual(undefined, await requestA.findInProgressRequest(
        r => true));
    assert.strictEqual(undefined, await requestB.findInProgressRequest(
        r => true));
  });

  test('respond', async() => {
    let resolveResult;
    const resultPromise = new Promise(resolve => {
      resolveResult = resolve;
    });
    const request = new TestCacheRequest({
      request: {url: 'https://example.com/pathname'},
      respondWith: async promise => {
        const response = await promise;
        resolveResult(await response.json());
      },
    });
    request.respond();
    assert.strictEqual('response', await resultPromise);
  });

  test('scheduleWrite', async() => {
    const finder = new TestCacheRequest(
        {request: {url: 'https://example.com/pathname'}});
    let resolveResult;
    const resultPromise = new Promise(resolve => {
      resolveResult = resolve;
    });
    let waitUntil;
    const writer = new TestCacheRequest({
      request: {url: 'https://example.com/pathname'},
      waitUntil: promise => {
        waitUntil = promise;
      },
      respondWith: async promise => {
        const response = await promise;
        resolveResult(await response.json());
      },
    });

    // Cache requests are IN_PROGRESS from when they are created until both
    // getResponse() returns and writeDatabase() returns if scheduleWrite is
    // called.

    assert.strictEqual(writer, await finder.findInProgressRequest(r => true));
    writer.scheduleWrite({key: 'key', value: 'value'});
    assert.strictEqual(writer, await finder.findInProgressRequest(r => true));
    writer.respond();
    assert.strictEqual(writer, await finder.findInProgressRequest(r => true));
    await resultPromise;
    assert.strictEqual(writer, await finder.findInProgressRequest(r => true));

    await flushWriterForTest();
    await waitUntil;
    assert.strictEqual(undefined, await finder.findInProgressRequest(
        r => true));

    const transaction = await writer.transaction(['store'], 'readonly');
    const dataStore = transaction.objectStore('store');
    assert.strictEqual('value', await dataStore.get('key'));
  });
});
