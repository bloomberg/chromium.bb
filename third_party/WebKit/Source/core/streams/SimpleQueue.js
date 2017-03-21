// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Queue implementation used by ReadableStream and WritableStream.

(function(global, binding, v8) {
  'use strict';

  // Simple queue structure. Avoids scalability issues with using
  // InternalPackedArray directly by using multiple arrays in a linked list and
  // keeping the array size bounded.
  const QUEUE_MAX_ARRAY_SIZE = 16384;
  class SimpleQueue {
    constructor() {
      this.front = {
        elements: new v8.InternalPackedArray(),
        next: undefined,
      };
      this.back = this.front;
      // The cursor is used to avoid calling InternalPackedArray.shift().
      this.cursor = 0;
      this.size = 0;
    }

    get length() {
      return this.size;
    }

    push(element) {
      ++this.size;
      if (this.back.elements.length === QUEUE_MAX_ARRAY_SIZE) {
        const oldBack = this.back;
        this.back = {
          elements: new v8.InternalPackedArray(),
          next: undefined,
        };
        oldBack.next = this.back;
      }
      this.back.elements.push(element);
    }

    shift() {
      // assert(this.size > 0);
      --this.size;
      if (this.front.elements.length === this.cursor) {
        // assert(this.cursor === QUEUE_MAX_ARRAY_SIZE);
        // assert(this.front.next !== undefined);
        this.front = this.front.next;
        this.cursor = 0;
      }
      const element = this.front.elements[this.cursor];
      // Permit shifted element to be garbage collected.
      this.front.elements[this.cursor] = undefined;
      ++this.cursor;

      return element;
    }

    forEach(callback) {
      let i = this.cursor;
      let node = this.front;
      let elements = node.elements;
      while (i !== elements.length || node.next !== undefined) {
        if (i === elements.length) {
          // assert(node.next !== undefined);
          // assert(i === QUEUE_MAX_ARRAY_SIZE);
          node = node.next;
          elements = node.elements;
          i = 0;
        }
        callback(elements[i]);
        ++i;
      }
    }

    // Return the element that would be returned if shift() was called now,
    // without modifying the queue.
    peek() {
      // assert(this.size > 0);
      if (this.front.elements.length === this.cursor) {
        // assert(this.cursor === QUEUE_MAX_ARRAY_SIZE)
        // assert(this.front.next !== undefined);
        return this.front.next.elements[0];
      }
      return this.front.elements[this.cursor];
    }
  }

  binding.SimpleQueue = SimpleQueue;
});
