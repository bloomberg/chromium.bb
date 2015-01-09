// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Provide polling-based "wait for" functionality, and defines some useful
 * predicates.
 */

'use strict';

/** @suppress {duplicate} */
var browserTest = browserTest || {};

/** @enum {number} */
browserTest.Timeout = {
  NONE: -1,
  DEFAULT: 5000
};

/** @interface */
browserTest.Predicate = function() {};

/** @return {boolean} */
browserTest.Predicate.prototype.evaluate = function() {};

/** @return {string} */
browserTest.Predicate.prototype.description = function() {};

/**
 * @param {browserTest.Predicate} predicate
 * @param {number=} opt_timeout Timeout in ms.
 * @return {Promise}
 */
browserTest.waitFor = function(predicate, opt_timeout) {
  return new Promise(function(fulfill, reject) {
    if (opt_timeout === undefined) {
      opt_timeout = browserTest.Timeout.DEFAULT;
    }
    var end = Number(new Date()) + opt_timeout;
    var testPredicate = function() {
      if (predicate.evaluate()) {
        console.log(predicate.description() + ' satisfied.');
        fulfill();
      } else if (Number(new Date()) >= end) {
        reject(new Error('Timed out (' + opt_timeout + 'ms) waiting for ' +
                         predicate.description()));
      } else {
        console.log(predicate.description() + ' not yet satisfied.');
        window.setTimeout(testPredicate, 500);
      }
    };
    testPredicate();
  });
};

/**
 * @param {string} id
 * @return {browserTest.Predicate}
 */
browserTest.isVisible = function(id) {
  var element = document.getElementById(id);
  browserTest.expect(element, 'No such element: ' + id);
  return {
    evaluate: function() {
      return element.getBoundingClientRect().width != 0;
    },
    description: function() {
      return 'isVisible(' + id + ')';
    }
  };
};

/**
 * @param {string} id
 * @return {browserTest.Predicate}
 */
browserTest.isEnabled = function(id) {
  var element = document.getElementById(id);
  browserTest.expect(element, 'No such element: ' + id);
  return {
    evaluate: function() {
      return !element.disabled;
    },
    description: function() {
      return 'isEnabled(' + id + ')';
    }
  };
};
