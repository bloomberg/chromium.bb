/**
 * @fileoverview Closure compiler externs for the Polymer library.
 *
 * @externs
 * @license
 * Copyright (c) 2015 The Polymer Project Authors. All rights reserved.
 * This code may only be used under the BSD style license found at
 * http://polymer.github.io/LICENSE.txt. The complete set of authors may be
 * found at http://polymer.github.io/AUTHORS.txt. The complete set of
 * contributors may be found at http://polymer.github.io/CONTRIBUTORS.txt. Code
 * distributed by Google as part of the polymer project is also subject to an
 * additional IP rights grant found at http://polymer.github.io/PATENTS.txt.
 */

/**
 * @param {!{is: string}} descriptor The Polymer descriptor of the element.
 * @see https://github.com/Polymer/polymer/blob/0.8-preview/PRIMER.md#custom-element-registration
 */
var Polymer = function(descriptor) {};


/** @constructor @extends {HTMLElement} */
var PolymerElement = function() {};

/**
 * A mapping from ID to element in this Polymer Element's local DOM.
 * @type {!Object}
 */
PolymerElement.prototype.$;

/** @type {string} The Custom element tag name. */
PolymerElement.prototype.is;

/** @type {string} The native element this element extends. */
PolymerElement.prototype.extends;

/**
 * An array of objects whose properties get mixed in to this element.
 *
 * @type {!Array<!Object>|undefined}
 */
PolymerElement.prototype.mixins;

/**
 * A string-separated list of dependent properties that should result in a
 * change function being called. These observers differ from single-property
 * observers in that the change handler is called asynchronously.
 *
 * @type {!Object<string, string>|undefined}
 */
PolymerElement.prototype.observers;

/** On create callback. */
PolymerElement.prototype.created = function() {};
/** On ready callback. */
PolymerElement.prototype.ready = function() {};
/** On attached to the DOM callback. */
PolymerElement.prototype.attached = function() {};
/** On detached from the DOM callback. */
PolymerElement.prototype.detached = function() {};

/**
 * Callback fired when an attribute on the element has been changed.
 *
 * @param {string} name The name of the attribute that changed.
 */
PolymerElement.prototype.attributeChanged = function(name) {};

/** @typedef {!{
 *    type: !Function,
 *    reflectToAttribute: (boolean|undefined),
 *    readOnly: (boolean|undefined),
 *    notify: (boolean|undefined),
 *    value: *,
 *    computed: (string|undefined),
 *    observer: (string|undefined)
 *  }} */
PolymerElement.PropertyConfig;

/** @typedef {!Object<string, (!Function|!PolymerElement.PropertyConfig)>} */
PolymerElement.Properties;

/** @type {!PolymerElement.Properties} */
PolymerElement.prototype.properties;

/** @type {!Object<string, *>} */
PolymerElement.prototype.hostAttributes;

/**
 * An object that maps events to event handler function names.
 * @type {!Object<string, string>}
 */
PolymerElement.prototype.listeners;

/**
 * Notifies the event binding system of a change to a property.
 * @param  {string} path  The path to set.
 * @param  {*}      value The value to send in the update notification.
 */
PolymerElement.prototype.notifyPath = function(path, value) {};

/**
 * Shorthand for setting a property, then calling notifyPath.
 * @param  {string} path  The path to set.
 * @param  {*}      value The new value.
 */
PolymerElement.prototype.setPathValue = function(path, value) {};

/**
 * Fire an event.
 *
 * @param {string} type An event name.
 * @param {Object=} detail
 * @param {{
 *   bubbles: (boolean|undefined),
 *   cancelable: (boolean|undefined),
 *   node: (!HTMLElement|undefined)}=} options
 * @return {Object} event
 */
PolymerElement.prototype.fire = function(type, detail, options) {};

