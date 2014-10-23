// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Get the |key| attribute in the given |dict| and verify that it is an
 * array value.
 *
 * If the attribute is not an array, then an exception will be thrown unless
 * a default value is specified in |opt_default|.
 *
 * @param {Object.<string,*>} dict The dictionary containing the |key|
 * @param {string} key The key to typecheck in the |dict|.
 * @param {Array=} opt_default The value to return if the key is not a bool.
 * @return {Array} The |key| attribute value as an object.
 */
function getArrayAttr(dict, key, opt_default) {
  var value = /** @type {Array} */ (dict[key]);
  if (!(value instanceof Array)) {
    if (opt_default === undefined) {
      throw 'Invalid data type for ' + key +
          ' (expected: array, actual: ' + typeof value + ')';
    } else {
      return opt_default;
    }
  }
  return value;
}

/**
 * Get the |key| attribute in the given |dict| and verify that it is a
 * boolean value.
 *
 * If the attribute is not a boolean, then an exception will be thrown unless
 * a default value is specified in |opt_default|.
 *
 * @param {Object.<string,*>} dict The dictionary containing the |key|
 * @param {string} key The key to typecheck in the |dict|.
 * @param {boolean=} opt_default The value to return if the key is not a bool.
 * @return {boolean} The |key| attribute value as a boolean.
 */
function getBooleanAttr(dict, key, opt_default) {
  var value = /** @type {boolean} */ (dict[key]);
  if (typeof value != 'boolean') {
    if (opt_default === undefined) {
      throw 'Invalid data type for ' + key +
          ' (expected: boolean, actual: ' + typeof value + ')';
    } else {
      return opt_default;
    }
  }
  return value;
}

/**
 * Get the |key| attribute in the given |dict| and verify that it is a
 * number value.
 *
 * If the attribute is not a number, then an exception will be thrown unless
 * a default value is specified in |opt_default|.
 *
 * @param {Object.<string,*>} dict The dictionary containing the |key|
 * @param {string} key The key to typecheck in the |dict|.
 * @param {number=} opt_default The value to return if the key is not a number.
 * @return {number} The |key| attribute value as a number.
 */
function getNumberAttr(dict, key, opt_default) {
  var value = /** @type {number} */(dict[key]);
  if (typeof value != 'number') {
    if (opt_default === undefined) {
      throw 'Invalid data type for ' + key +
          ' (expected: number, actual: ' + typeof value + ')';
    } else {
      return opt_default;
    }
  }
  return value;
}

/**
 * Get the |key| attribute in the given |dict| and verify that it is an
 * object value.
 *
 * If the attribute is not an object, then an exception will be thrown unless
 * a default value is specified in |opt_default|.
 *
 * @param {Object.<string,*>} dict The dictionary containing the |key|
 * @param {string} key The key to typecheck in the |dict|.
 * @param {Object=} opt_default The value to return if the key is not a bool.
 * @return {Object} The |key| attribute value as an object.
 */
function getObjectAttr(dict, key, opt_default) {
  var value = /** @type {Object} */ (dict[key]);
  if (typeof value != 'object') {
    if (opt_default === undefined) {
      throw 'Invalid data type for ' + key +
          ' (expected: object, actual: ' + typeof value + ')';
    } else {
      return opt_default;
    }
  }
  return value;
}

/**
 * Get the |key| attribute in the given |dict| and verify that it is a
 * string value.
 *
 * If the attribute is not a string, then an exception will be thrown unless
 * a default value is specified in |opt_default|.
 *
 * @param {Object.<string,*>} dict The dictionary containing the |key|
 * @param {string} key The key to typecheck in the |dict|.
 * @param {string=} opt_default The value to return if the key is not a string.
 * @return {string} The |key| attribute value as a string.
 */
function getStringAttr(dict, key, opt_default) {
  var value =  /** @type {string} */ (dict[key]);
  if (typeof value != 'string') {
    if (opt_default === undefined) {
      throw 'Invalid data type for ' + key +
          ' (expected: string, actual: ' + typeof value + ')';
    } else {
      return opt_default;
    }
  }
  return value;
}

/**
 * Return a JSON object parsed from a string.
 *
 * If the string cannot be parsed, or does not result in an object, then an
 * exception will be thrown.
 *
 * @param {string} jsonString The JSON string to parse.
 * @return {Object} The JSON object created from the |jsonString|.
 */
function getJsonObjectFromString(jsonString) {
  var value = /** @type {Object} */ JSON.parse(jsonString);
  if (typeof value != 'object') {
    throw 'Invalid data type (expected: Object, actual: ' + typeof value + ')';
  }
  return value;
}
