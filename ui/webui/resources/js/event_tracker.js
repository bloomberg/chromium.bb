// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview EventTracker is a simple class that manages the addition and
 * removal of DOM event listeners. In particular, it keeps track of all
 * listeners that have been added and makes it easy to remove some or all of
 * them without requiring all the information again. This is particularly handy
 * when the listener is a generated function such as a lambda or the result of
 * calling Function.bind.
 */

/**
 * The type of the internal tracking entry. TODO(dbeam): move this back to
 * EventTracker.Entry when https://github.com/google/closure-compiler/issues/544
 * is fixed.
 * @typedef {{node: !Node,
 *            eventType: string,
 *            listener: Function,
 *            capture: boolean}}
 */
var EventTrackerEntry;

/**
 * Create an EventTracker to track a set of events.
 * EventTracker instances are typically tied 1:1 with other objects or
 * DOM elements whose listeners should be removed when the object is disposed
 * or the corresponding elements are removed from the DOM.
 * @constructor
 */
function EventTracker() {
  /**
   * @type {Array.<EventTrackerEntry>}
   * @private
   */
  this.listeners_ = [];
}

EventTracker.prototype = {
  /**
   * Add an event listener - replacement for Node.addEventListener.
   * @param {!Node} node The DOM node to add a listener to.
   * @param {string} eventType The type of event to subscribe to.
   * @param {Function} listener The listener to add.
   * @param {boolean=} opt_capture Whether to invoke during the capture phase.
   */
  add: function(node, eventType, listener, opt_capture) {
    var capture = !!opt_capture;
    var h = {
      node: node,
      eventType: eventType,
      listener: listener,
      capture: capture,
    };
    this.listeners_.push(h);
    node.addEventListener(eventType, listener, capture);
  },

  /**
   * Remove any specified event listeners added with this EventTracker.
   * @param {!Node} node The DOM node to remove a listener from.
   * @param {?string} eventType The type of event to remove.
   */
  remove: function(node, eventType) {
    this.listeners_ = this.listeners_.filter(function(h) {
      if (h.node == node && (!eventType || (h.eventType == eventType))) {
        EventTracker.removeEventListener_(h);
        return false;
      }
      return true;
    });
  },

  /**
   * Remove all event listeners added with this EventTracker.
   */
  removeAll: function() {
    this.listeners_.forEach(EventTracker.removeEventListener_);
    this.listeners_ = [];
  }
};

/**
 * Remove a single event listener given it's tracking entry. It's up to the
 * caller to ensure the entry is removed from listeners_.
 * @param {EventTrackerEntry} h The entry describing the listener to remove.
 * @private
 */
EventTracker.removeEventListener_ = function(h) {
  h.node.removeEventListener(h.eventType, h.listener, h.capture);
};
