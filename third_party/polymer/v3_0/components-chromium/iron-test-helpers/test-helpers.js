/**
 * @license
 * Copyright (c) 2015 The Polymer Project Authors. All rights reserved.
 * This code may only be used under the BSD style license found at
 * http://polymer.github.io/LICENSE.txt The complete set of authors may be found
 * at http://polymer.github.io/AUTHORS.txt The complete set of contributors may
 * be found at http://polymer.github.io/CONTRIBUTORS.txt Code distributed by
 * Google as part of the polymer project is also subject to an additional IP
 * rights grant found at http://polymer.github.io/PATENTS.txt
 */

import {dom, flush} from '../polymer/lib/legacy/polymer.dom.js';

/**
 * Forces distribution of light children, and lifecycle callbacks on the
 * Custom Elements polyfill. Used when testing elements that rely on their
 * distributed children.
 */
export function flushAsynchronousOperations() {
  // force distribution
  flush();
  // force lifecycle callback to fire on polyfill
  window.CustomElements && window.CustomElements.takeRecords();
}

/**
 * Stamps and renders a `dom-if` template.
 *
 * @param {!Element} node The node containing the template,
 */
export function forceXIfStamp(node) {
  const templates = dom(node.root).querySelectorAll('template[is=dom-if]');
  for (let tmpl, i = 0; tmpl = templates[i]; i++) {
    tmpl.render();
  }

  flushAsynchronousOperations();
}

/**
 * Fires a custom event on a specific node. This event bubbles and is
 * cancellable.
 *
 * @param {string} type The type of event.
 * @param {?Object} props Any custom properties the event contains.
 * @param {!Element} node The node to fire the event on.
 */
export function fireEvent(type, props, node) {
  const event = new CustomEvent(type, {bubbles: true, cancelable: true});
  for (const p in props) {
    event[p] = props[p];
  }
  node.dispatchEvent(event);
}

/**
 * Skips a test unless a condition is met. Sample use:
 *    function isNotIE() {
 *      return !navigator.userAgent.match(/MSIE/i);
 *    }
 *    test('runs on non IE browsers', skipUnless(isNotIE, function() {
 *      ...
 *    });
 *
 * @param {Function} condition The name of a Boolean function determining if the
 * test should be run.
 * @param {Function} test The test to be run.
 */
export function skipUnless(condition, test) {
  const isAsyncTest = !!test.length;

  return (done) => {
    if (!condition()) {
      return done();
    }

    const result = test.call(this, done);

    if (!isAsyncTest) {
      done();
    }

    return result;
  };
}

/**
 * The globals below are provided for backwards compatibility and will be
 * removed in the next major version. All users should migrate to importing
 * functions directly from this module instead of accessing them via globals.
 */
window.TestHelpers = {
  flushAsynchronousOperations,
  forceXIfStamp,
  fireEvent,
  skipUnless,
};

window.flushAsynchronousOperations = flushAsynchronousOperations;
window.forceXIfStamp = forceXIfStamp;
window.fireEvent = fireEvent;
window.skipUnless = skipUnless;
