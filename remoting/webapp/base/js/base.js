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
  } catch (/** @type {Error} */ error) {
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
 * @constructor
 * @param {...base.Disposable} var_args
 * @implements {base.Disposable}
 */
base.Disposables = function(var_args) {
  /**
   * @type {Array<base.Disposable>}
   * @private
   */
  this.disposables_ = Array.prototype.slice.call(arguments, 0);
};

/**
 * @param {...base.Disposable} var_args
 */
base.Disposables.prototype.add = function(var_args) {
  var disposables = Array.prototype.slice.call(arguments, 0);
  for (var i = 0; i < disposables.length; i++) {
    var current = /** @type {base.Disposable} */ (disposables[i]);
    if (this.disposables_.indexOf(current) === -1) {
      this.disposables_.push(current);
    }
  }
};

/**
 * @param {...base.Disposable} var_args  Dispose |var_args| and remove
 *    them from the current object.
 */
base.Disposables.prototype.remove = function(var_args) {
  var disposables = Array.prototype.slice.call(arguments, 0);
  for (var i = 0; i < disposables.length; i++) {
    var disposable = /** @type {base.Disposable} */ (disposables[i]);
    var index = this.disposables_.indexOf(disposable);
    if(index !== -1) {
      this.disposables_.splice(index, 1);
      disposable.dispose();
    }
  }
};

base.Disposables.prototype.dispose = function() {
  for (var i = 0; i < this.disposables_.length; i++) {
    this.disposables_[i].dispose();
  }
  this.disposables_ = null;
};

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
 * @suppress {checkTypes|reportUnknownTypes}
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

/**
 * @param {*} value
 * @return {*} a recursive copy of |value| or null if |value| is not copyable
 *   (e.g. undefined, NaN).
 */
base.deepCopy = function(value) {
  try {
    return JSON.parse(JSON.stringify(value));
  } catch (e) {}
  return null;
};

/**
 * @type {boolean|undefined}
 * @private
 */
base.isAppsV2_ = undefined;

/**
 * @return {boolean} True if this is a v2 app; false if it is a legacy app.
 */
base.isAppsV2 = function() {
  if (base.isAppsV2_ === undefined) {
    var manifest = chrome.runtime.getManifest();
    base.isAppsV2_ =
        Boolean(manifest && manifest.app && manifest.app.background);
  }
  return base.isAppsV2_;
};

/**
 * Joins the |url| with optional query parameters defined in |opt_params|
 * See unit test for usage.
 * @param {string} url
 * @param {Object<string>=} opt_params
 * @return {string}
 */
base.urlJoin = function(url, opt_params) {
  if (!opt_params) {
    return url;
  }
  var queryParameters = [];
  for (var key in opt_params) {
    queryParameters.push(encodeURIComponent(key) + "=" +
                         encodeURIComponent(opt_params[key]));
  }
  return url + '?' + queryParameters.join('&');
};


/**
 * @return {Object<string, string>} The URL parameters.
 */
base.getUrlParameters = function() {
  var result = {};
  var parts = window.location.search.substring(1).split('&');
  for (var i = 0; i < parts.length; i++) {
    var pair = parts[i].split('=');
    result[pair[0]] = decodeURIComponent(pair[1]);
  }
  return result;
};

/**
 * Convert special characters (e.g. &, < and >) to HTML entities.
 *
 * @param {string} str
 * @return {string}
 */
base.escapeHTML = function(str) {
  var div = document.createElement('div');
  div.appendChild(document.createTextNode(str));
  return div.innerHTML;
};

/**
 * Promise is a great tool for writing asynchronous code. However, the construct
 *   var p = new promise(function init(resolve, reject) {
 *     ... // code that fulfills the Promise.
 *   });
 * forces the Promise-resolving logic to reside in the |init| function
 * of the constructor.  This is problematic when you need to resolve the
 * Promise in a member function(which is quite common for event callbacks).
 *
 * base.Deferred comes to the rescue.  It encapsulates a Promise
 * object and exposes member methods (resolve/reject) to fulfill it.
 *
 * Here are the recommended steps to follow when implementing an asynchronous
 * function that returns a Promise:
 * 1. Create a deferred object by calling
 *      var deferred = new base.Deferred();
 * 2. Call deferred.resolve() when the asynchronous operation finishes.
 * 3. Call deferred.reject() when the asynchronous operation fails.
 * 4. Return deferred.promise() to the caller so that it can subscribe
 *    to status changes using the |then| handler.
 *
 * Sample Usage:
 *  function myAsyncAPI() {
 *    var deferred = new base.Deferred();
 *    window.setTimeout(function() {
 *      deferred.resolve();
 *    }, 100);
 *    return deferred.promise();
 *  };
 *
 * @constructor
 * @template T
 */
base.Deferred = function() {
  /**
   * @private {?function(?):void}
   */
  this.resolve_ = null;

  /**
   * @private {?function(?):void}
   */
  this.reject_ = null;

  /**
   * @this {base.Deferred}
   * @param {function(?):void} resolve
   * @param {function(*):void} reject
   */
  var initPromise = function(resolve, reject) {
    this.resolve_ = resolve;
    this.reject_ = reject;
  };

  /**
   * @private {!Promise<T>}
   */
  this.promise_ = new Promise(initPromise.bind(this));
};

/** @param {*} reason */
base.Deferred.prototype.reject = function(reason) {
  this.reject_(reason);
};

/** @param {*=} opt_value */
base.Deferred.prototype.resolve = function(opt_value) {
  this.resolve_(opt_value);
};

/** @return {!Promise<T>} */
base.Deferred.prototype.promise = function() {
  return this.promise_;
};

base.Promise = function() {};

/**
 * @param {number} delay
 * @return {Promise} a Promise that will be fulfilled after |delay| ms.
 */
base.Promise.sleep = function(delay) {
  return new Promise(
    /** @param {function(*):void} fulfill */
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
 * Converts a |method| with callbacks into a Promise.
 *
 * @param {Function} method
 * @param {Array} params
 * @param {*=} opt_context
 * @param {boolean=} opt_hasErrorHandler whether the method has an error handler
 * @return {Promise}
 */
base.Promise.as = function(method, params, opt_context, opt_hasErrorHandler) {
  return new Promise(function(resolve, reject) {
    params.push(resolve);
    if (opt_hasErrorHandler) {
      params.push(reject);
    }
    try {
      method.apply(opt_context, params);
    } catch (/** @type {*} */ e) {
      reject(e);
    }
  });
};

/**
 * A mixin for classes with events.
 *
 * For example, to create an alarm event for SmokeDetector:
 * functionSmokeDetector() {
 *    this.defineEvents(['alarm']);
 * };
 * base.extend(SmokeDetector, base.EventSourceImpl);
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
  /** @type {Array<function():void>} */
  this.listeners = [];
};


/** @interface */
base.EventSource = function() {};

 /**
  * Add a listener |fn| to listen to |type| event.
  * @param {string} type
  * @param {Function} fn
  */
base.EventSource.prototype.addEventListener = function(type, fn) {};

 /**
  * Remove a listener |fn| to listen to |type| event.
  * @param {string} type
  * @param {Function} fn
  */
base.EventSource.prototype.removeEventListener = function(type, fn) {};


/**
  * @constructor
  * Since this class is implemented as a mixin, the constructor may not be
  * called.  All initializations should be done in defineEvents.
  * @implements {base.EventSource}
  */
base.EventSourceImpl = function() {
  /** @type {Object<string, base.EventEntry>} */
  this.eventMap_;
};

/**
  * @param {base.EventSourceImpl} obj
  * @param {string} type
  */
base.EventSourceImpl.isDefined = function(obj, type) {
  base.debug.assert(Boolean(obj.eventMap_),
                   "The object doesn't support events");
  base.debug.assert(Boolean(obj.eventMap_[type]), 'Event <' + type +
    '> is undefined for the current object');
};

base.EventSourceImpl.prototype = {
  /**
    * Define |events| for this event source.
    * @param {Array<string>} events
    */
  defineEvents: function(events) {
    base.debug.assert(!Boolean(this.eventMap_),
                     'defineEvents can only be called once.');
    this.eventMap_ = {};
    events.forEach(
      /**
        * @this {base.EventSourceImpl}
        * @param {string} type
        */
      function(type) {
        base.debug.assert(typeof type == 'string');
        this.eventMap_[type] = new base.EventEntry();
    }, this);
  },

  /**
    * @param {string} type
    * @param {Function} fn
    */
  addEventListener: function(type, fn) {
    base.debug.assert(typeof fn == 'function');
    base.EventSourceImpl.isDefined(this, type);

    var listeners = this.eventMap_[type].listeners;
    listeners.push(fn);
  },

  /**
    * @param {string} type
    * @param {Function} fn
    */
  removeEventListener: function(type, fn) {
    base.debug.assert(typeof fn == 'function');
    base.EventSourceImpl.isDefined(this, type);

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
    base.EventSourceImpl.isDefined(this, type);

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


/**
  * A lightweight object that helps manage the lifetime of an event listener.
  *
  * For example, do the following if you want to automatically unhook events
  * when your object is disposed:
  *
  * var MyConstructor = function(domElement) {
  *   this.eventHooks_ = new base.Disposables(
  *     new base.EventHook(domElement, 'click', this.onClick_.bind(this)),
  *     new base.EventHook(domElement, 'keydown', this.onClick_.bind(this)),
  *     new base.ChromeEventHook(chrome.runtime.onMessage,
  *                              this.onMessage_.bind(this))
  *   );
  * }
  *
  * MyConstructor.prototype.dispose = function() {
  *   this.eventHooks_.dispose();
  *   this.eventHooks_ = null;
  * }
  *
  * @param {base.EventSource} src
  * @param {string} eventName
  * @param {Function} listener
  *
  * @constructor
  * @implements {base.Disposable}
  */
base.EventHook = function(src, eventName, listener) {
  this.src_ = src;
  this.eventName_ = eventName;
  this.listener_ = listener;
  src.addEventListener(eventName, listener);
};

base.EventHook.prototype.dispose = function() {
  this.src_.removeEventListener(this.eventName_, this.listener_);
};

/**
  * An event hook implementation for DOM Events.
  *
  * @param {HTMLElement|Element|Window|HTMLDocument} src
  * @param {string} eventName
  * @param {Function} listener
  * @param {boolean} capture
  *
  * @constructor
  * @implements {base.Disposable}
  */
base.DomEventHook = function(src, eventName, listener, capture) {
  this.src_ = src;
  this.eventName_ = eventName;
  this.listener_ = listener;
  this.capture_ = capture;
  src.addEventListener(eventName, listener, capture);
};

base.DomEventHook.prototype.dispose = function() {
  this.src_.removeEventListener(this.eventName_, this.listener_, this.capture_);
};


/**
  * An event hook implementation for Chrome Events.
  *
  * @param {chrome.Event} src
  * @param {Function} listener
  *
  * @constructor
  * @implements {base.Disposable}
  */
base.ChromeEventHook = function(src, listener) {
  this.src_ = src;
  this.listener_ = listener;
  src.addListener(listener);
};

base.ChromeEventHook.prototype.dispose = function() {
  this.src_.removeListener(this.listener_);
};

/**
 * A disposable repeating timer.
 *
 * @constructor
 * @implements {base.Disposable}
 */
base.RepeatingTimer = function(/** Function */callback, /** number */interval) {
  /** @private */
  this.intervalId_ = window.setInterval(callback, interval);
};

base.RepeatingTimer.prototype.dispose = function() {
  window.clearInterval(this.intervalId_);
  this.intervalId_ = null;
};

/**
  * Converts UTF-8 string to ArrayBuffer.
  *
  * @param {string} string
  * @return {ArrayBuffer}
  */
base.encodeUtf8 = function(string) {
  var utf8String = unescape(encodeURIComponent(string));
  var result = new Uint8Array(utf8String.length);
  for (var i = 0; i < utf8String.length; i++)
    result[i] = utf8String.charCodeAt(i);
  return result.buffer;
};

/**
  * Decodes UTF-8 string from ArrayBuffer.
  *
  * @param {ArrayBuffer} buffer
  * @return {string}
  */
base.decodeUtf8 = function(buffer) {
  return decodeURIComponent(
      escape(String.fromCharCode.apply(null, new Uint8Array(buffer))));
};

/**
 * Generate a nonce, to be used as an xsrf protection token.
 *
 * @return {string} A URL-Safe Base64-encoded 128-bit random value. */
base.generateXsrfToken = function() {
  var random = new Uint8Array(16);
  window.crypto.getRandomValues(random);
  var base64Token = window.btoa(String.fromCharCode.apply(null, random));
  return base64Token.replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '');
};

/**
 * @param {string} jsonString A JSON-encoded string.
 * @return {Object|undefined} The decoded object, or undefined if the string
 *     cannot be parsed.
 */
base.jsonParseSafe = function(jsonString) {
  try {
    return /** @type {Object} */ (JSON.parse(jsonString));
  } catch (err) {
    return undefined;
  }
};

/**
 * Size the current window to fit its content vertically.
 */
base.resizeWindowToContent = function() {
  var appWindow = chrome.app.window.current();
  var outerBounds = appWindow.outerBounds;
  var borderY = outerBounds.height - appWindow.innerBounds.height;
  appWindow.resizeTo(outerBounds.width, document.body.clientHeight + borderY);
  // Sometimes, resizing the window causes its position to be reset to (0, 0),
  // so restore it explicitly.
  appWindow.moveTo(outerBounds.left, outerBounds.top);
};
