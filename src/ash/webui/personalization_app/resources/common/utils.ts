// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utility functions to be shared between trusted and untrusted
 * code.
 */

/**
 * Checks if argument is an array with non-zero length.
 */
export function isNonEmptyArray(maybeArray: unknown): maybeArray is unknown[] {
  return Array.isArray(maybeArray) && maybeArray.length > 0;
}

/**
 * Checks if argument is null or is an array.
 */
export function isNullOrArray(maybeNullOrArray: unknown):
    maybeNullOrArray is unknown[]|null {
  return maybeNullOrArray === null || Array.isArray(maybeNullOrArray);
}

/** Checks if argument is null or is a bigint. */
export function isNullOrBigint(maybeNullOrBigint: unknown):
    maybeNullOrBigint is bigint|null {
  return maybeNullOrBigint === null || typeof maybeNullOrBigint === 'bigint';
}

/**
 * Checks if argument is null or is a number.
 */
export function isNullOrNumber(maybeNullOrNumber: unknown):
    maybeNullOrNumber is number|null {
  return maybeNullOrNumber === null || typeof maybeNullOrNumber === 'number';
}

/**
 * Attach a listener to a child element onload function. Returns a promise
 * that resolves when that child element is loaded.
 * `id` Id of the child element.
 * `afterNextRender` callback for first render of element.
 */
export function promisifyOnload(
    element: HTMLElement, id: string,
    afterNextRender: (element: HTMLElement, callback: (...args: any) => void) =>
        void): Promise<HTMLElement> {
  const promise = new Promise<HTMLElement>((resolve) => {
    function readyCallback() {
      if (!element.shadowRoot) {
        return;
      }
      const child = element.shadowRoot.getElementById(id);
      if (!child) {
        return;
      }
      child.onload = () => resolve(child);
    }
    afterNextRender(element, readyCallback);
  });
  return promise;
}


type WallpaperSelectionEvent =
    MouseEvent&{type: 'click'}|KeyboardEvent&{key: 'Enter'};
/**
 * Returns true if this event is a user action to select an item.
 */
export function isSelectionEvent(event: Event):
    event is WallpaperSelectionEvent {
  return (event instanceof MouseEvent && event.type === 'click') ||
      (event instanceof KeyboardEvent && event.key === 'Enter');
}

/**
 * Sets a css variable to control the animation delay.
 */
export function getLoadingPlaceholderAnimationDelay(index: number): string {
  return `--animation-delay: ${index * 83}ms;`;
}

/**
 * Returns the number of grid items to render per row given the current inner
 * width of the |window|.
 */
export function getNumberOfGridItemsPerRow(): number {
  return window.innerWidth > 688 ? 4 : 3;
}

/**
 * Normalizes the given |key| for RTL.
 */
export function normalizeKeyForRTL(key: string, isRTL: boolean): string {
  if (isRTL) {
    if (key === 'ArrowLeft') {
      return 'ArrowRight';
    }
    if (key === 'ArrowRight') {
      return 'ArrowLeft';
    }
  }
  return key;
}
