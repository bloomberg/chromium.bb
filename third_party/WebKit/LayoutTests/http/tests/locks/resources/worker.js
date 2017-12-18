'use strict';

// Map of id => function that releases a lock.

const held = new Map();

self.addEventListener('message', e => {
  function respond(data) {
    self.postMessage(Object.assign(data, {rqid: e.data.rqid}));
  }

  switch (e.data.op) {
  case 'request':
    navigator.locks.acquire(
      e.data.name, {
        mode: e.data.mode || 'exclusive',
        ifAvailable: e.data.ifAvailable || false
      }, lock => {
        if (lock === null) {
          respond({ack: 'request', id: e.data.lock_id, failed: true});
          return;
        }
        let release;
        const promise = new Promise(r => { release = r; });
        held.set(e.data.lock_id, release);
        respond({ack: 'request', id: e.data.lock_id});
        return promise;
      });
    break;

  case 'release':
    held.get(e.data.lock_id)();
    held.delete(e.data.lock_id);
    respond({ack: 'release', id: e.data.lock_id});
    break;
  }
});
