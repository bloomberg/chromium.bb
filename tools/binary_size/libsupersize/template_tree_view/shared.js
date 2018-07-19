// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

/**
 * @fileoverview
 * Constants used by both the UI and Web Worker scripts.
 */

/**
 * @typedef {object} TreeNode Node object used to represent the file tree. Can
 * represent either a container or a symbol.
 * @prop {TreeNode[] | null} children Child tree nodes. Null values indicate
 * that there are children, but that they haven't been loaded in yet. Empty
 * arrays indicate this is a leaf node.
 * @prop {TreeNode | null} parent Parent tree node. null if this is a root node.
 * @prop {string} idPath Full path to this node.
 * @prop {number} shortNameIndex The name of the node is include in the idPath.
 * This index indicates where to start to slice the idPath to read the name.
 * @prop {number} size Byte size of this node and its children.
 * @prop {string} type Type of this node. If this node has children, the string
 * may have a second character to denote the most common child.
 * @prop {{[type: string]: {size:number,count:number}}} childStats Stats about
 * this node's descendants, organized by symbol type.
 */

/**
 * @typedef {object} TreeProgress
 * @prop {TreeNode} root Root node and its direct children.
 * @prop {number} percent Number from (0-1] to represent percentage.
 * @prop {boolean} diffMode True if we are currently showing the diff of two
 * different size files.
 * @prop {string} [error] Error message, if an error occured in the worker.
 * If unset, then there was no error.
 */

/**
 * @typedef {object} GetSizeResult
 * @prop {string} description Description of the size, shown as hover text
 * @prop {Node} element Abbreviated representation of the size, which can
 * include DOM elements for styling.
 * @prop {number} value The size number used to create the other strings.
 */
/**
 * @typedef {(node: TreeNode, unit: string) => GetSizeResult} GetSize
 */

/** Abberivated keys used by FileEntrys in the JSON data file. */
const _KEYS = Object.freeze({
  SOURCE_PATH: /** @type {'p'} */ ('p'),
  COMPONENT_INDEX: /** @type {'c'} */ ('c'),
  FILE_SYMBOLS: /** @type {'s'} */ ('s'),
  SYMBOL_NAME: /** @type {'n'} */ ('n'),
  SIZE: /** @type {'b'} */ ('b'),
  TYPE: /** @type {'t'} */ ('t'),
  COUNT: /** @type {'u'} */ ('u'),
});

/**
 * @enum {number} Various byte units and the corresponding amount of bytes
 * that one unit represents.
 */
const _BYTE_UNITS = Object.freeze({
  GiB: 1024 ** 3,
  MiB: 1024 ** 2,
  KiB: 1024 ** 1,
  B: 1024 ** 0,
});
/** Set of all byte units */
const _BYTE_UNITS_SET = new Set(Object.keys(_BYTE_UNITS));

/**
 * Special types used by containers, such as folders and files.
 */
const _CONTAINER_TYPES = {
  DIRECTORY: 'D',
  COMPONENT: 'C',
  FILE: 'F',
  JAVA_CLASS: 'J',
};
const _CONTAINER_TYPE_SET = new Set(Object.values(_CONTAINER_TYPES));

/** Type for a dex method symbol */
const _DEX_METHOD_SYMBOL_TYPE = 'm';
/** Type for an 'other' symbol */
const _OTHER_SYMBOL_TYPE = 'o';

/** Set of all known symbol types. Container types are not included. */
const _SYMBOL_TYPE_SET = new Set('bdrtv*xmpP' + _OTHER_SYMBOL_TYPE);

/** Name used by a directory created to hold symbols with no name. */
const _NO_NAME = '(No path)';

/** Key where type is stored in the query string state. */
const _TYPE_STATE_KEY = 'type';

/** @type {string | string[]} */
const _LOCALE = navigator.languages || navigator.language;

/**
 * Returns shortName for a tree node.
 * @param {TreeNode} node
 */
function shortName(node) {
  return node.idPath.slice(node.shortNameIndex);
}

/**
 * Iterate through each type in the query string. Types can be expressed as
 * repeats of the same key in the query string ("type=b&type=p") or as a long
 * string with multiple characters ("type=bp").
 * @param {string[]} typesList All values associated with the "type" key in the
 * query string.
 */
function* types(typesList) {
  for (const typeOrTypes of typesList) {
    for (const typeChar of typeOrTypes) {
      yield typeChar;
    }
  }
}

/**
 * Limit how frequently `func` is called.
 * @template {T}
 * @param {T & Function} func
 * @param {number} wait Time to wait before func can be called again (ms).
 * @returns {T}
 */
function debounce(func, wait) {
  /** @type {number} */
  let timeoutId;
  function debounced (...args) {
    clearTimeout(timeoutId);
    timeoutId = setTimeout(() => func(...args), wait);
  };
  return /** @type {any} */ (debounced);
}
