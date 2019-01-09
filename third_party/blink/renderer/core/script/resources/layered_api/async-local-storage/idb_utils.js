// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

export function promiseForRequest(request) {
  return new Promise((resolve, reject) => {
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  });
}

export function promiseForTransaction(transaction) {
  return new Promise((resolve, reject) => {
    transaction.oncomplete = () => resolve();
    transaction.onabort = () => reject(transaction.error);
    transaction.onerror = () => reject(transaction.error);
  });
}

export function throwForDisallowedKey(key) {
  if (!isAllowedAsAKey(key)) {
    throw new DOMException(
        'The given value is not allowed as a key', 'DataError');
  }
}

function isAllowedAsAKey(value) {
  if (typeof value === 'number' || typeof value === 'string') {
    return true;
  }

  if (Array.isArray(value)) {
    return true;
  }

  if (isDate(value)) {
    return true;
  }

  if (ArrayBuffer.isView(value)) {
    return true;
  }

  if (isArrayBuffer(value)) {
    return true;
  }

  return false;
}

function isDate(value) {
  try {
    Date.prototype.getTime.call(value);
    return true;
  } catch {
    return false;
  }
}

const byteLengthGetter =
    Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, 'byteLength').get;
function isArrayBuffer(value) {
  try {
    byteLengthGetter.call(value);
    return true;
  } catch {
    return false;
  }
}
