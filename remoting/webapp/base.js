// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A module that contains basic utility components and methods for the
 * chromoting project
 *
 */

'use strict';

var base = {};
base.debug = function() {};

/**
 * Whether to break in debugger and alert when an assertion fails.
 * Set it to true for debugging.
 * @type {boolean}
 */
base.debug.breakOnAssert = false;

/**
 * Assert that |expr| is true else print the |opt_msg|.
 * @param {boolean} expr
 * @param {string=} opt_msg
 */
base.debug.assert = function(expr, opt_msg) {
  if (!expr) {
    var msg = 'Assertion Failed.';
    if (opt_msg) {
      msg += ' ' + opt_msg;
    }
    console.error(msg);
    if (base.debug.breakOnAssert) {
      alert(msg);
      debugger;
    }
  }
};

/**
 * @return {string} The callstack of the current method.
 */
base.debug.callstack = function() {
  try {
    throw new Error();
  } catch (e) {
    var error = /** @type {Error} */ e;
    var callstack = error.stack
      .replace(/^\s+(at eval )?at\s+/gm, '') // Remove 'at' and indentation.
      .split('\n');
    callstack.splice(0,2); // Remove the stack of the current function.
  }
  return callstack.join('\n');
};

/**
  * @interface
  */
base.Disposable = function() {};
base.Disposable.prototype.dispose = function() {};

/**
 * A utility function to invoke |obj|.dispose without a null check on |obj|.
 * @param {base.Disposable} obj
 */
base.dispose = function(obj) {
  if (obj) {
    base.debug.assert(typeof obj.dispose == 'function');
    obj.dispose();
  }
};

/**
 * Copy all properties from src to dest.
 * @param {Object} dest
 * @param {Object} src
 */
base.mix = function(dest, src) {
  for (var prop in src) {
    if (src.hasOwnProperty(prop)) {
      base.debug.assert(!dest.hasOwnProperty(prop),"Don't override properties");
      dest[prop] = src[prop];
    }
  }
};

/**
 * Adds a mixin to a class.
 * @param {Object} dest
 * @param {Object} src
 * @suppress {checkTypes}
 */
base.extend = function(dest, src) {
  base.mix(dest.prototype, src.prototype || src);
};

base.doNothing = function() {};

/**
 * Returns an array containing the values of |dict|.
 * @param {!Object} dict
 * @return {Array}
 */
base.values = function(dict) {
  return Object.keys(dict).map(
    /** @param {string} key */
    function(key) {
      return dict[key];
    });
};

base.Promise = function() {};

/**
 * @param {number} delay
 * @return {Promise} a Promise that will be fulfilled after |delay| ms.
 */
base.Promise.sleep = function(delay) {
  return new Promise(
    /** @param {function():void} fulfill */
    function(fulfill) {
      window.setTimeout(fulfill, delay);
    });
};


/**
 * @param {Promise} promise
 * @return {Promise} a Promise that will be fulfilled iff the specified Promise
 *     is rejected.
 */
base.Promise.negate = function(promise) {
  return promise.then(
      /** @return {Promise} */
      function() {
        return Promise.reject();
      },
      /** @return {Promise} */
      function() {
        return Promise.resolve();
      });
};

/**
 * A mixin for classes with events.
 *
 * For example, to create an alarm event for SmokeDetector:
 * functionSmokeDetector() {
 *    this.defineEvents(['alarm']);
 * };
 * base.extend(SmokeDetector, base.EventSource);
 *
 * To fire an event:
 * SmokeDetector.prototype.onCarbonMonoxideDetected = function() {
 *   var param = {} // optional parameters
 *   this.raiseEvent('alarm', param);
 * }
 *
 * To listen to an event:
 * var smokeDetector = new SmokeDetector();
 * smokeDetector.addEventListener('alarm', listenerObj.someCallback)
 *
 */

/**
  * Helper interface for the EventSource.
  * @constructor
  */
base.EventEntry = function() {
  /** @type {Array.<function():void>} */
  this.listeners = [];
};

/**
  * @constructor
  * Since this class is implemented as a mixin, the constructor may not be
  * called.  All initializations should be done in defineEvents.
  */
base.EventSource = function() {
  /** @type {Object.<string, base.EventEntry>} */
  this.eventMap_;
};

/**
  * @param {base.EventSource} obj
  * @param {string} type
  */
base.EventSource.isDefined = function(obj, type) {
  base.debug.assert(Boolean(obj.eventMap_),
                   "The object doesn't support events");
  base.debug.assert(Boolean(obj.eventMap_[type]), 'Event <' + type +
    '> is undefined for the current object');
};

base.EventSource.prototype = {
  /**
    * Define |events| for this event source.
    * @param {Array.<string>} events
    */
  defineEvents: function(events) {
    base.debug.assert(!Boolean(this.eventMap_),
                     'defineEvents can only be called once.');
    this.eventMap_ = {};
    events.forEach(
      /**
        * @this {base.EventSource}
        * @param {string} type
        */
      function(type) {
        base.debug.assert(typeof type == 'string');
        this.eventMap_[type] = new base.EventEntry();
    }, this);
  },

  /**
    * Add a listener |fn| to listen to |type| event.
    * @param {string} type
    * @param {function(?=):void} fn
    */
  addEventListener: function(type, fn) {
    base.debug.assert(typeof fn == 'function');
    base.EventSource.isDefined(this, type);

    var listeners = this.eventMap_[type].listeners;
    listeners.push(fn);
  },

  /**
    * Remove the listener |fn| from the event source.
    * @param {string} type
    * @param {function(?=):void} fn
    */
  removeEventListener: function(type, fn) {
    base.debug.assert(typeof fn == 'function');
    base.EventSource.isDefined(this, type);

    var listeners = this.eventMap_[type].listeners;
    // find the listener to remove.
    for (var i = 0; i < listeners.length; i++) {
      var listener = listeners[i];
      if (listener == fn) {
        listeners.splice(i, 1);
        break;
      }
    }
  },

  /**
    * Fire an event of a particular type on this object.
    * @param {string} type
    * @param {*=} opt_details The type of |opt_details| should be ?= to
    *     match what is defined in add(remove)EventListener.  However, JSCompile
    *     cannot handle invoking an unknown type as an argument to |listener|
    *     As a hack, we set the type to *=.
    */
  raiseEvent: function(type, opt_details) {
    base.EventSource.isDefined(this, type);

    var entry = this.eventMap_[type];
    var listeners = entry.listeners.slice(0); // Make a copy of the listeners.

    listeners.forEach(
      /** @param {function(*=):void} listener */
      function(listener){
        if (listener) {
          listener(opt_details);
        }
    });
  }
};
