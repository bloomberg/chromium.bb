/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';

import {assert} from 'chai';
import ResultChannelSender from './result-channel-sender.js';
import ResultChannelReceiver from './result-channel-receiver.js';

suite('ResultChannelReceiver', function() {
  test('pass errors through', async function() {
    const url = 'pass errors through';
    const sender = new ResultChannelSender(url);
    const receiver = new ResultChannelReceiver(url);
    sender.send((async function* () {
      throw new Error('occam');
    })());

    // Start the receiver after the sender has sent its messages.
    // The receiver should queue all messages until the caller is ready for
    // them.
    const results = [];
    let error;
    try {
      for await (const result of receiver) {
        results.push(result);
      }
    } catch (err) {
      error = err.message;
    }
    assert.lengthOf(results, 0);
    assert.strictEqual('occam', error);
  });

  test('receives two queued messages', async() => {
    const url = 'receives two queued messages';
    const sender = new ResultChannelSender(url);
    const receiver = new ResultChannelReceiver(url);
    sender.send((async function* () {
      yield 'foo';
      yield 'bar';
    })());

    // Start the receiver after the sender has sent its messages.
    // The receiver should queue all messages until the caller is ready for
    // them.
    const results = [];
    for await (const result of receiver) {
      results.push(result);
    }
    assert.deepEqual(results, ['foo', 'bar']);
  });

  test('receives two delayed messages', async() => {
    const url = 'receives two delayed messages';
    const sender = new ResultChannelSender(url);
    const receiver = new ResultChannelReceiver(url);

    // Start the receiver before the sender has sent any messages.
    // The receiver should block until the sender sends messages.
    const results = [];
    const receiverDone = (async() => {
      for await (const result of receiver) {
        results.push(result);
      }
    })();

    sender.send((async function* () {
      yield 'foo';
      yield 'bar';
    })());

    await receiverDone;
    assert.deepEqual(results, ['foo', 'bar']);
  });

  test('receives one queued message and one delayed message', async() => {
    const url = 'receives one queued message and one delayed message';
    const receiver = new ResultChannelReceiver(url);
    const sender = new ResultChannelSender(url);

    let resolveDelay;
    sender.send((async function* () {
      yield 'foo';
      await new Promise(resolve => {
        resolveDelay = resolve;
      });
      yield 'bar';
    })());

    // Start the receiver after the sender has sent one message but before it
    // has sent the second.
    // The receiver should yield the first message immediately, then block until
    // the second message is sent.
    const results = [];
    let resolveFirst;
    const receivedFirst = new Promise(resolve => {
      resolveFirst = resolve;
    });
    const receiverDone = (async() => {
      for await (const result of receiver) {
        results.push(result);
        resolveFirst();
      }
    })();

    await receivedFirst;
    assert.deepEqual(results, ['foo']);

    resolveDelay();
    await receiverDone;
    assert.deepEqual(results, ['foo', 'bar']);
  });

  test('queued unknown message type', async() => {
    const url = 'unknown message type';
    const receiver = new ResultChannelReceiver(url);
    const channel = new BroadcastChannel(url);
    channel.postMessage({type: 'unknown'});
    let error;
    try {
      for await (const result of receiver) {
      }
    } catch (err) {
      error = err.message;
    }
    assert.strictEqual('Unknown message type: unknown', error);
  });

  test('delayed unknown message type', async() => {
    const url = 'unknown message type';
    const receiver = new ResultChannelReceiver(url);
    const channel = new BroadcastChannel(url);
    let error;
    const receiverDone = (async() => {
      try {
        for await (const result of receiver) {
        }
      } catch (err) {
        error = err.message;
      }
    })();
    channel.postMessage({type: 'unknown'});
    await receiverDone;
    assert.strictEqual('Unknown message type: unknown', error);
  });

  test('send after close', async() => {
    const url = 'unknown message type';
    const receiver = new ResultChannelReceiver(url);
    const channel = new BroadcastChannel(url);
    channel.postMessage({type: 'DONE'});
    channel.postMessage({type: 'RESULT', payload: 'post facto'});
    const results = [];
    const receiverDone = (async() => {
      for await (const result of receiver) {
        results.push(result);
      }
    })();
    assert.lengthOf(results, 0);
  });
});
