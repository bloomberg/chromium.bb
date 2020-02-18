/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import * as testUtils from './cache-request-base.js';
import idb from 'idb';
import {ResultChannelReceiver} from '@chopsui/result-channel';
import {SessionIdCacheRequest} from './session-id-cache-request.js';
import {assert} from 'chai';

class MockFetchEvent {
  constructor(state) {
    const body = new Map([['page_state', JSON.stringify(state)]]);
    this.request = {
      method: 'POST',
      url: 'http://example.com/path',
      clone: () => this.request,
      formData: () => body,
    };
  }
  respondWith(promise) {
    this.response = promise.then(r => r.json());
  }

  waitUntil(promise) {
    this.waitUntil_ = promise;
  }

  async wait() {
    return await this.waitUntil_;
  }
}

async function clearData() {
  const db = await idb.open('short_uri', 1, db => db.createObjectStore('sids'));
  const transaction = db.transaction(['sids'], 'readwrite');
  const dataStore = transaction.objectStore('sids');
  await dataStore.clear();
}

suite('SessionIdCacheRequest', function() {
  let originalFetch;
  setup(() => {
    originalFetch = window.fetch;
  });
  teardown(() => {
    window.fetch = originalFetch;
  });

  test('miss', async() => {
    testUtils.clearInProgressForTest();
    await clearData();
    const sid =
    '56c7b34276add8604fa4871fc2e58bb6f5cdfdaa2561ee91440ef362d8a7fae8';
    window.fetch = async() => testUtils.jsonResponse({sid});
    const fetchEvent = new MockFetchEvent('miss');
    const cacheRequest = new SessionIdCacheRequest(fetchEvent);
    await cacheRequest.respond();
    const response = await fetchEvent.response;
    assert.strictEqual(sid, response.sid);

    await testUtils.flushWriterForTest();
    const db = await idb.open('short_uri', 1, db => {
      throw new Error('databaseUpgrade was unexpectedly called');
    });
    const transaction = db.transaction(['sids'], 'readonly');
    const dataStore = transaction.objectStore('sids');
    const accessTime = await dataStore.get(sid);
    assert.instanceOf(accessTime, Date);
  });

  test('invalid', async() => {
    testUtils.clearInProgressForTest();
    await clearData();
    const sid =
    '5300ed0fabc7e270acce0ed6f1a575ec29129cc30c617e1e31540b6ebdc0e411';
    window.fetch = async() => testUtils.jsonResponse({sid: 'invalid'});
    const fetchEvent = new MockFetchEvent('invalid');
    const cacheRequest = new SessionIdCacheRequest(fetchEvent);
    const receiver = new ResultChannelReceiver(fetchEvent.request.url + '?' +
    new URLSearchParams({page_state: JSON.stringify('invalid')}));
    await cacheRequest.respond();
    await fetchEvent.wait();
    let error;
    try {
      for await (const unused of receiver) {
      }
    } catch (err) {
      error = err.message;
    }
    assert.strictEqual(`short_uri expected ${sid} actual invalid`, error);
    const response = await fetchEvent.response;
    assert.strictEqual(sid, response.sid);
  });

  test('hit', async() => {
    testUtils.clearInProgressForTest();
    await clearData();
    const sid =
    'c8b044f9d703ebaf9094056de7314e2ce70bdb3c15296dc2770146a6f62c234e';
    window.fetch = async() => {
      throw new Error('fetch was unexpectedly called in "hit"');
    };
    const db = await idb.open('short_uri', 1, db => {
      db.createObjectStore('sids');
    });
    const transaction = db.transaction(['sids'], 'readwrite');
    const dataStore = transaction.objectStore('sids');
    dataStore.put(new Date(), sid);
    await transaction.complete;

    const fetchEvent = new MockFetchEvent('hit');
    const cacheRequest = new SessionIdCacheRequest(fetchEvent);
    await cacheRequest.respond();
    const response = await fetchEvent.response;
    assert.strictEqual(sid, response.sid);
  });

  test('in progress', async() => {
    testUtils.clearInProgressForTest();
    await clearData();
    // Start two requests at the same time. Only one of them should call
    // fetch().
    const sid =
    '382d309f29a5039934f803d3dfb4ff2782de77292755d61ddfa990c69c0d1dc2';
    window.fetch = async() => {
      window.fetch = async() => {
        throw new Error('fetch was unexpectedly called in "in progress"');
      };
      return testUtils.jsonResponse({sid});
    };
    const fetchEventA = new MockFetchEvent('in progress');
    const cacheRequestA = new SessionIdCacheRequest(fetchEventA);
    const fetchEventB = new MockFetchEvent('in progress');
    const cacheRequestB = new SessionIdCacheRequest(fetchEventB);

    await Promise.all([cacheRequestA.respond(), cacheRequestB.respond()]);
    const responseA = await fetchEventA.response;
    const responseB = await fetchEventB.response;
    assert.strictEqual(sid, responseA.sid);
    assert.strictEqual(sid, responseB.sid);
  });
});
