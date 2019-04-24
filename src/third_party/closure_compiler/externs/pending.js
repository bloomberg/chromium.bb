// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Externs for stuff not added to the Closure compiler yet, but
 * should get added.
 * @externs
 */

/**
 * @see https://drafts.fxtf.org/geometry-1/#domrectreadonly
 * TODO(scottchen): Remove this once it is added to Closure Compiler itself.
 */
class DOMRectReadOnly {
  /**
   * @param {number} x
   * @param {number} y
   * @param {number} width
   * @param {number} height
   */
  constructor(x, y, width, height) {
    /** @type {number} */
    this.x;
    /** @type {number} */
    this.y;
    /** @type {number} */
    this.width;
    /** @type {number} */
    this.height;
    /** @type {number} */
    this.top;
    /** @type {number} */
    this.right;
    /** @type {number} */
    this.bottom;
    /** @type {number} */
    this.left;
  }

  /**
   * @param {{x: number, y: number, width: number, height: number}=} rectangle
   * @return {DOMRectReadOnly}
   */
  fromRect(rectangle) {}
}

/**
 * TODO(dstockwell): Remove this once it is added to Closure Compiler itself.
 * @see https://drafts.fxtf.org/geometry/#DOMMatrix
 */
class DOMMatrix {
  /**
   * @param {number} x
   * @param {number} y
   */
  translateSelf(x, y) {}
  /**
   * @param {number} x
   * @param {number} y
   * @param {number} z
   */
  rotateSelf(x, y, z) {}
  /**
   * @param {number} x
   * @param {number} y
   */
  scaleSelf(x, y) {}
  /**
   * @param {{x: number, y: number}} point
   * @return {{x: number, y: number}}
   */
  transformPoint(point) {}
}

/**
 * @see https://wicg.github.io/ResizeObserver/#resizeobserverentry
 * @typedef {{contentRect: DOMRectReadOnly,
 *            target: Element}}
 * TODO(scottchen): Remove this once it is added to Closure Compiler itself.
 */
let ResizeObserverEntry;

/**
 * @see https://wicg.github.io/ResizeObserver/#api
 * TODO(scottchen): Remove this once it is added to Closure Compiler itself.
 */
class ResizeObserver {
  /**
   * @param {!function(Array<ResizeObserverEntry>, ResizeObserver)} callback
   */
  constructor(callback) {}

  disconnect() {}

  /** @param {Element} target */
  observe(target) {}

  /** @param {Element} target */
  unobserve(target) {}
}

/**
 * @see
 * https://www.polymer-project.org/2.0/docs/api/namespaces/Polymer.RenderStatus
 * Queue a function call to be run before the next render.
 * @param {!Element} element The element on which the function call is made.
 * @param {!function()} fn The function called on next render.
 * @param {...*} args The function arguments.
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.RenderStatus.beforeNextRender = function(element, fn, args) {};

/**
 * @see
 * https://www.webcomponents.org/element/@polymer/iron-iconset-svg
 * Polymer iconset of SVGs.
 * @implements {Polymer.Iconset}
 * @constructor
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.IronIconsetSvg = function() {};

/**
 * Added to IronIconsetSvg in chromium.patch.
 * @param {string} iconName Name of the icon to apply.
 * @param {boolean} targetIsRTL Whether the target element is RTL.
 * @return {Element} Returns an installable clone of the SVG element
 *     matching `id`.
 * TODO(rbpotter): Remove this once it is added to Closure Compiler itself.
 */
Polymer.IronIconsetSvg.prototype.createIcon = function(iconName, targetIsRTL) {};

/**
 * @see
 * https://github.com/tc39/proposal-bigint
 * This supports wrapping and operating on arbitrarily large integers.
 *
 * @param {number} value
 */
let BigInt = function(value) {};

/** @const {!Clipboard} */
Navigator.prototype.clipboard;

/**
 * TODO(manukh): remove this once it is added to Closure Compiler itself.
 * @see https://tc39.github.io/proposal-flatMap/#sec-Array.prototype.flatMap
 * @param {?function(this:S, T, number, !Array<T>): R} callback
 * @param {S=} opt_this
 * @return {!Array<R>}
 * @this {IArrayLike<T>|string}
 * @template T,S,R
 * @see https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/flatMap
 */
Array.prototype.flatMap = function(callback, opt_this) {};

/**
 * TODO(katie): Remove this once length is added to the Closure
 * chrome_extensions.js.
 * An event from the TTS engine to communicate the status of an utterance.
 * @constructor
 */
function TtsEvent() {}

/** @type {number} */
TtsEvent.prototype.length;
