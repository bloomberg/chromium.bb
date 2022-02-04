// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertBoolean, assertInstanceof, assertString} from '../assert.js';

/**
 * @return The value in storage or defaultValue if not found.
 */
function getHelper(key: string, defaultValue: unknown): unknown {
  const rawValue = window.localStorage.getItem(key);
  if (rawValue === null) {
    return defaultValue;
  }
  return JSON.parse(rawValue);
}

/**
 * @return The object in storage or defaultValue if not found.
 */
export function getObject<T>(
    key: string, defaultValue: Record<string, T> = {}): Record<string, T> {
  // TODO(pihsun): actually verify the type at runtime here?
  return assertInstanceof(getHelper(key, defaultValue), Object) as
      Record<string, T>;
}

/**
 * @return The string in storage or defaultValue if not found.
 */
export function getString(key: string, defaultValue = ''): string {
  return assertString(getHelper(key, defaultValue));
}
/**
 * @return The boolean in storage or defaultValue if not found.
 */
export function getBool(key: string, defaultValue = false): boolean {
  return assertBoolean(getHelper(key, defaultValue));
}

export function set(key: string, value: unknown): void {
  window.localStorage.setItem(key, JSON.stringify(value));
}

export function remove(...keys: string[]): void {
  for (const key of keys) {
    window.localStorage.removeItem(key);
  }
}

/**
 * Clears all the items in the local storage.
 */
export function clear(): void {
  window.localStorage.clear();
}
