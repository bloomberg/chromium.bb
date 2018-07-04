// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

/**
 * @fileoverview
 * Web worker code to parse JSON data from binary_size into data for the UI to
 * display.
 * Note: backticks (\`) are banned from this file as the file contents are
 * turned into a Javascript string encapsulated by backticks.
 */

/**
 * @typedef {object} DataFile JSON data created by html_report python script
 * @prop {FileEntry[]} file_nodes - List of file entries
 * @prop {string[]} source_paths - Array of source_paths referenced by index in
 * the symbols.
 * @prop {string[]} components - Array of components referenced by index in the
 * symbols.
 * @prop {object} meta - Metadata about the data
 */

/**
 * @typedef {object} FileEntry JSON object representing a single file and its
 * symbols.
 * @prop {number} p - source_path
 * @prop {number} c - component_index
 * @prop {Array<{n:string,b:number,t:string}>} s - Symbols belonging to this
 * node. Array of objects.
 *    n - name of the symbol
 *    b - size of the symbol in bytes, divided by num_aliases.
 *    t - single character string to indicate the symbol type
 */

/**
 * @typedef {object} TreeNode Node object used to represent the file tree
 * @prop {TreeNode[]} children Child tree nodes
 * @prop {TreeNode | null} parent Parent tree node. null if this is a root node.
 * @prop {string} idPath
 * @prop {string} shortName
 * @prop {number} size
 * @prop {string} type
 */

/**
 * Abberivated keys used by FileEntrys in the JSON data file.
 * @const
 * @enum {string}
 */
const _KEYS = {
  SOURCE_PATH: 'p',
  COMPONENT_INDEX: 'c',
  FILE_SYMBOLS: 's',
  SYMBOL_NAME: 'n',
  SIZE: 'b',
  TYPE: 't',
};

const _NO_NAME = '(No path)';
const _DIRECTORY_TYPE = 'D';
const _COMPONENT_TYPE = 'C';
const _FILE_TYPE = 'F';

/**
 * Return the basename of the pathname 'path'. In a file path, this is the name
 * of the file and its extension. In a folder path, this is the name of the
 * folder.
 * @param {string} path Path to find basename of.
 * @param {string} sep Path seperator, such as '/'.
 */
function basename(path, sep) {
  const sepIndex = path.lastIndexOf(sep);
  const pathIndex = path.lastIndexOf('/');
  return path.substring(Math.max(sepIndex, pathIndex) + 1);
}

/**
 * Return the basename of the pathname 'path'. In a file path, this is the
 * full path of its folder.
 * @param {string} path Path to find dirname of.
 * @param {string} sep Path seperator, such as '/'.
 */
function dirname(path, sep) {
  const sepIndex = path.lastIndexOf(sep);
  const pathIndex = path.lastIndexOf('/');
  return path.substring(0, Math.max(sepIndex, pathIndex));
}

/**
 * Compare two nodes for sorting. Used in sortTree.
 * @param {TreeNode} a
 * @param {TreeNode} b
 */
function _compareFunc(a, b) {
  return Math.abs(b.size) - Math.abs(a.size);
}

/**
 * Sorts nodes in place based on their sizes.
 * @param {TreeNode} node Node whose children will be sorted. Will be modified
 * by this function.
 */
function sortTree(node) {
  node.children.sort(_compareFunc);
  node.children.forEach(sortTree);
}

/**
 * Link a node to a new parent. Will go up the tree to update parent sizes to
 * include the new child.
 * @param {TreeNode} node Child node.
 * @param {TreeNode} parent New parent node.
 */
function attachToParent(node, parent) {
  // Link the nodes together
  parent.children.push(node);
  node.parent = parent;

  // Update the size of all ancestors
  const {size} = node;
  while (node != null && node.parent != null) {
    node.parent.size += size;
    node = node.parent;
  }
}

/**
 * Make a node with some default arguments
 * @param {Partial<TreeNode>} options Values to use for the node. If a value is
 * omitted, a default will be used instead.
 * @param {string} sep Path seperator, such as '/'. Used for creating a default
 * shortName.
 * @returns {TreeNode}
 */
function createNode(options, sep) {
  const {idPath, type, shortName = basename(idPath, sep), size = 0} = options;
  return {
    children: [],
    parent: null,
    idPath,
    shortName,
    size,
    type,
  };
}

/**
 * Build a tree from a list of symbol objects.
 * @param {object} options
 * @param {Iterable<FileEntry>} options.symbols List of basic symbols.
 * @param {(file: FileEntry) => string} options.getPath Called to get the id
 * path of a symbol's file.
 * @param {(symbol: TreeNode) => boolean} options.filterTest Called to see if
 * a symbol should be included. If a symbol fails the test, it will not be
 * attached to the tree.
 * @param {string} [options.sep] Path seperator used to find parent names.
 * @param {boolean} [options.methodCountMode] If true, return number of dex
 * methods instead of size.
 * @returns {TreeNode} Root node of the new tree
 */
function makeTree(options) {
  const {
    symbols,
    getPath,
    filterTest,
    sep = '/',
    methodCountMode = false,
  } = options;
  if (!getPath) throw new TypeError('Missing getPath');

  const rootNode = createNode(
    {idPath: sep, shortName: sep, type: _DIRECTORY_TYPE},
    sep
  );

  /** @type {Map<string, TreeNode>} Cache for directory nodes */
  const parents = new Map();
  /**
   * Helper to return the parent of the given node. The parent is determined
   * based in the idPath and the path seperator. If the parent doesn't yet
   * exist, one is created and stored in the parents map.
   * @param {TreeNode} node
   */
  function getOrMakeParentNode(node) {
    // Get idPath of this node's parent.
    let parentPath;
    if (node.idPath === '') parentPath = _NO_NAME;
    else parentPath = dirname(node.idPath, sep);

    // check if parent exists
    let parentNode;
    if (parentPath === '') {
      // parent is root node if dirname is ''
      parentNode = rootNode;
    } else {
      // get parent from cache if it exists, otherwise create it
      parentNode = parents.get(parentPath);
      if (parentNode == null) {
        const useAltType =
          node.idPath.lastIndexOf(sep) > node.idPath.lastIndexOf('/');
        parentNode = createNode(
          {
            idPath: parentPath,
            type: useAltType ? _COMPONENT_TYPE : _DIRECTORY_TYPE,
          },
          sep
        );
        parents.set(parentPath, parentNode);
      }
    }

    // attach node to the newly found parent
    attachToParent(node, parentNode);
  }

  // Iterate through every file node generated by supersize. Each node includes
  // symbols that belong to that file. Create a tree node for each file with
  // tree nodes for that file's symbols attached. Afterwards attach that node to
  // its parent directory node, or create it if missing.
  for (const fileNode of symbols) {
    // make path for this
    const filePath = fileNode[_KEYS.SOURCE_PATH];
    const idPath = getPath(fileNode);
    // make node for this
    const node = createNode(
      {idPath, shortName: basename(filePath, sep), type: _FILE_TYPE},
      sep
    );
    // build child nodes for this file's symbols and attach to self
    for (const symbol of fileNode[_KEYS.FILE_SYMBOLS]) {
      const size = methodCountMode ? 1 : symbol[_KEYS.SIZE];
      const symbolNode = createNode(
        {
          idPath: idPath + ':' + symbol[_KEYS.SYMBOL_NAME],
          shortName: symbol[_KEYS.SYMBOL_NAME],
          size,
          type: symbol[_KEYS.TYPE],
        },
        sep
      );
      if (filterTest(symbolNode)) attachToParent(symbolNode, node);
    }
    // build parent node and attach file to parent,
    // unless we filtered out every symbol belonging to this file
    if (node.children.length > 0) getOrMakeParentNode(node);
  }

  // build parents for the directories until reaching the root node
  for (const directory of parents.values()) {
    getOrMakeParentNode(directory);
  }

  // Sort the tree so that larger items are higher.
  sortTree(rootNode);

  return rootNode;
}

/**
 * Assemble a tree when this worker receives a message.
 * @param {MessageEvent} event Event for when this worker receives a message.
 */
self.onmessage = event => {
  /**
   * @type {{tree:DataFile,options:string}} JSON data parsed from a string
   * supplied by the UI thread. Includes JSON representing the symbol tree as a
   * flat list of files, and options represented as a query string.
   */
  const {tree, options} = JSON.parse(event.data);

  const params = new URLSearchParams(options);
  const groupBy = params.get('group_by') || 'source_path';
  const methodCountMode = params.has('method_count');
  let typeFilter;
  if (methodCountMode) typeFilter = new Set('m');
  else {
    const types = params.getAll('types');
    typeFilter = new Set(types.length === 0 ? 'bdrtv*xmpPo' : types);
  }

  const getPathMap = {
    component(fileEntry) {
      const component = tree.components[fileEntry[_KEYS.COMPONENT_INDEX]];
      const path = getPathMap.source_path(fileEntry);
      return (component || '(No component)') + '>' + path;
    },
    source_path(fileEntry) {
      return fileEntry[_KEYS.SOURCE_PATH];
    },
  };
  const sepMap = {
    component: '>',
  };

  const rootNode = makeTree({
    symbols: tree.file_nodes,
    sep: sepMap[groupBy],
    methodCountMode,
    getPath: getPathMap[groupBy],
    filterTest: s => typeFilter.has(s.type),
  });

  // @ts-ignore
  self.postMessage(rootNode);
};
