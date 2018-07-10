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
 * @prop {{[type: string]: number}} childSizes The sizes of the children
 * of this node, split by the types of the children.
 */

/**
 * @typedef {object} TreeProgress
 * @prop {TreeNode} root Root node and its direct children.
 * @prop {number} percent Number from (0-1] to represent percentage.
 * @prop {string} sizeHeader String to display as the header for the
 * Size column.
 * @prop {string} [error] Error message, if an error occured in the worker.
 * If unset, then there was no error.
 */

/**
 * @typedef {(size: number,unit:string) => {title:string,element:Node}} GetSize
 */

/** Abberivated keys used by FileEntrys in the JSON data file. */
const _KEYS = {
  SOURCE_PATH: 'p',
  COMPONENT_INDEX: 'c',
  FILE_SYMBOLS: 's',
  SYMBOL_NAME: 'n',
  SIZE: 'b',
  TYPE: 't',
};

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
};
const _CONTAINER_TYPE_SET = new Set(Object.values(_CONTAINER_TYPES));

/** Type for an 'other' symbol */
const _OTHER_SYMBOL_TYPE = 'o';

/** Set of all known symbol types. Container types are not included. */
const _SYMBOL_TYPE_SET = new Set('bdrtv*xmpP' + _OTHER_SYMBOL_TYPE);

/** Name used by a directory created to hold symbols with no name. */
const _NO_NAME = '(No path)';

/**
 * Returns shortName for a tree node.
 * @param {TreeNode} node
 */
function shortName(node) {
  return node.idPath.slice(node.shortNameIndex);
}
