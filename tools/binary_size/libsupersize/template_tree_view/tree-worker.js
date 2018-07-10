// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

/**
 * @fileoverview
 * Web worker code to parse JSON data from binary_size into data for the UI to
 * display.
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

importScripts('./shared.js');

const _PATH_SEP = '/';

/**
 * Return the basename of the pathname 'path'. In a file path, this is the name
 * of the file and its extension. In a folder path, this is the name of the
 * folder.
 * @param {string} path Path to find basename of.
 * @param {string} sep Path seperator, such as '/'.
 */
function basename(path, sep) {
  const sepIndex = path.lastIndexOf(sep);
  const pathIndex = path.lastIndexOf(_PATH_SEP);
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
  const pathIndex = path.lastIndexOf(_PATH_SEP);
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
 * Make a node with some default arguments
 * @param {{idPath:string,type:string,shortName?:string,size?:number}} options
 * Values to use for the node. If a value is
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
    childSizes: {},
    idPath,
    shortNameIndex: idPath.lastIndexOf(shortName),
    size,
    type,
  };
}

/**
 * Formats a tree node by removing references to its desendants and ancestors.
 *
 * Only children up to `depth` will be kept, and deeper children will be
 * replaced with `null` to indicate that there were children by they were
 * removed.
 *
 * Leaves will no children will always have an empty children array.
 * If a tree has only 1 child, it is kept as the UI will expand chain of single
 * children in the tree.
 * @param {TreeNode} node Node to format
 * @param {number} depth How many levels of children to keep.
 */
function formatNode(node, depth = 1) {
  const childDepth = depth - 1;
  // `null` represents that the children have not been loaded yet
  let children = null;
  if (depth > 0 || node.children.length <= 1) {
    // If depth is larger than 0, include the children.
    // If there are 0 children, include the empty array to indicate the node is
    // a leaf.
    // If there is 1 child, include it so the UI doesn't need to make a
    // roundtrip in order to expand the chain.
    children = node.children.map(n => formatNode(n, childDepth));
  }

  return {
    ...node,
    children,
    parent: null,
  };
}

/**
 * Class used to build a tree from a list of symbol objects.
 * Add each file node using `addFileEntry()`, then call `build()` to finalize
 * the tree and return the root node. The in-progress tree can be obtained from
 * the `rootNode` property.
 */
class TreeBuilder {
  /**
   * @param {object} options
   * @param {(fileEntry: FileEntry) => string} options.getPath Called to get the
   * id path of a symbol's file entry.
   * @param {(symbolNode: TreeNode) => boolean} options.filterTest Called to see
   * if a symbol should be included. If a symbol fails the test, it will not be
   * attached to the tree.
   * @param {string} options.sep Path seperator used to find parent names.
   * @param {boolean} options.methodCountMode If true, return number of dex
   * methods instead of size.
   */
  constructor(options) {
    this._getPath = options.getPath;
    this._filterTest = options.filterTest;
    this._sep = options.sep || _PATH_SEP;
    this._methodCountMode = options.methodCountMode || false;

    this.rootNode = createNode(
      {
        idPath: this._sep,
        shortName: this._sep,
        type: _CONTAINER_TYPES.DIRECTORY,
      },
      this._sep
    );
    /** @type {Map<string, TreeNode>} Cache for directory nodes */
    this._parents = new Map();

    /**
     * Regex used to split the `idPath` when finding nodes. Equivalent to
     * one of: "/" or |sep|
     */
    this._splitter = new RegExp(`[/${this._sep}]`);
  }

  /**
   * Link a node to a new parent. Will go up the tree to update parent sizes to
   * include the new child.
   * @param {TreeNode} node Child node.
   * @param {TreeNode} parent New parent node.
   */
  static _attachToParent(node, parent) {
    // Link the nodes together
    parent.children.push(node);
    node.parent = parent;

    // Update the size of all ancestors
    const {size} = node;
    const symbolType = node.type.slice(-1);
    const validSymbol = _SYMBOL_TYPE_SET.has(symbolType);
    while (node != null && node.parent != null) {
      // Only add symbol types to `childSizes`
      if (validSymbol) {
        const {childSizes} = node.parent;
        const [containerType, lastBiggestType] = node.parent.type;

        childSizes[symbolType] = size + (childSizes[symbolType] || 0);
        if (childSizes[symbolType] > (childSizes[lastBiggestType] || 0)) {
          node.parent.type = containerType + symbolType;
        }
      }

      node.parent.size += size;
      node = node.parent;
    }
  }

  /**
   * Helper to return the parent of the given node. The parent is determined
   * based in the idPath and the path seperator. If the parent doesn't yet
   * exist, one is created and stored in the parents map.
   * @param {TreeNode} node
   * @private
   */
  _getOrMakeParentNode(node) {
    // Get idPath of this node's parent.
    let parentPath;
    if (node.idPath === '') parentPath = _NO_NAME;
    else parentPath = dirname(node.idPath, this._sep);

    // check if parent exists
    let parentNode;
    if (parentPath === '') {
      // parent is root node if dirname is ''
      parentNode = this.rootNode;
    } else {
      // get parent from cache if it exists, otherwise create it
      parentNode = this._parents.get(parentPath);
      if (parentNode == null) {
        let type = _CONTAINER_TYPES.DIRECTORY;
        if (
          node.idPath.lastIndexOf(this._sep) >
          node.idPath.lastIndexOf(_PATH_SEP)
        ) {
          type = _CONTAINER_TYPES.COMPONENT;
        }

        parentNode = createNode({idPath: parentPath, type}, this._sep);
        this._parents.set(parentPath, parentNode);
      }
    }

    // attach node to the newly found parent
    TreeBuilder._attachToParent(node, parentNode);
    return parentNode;
  }

  /**
   * Iterate through every file node generated by supersize. Each node includes
   * symbols that belong to that file. Create a tree node for each file with
   * tree nodes for that file's symbols attached. Afterwards attach that node to
   * its parent directory node, or create it if missing.
   * @param {FileEntry} fileEntry File entry from data file
   */
  addFileEntry(fileEntry) {
    // make path for this
    const filePath = fileEntry[_KEYS.SOURCE_PATH];
    const idPath = this._getPath(fileEntry);
    // make node for this
    const fileNode = createNode(
      {
        idPath,
        shortName: basename(filePath, this._sep),
        type: _CONTAINER_TYPES.FILE,
      },
      this._sep
    );
    // build child nodes for this file's symbols and attach to self
    for (const symbol of fileEntry[_KEYS.FILE_SYMBOLS]) {
      const symbolNode = createNode(
        {
          // Join file path to symbol name with a ":"
          idPath: `${idPath}:${symbol[_KEYS.SYMBOL_NAME]}`,
          shortName: symbol[_KEYS.SYMBOL_NAME],
          // Method count mode just counts number of dex method symbols.
          size: this._methodCountMode ? 1 : symbol[_KEYS.SIZE],
          // Turn all unknown types into other.
          type: _SYMBOL_TYPE_SET.has(symbol[_KEYS.TYPE])
            ? symbol[_KEYS.TYPE]
            : _OTHER_SYMBOL_TYPE,
        },
        this._sep
      );

      if (this._filterTest(symbolNode)) {
        TreeBuilder._attachToParent(symbolNode, fileNode);
      }
    }
    // unless we filtered out every symbol belonging to this file,
    if (fileNode.children.length > 0) {
      // build all ancestor nodes for this file
      let orphanNode = fileNode;
      while (orphanNode.parent == null && orphanNode !== this.rootNode) {
        orphanNode = this._getOrMakeParentNode(orphanNode);
      }
    }
  }

  /**
   * Finalize the creation of the tree and return the root node.
   */
  build() {
    // Sort the tree so that larger items are higher.
    sortTree(this.rootNode);

    return this.rootNode;
  }

  /**
   * Internal handler for `find` to search for a node.
   * @private
   * @param {string[]} idPathList
   * @param {TreeNode} node
   * @returns {TreeNode | null}
   */
  _find(idPathList, node) {
    if (node == null) {
      return null;
    } else if (idPathList.length === 0) {
      // Found desired node
      return node;
    }

    const [shortNameToFind] = idPathList;
    const child = node.children.find(n => shortName(n) === shortNameToFind);

    return this._find(idPathList.slice(1), child);
  }

  /**
   * Find a node with a given `idPath` by traversing the tree.
   * @param {string} idPath
   */
  find(idPath) {
    // If `idPath` is the root's ID, return the root
    if (idPath === this.rootNode.idPath) {
      return this.rootNode;
    }

    const symbolIndex = idPath.indexOf(':');
    let path;
    if (symbolIndex > -1) {
      const filePath = idPath.slice(0, symbolIndex);
      const symbolName = idPath.slice(symbolIndex + 1);

      path = filePath.split(this._splitter);
      path.push(symbolName);
    } else {
      path = idPath.split(this._splitter);
    }

    // If the path is empty, it refers to the _NO_NAME container.
    if (path[0] === '') {
      path.unshift(_NO_NAME);
    }

    return this._find(path, this.rootNode);
  }
}

/**
 * Transforms a binary stream into a newline delimited JSON (.ndjson) stream.
 * Each yielded value corresponds to one line in the stream.
 * @param {Response} response Response to convert.
 */
async function* newlineDelimtedJsonStream(response) {
  // Are streams supported?
  if (response.body) {
    const decoder = new TextDecoder();
    const decodeOptions = {stream: true};
    const reader = response.body.getReader();

    let buffer = '';
    while (true) {
      // Read values from the stream
      const {done, value} = await reader.read();
      if (done) break;

      // Convert binary values to text chunks.
      const chunk = decoder.decode(value, decodeOptions);
      buffer += chunk;
      // Split the chunk base on newlines, and turn each complete line into JSON
      const lines = buffer.split('\n');
      [buffer] = lines.splice(lines.length - 1, 1);

      for (const line of lines) {
        yield JSON.parse(line);
      }
    }
  } else {
    // In-memory version for browsers without stream support
    const text = await response.text();
    for (const line of text.split('\n')) {
      if (line) yield JSON.parse(line);
    }
  }
}

/**
 * Parse the options represented as a query string, into an object.
 * Includes checks for valid values.
 * @param {string} options Query string
 */
function parseOptions(options) {
  const params = new URLSearchParams(options);

  const groupBy = params.get('group_by') || 'source_path';
  const methodCountMode = params.has('method_count');

  let minSymbolSize = Number(params.get('min_size'));
  if (Number.isNaN(minSymbolSize)) {
    minSymbolSize = 0;
  }

  const includeRegex = params.get('include');
  const excludeRegex = params.get('exclude');

  /** @type {Set<string>} */
  let typeFilter;
  if (methodCountMode) typeFilter = new Set('m');
  else {
    const types = params.getAll('type');
    typeFilter = new Set(types.length === 0 ? _SYMBOL_TYPE_SET : types);
  }

  /** Ensure symbol size is past the minimum */
  const checkSize = s => Math.abs(s.size) >= minSymbolSize;
  /** Ensure the symbol size wasn't filtered out */
  const checkType = s => typeFilter.has(s.type);
  const filters = [checkSize, checkType];

  if (includeRegex) {
    const regex = new RegExp(includeRegex);
    filters.push(s => regex.test(s.idPath));
  }
  if (excludeRegex) {
    const regex = new RegExp(excludeRegex);
    filters.push(s => !regex.test(s.idPath));
  }

  /**
   * Check that a symbol node passes all the filters in the filters array.
   * @param {TreeNode} symbolNode
   */
  function filterTest(symbolNode) {
    return filters.every(fn => fn(symbolNode));
  }

  return {groupBy, methodCountMode, filterTest};
}

/** @type {TreeBuilder | null} */
let builder = null;
let responsePromise = fetch('data.ndjson');

/**
 * Assemble a tree when this worker receives a message.
 * @param {string} options Query string containing options for the builder.
 * @param {(msg: object) => void} callback
 */
async function buildTree(options, callback) {
  const {groupBy, methodCountMode, filterTest} = parseOptions(options);

  /** Object from the first line of the data file */
  let meta = null;

  const getPathMap = {
    component(fileEntry) {
      const component = meta.components[fileEntry[_KEYS.COMPONENT_INDEX]];
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

  builder = new TreeBuilder({
    sep: sepMap[groupBy],
    methodCountMode,
    getPath: getPathMap[groupBy],
    filterTest,
  });

  /**
   * Post data to the UI thread. Defaults will be used for the root and percent
   * values if not specified.
   * @param {{root?:TreeNode,percent?:number,error?:Error}} data Default data
   * values to post.
   */
  function postToUi(data = {}) {
    let {percent} = data;
    if (percent == null) {
      if (meta == null) {
        percent = 0;
      } else {
        percent = Math.max(builder.rootNode.size / meta.total, 0.1);
      }
    }

    let sizeHeader = methodCountMode ? 'Methods' : 'Size';
    if (meta.diff_mode) sizeHeader += ' diff';

    const message = {
      id: 0,
      root: formatNode(data.root || builder.rootNode),
      percent,
      sizeHeader,
    };
    if (data.error) {
      message.error = data.error.message;
    }

    callback(message);
  }

  /** @type {number} ID from setInterval */
  let interval = null;
  try {
    let response = await responsePromise;
    if (response.bodyUsed) {
      // We start the first request when the worker loads so the response is
      // ready earlier. Subsequent requests (such as when filters change) must
      // re-fetch the data file from the cache or network.
      response = await fetch('data.ndjson');
    }

    // Post partial state every second
    interval = setInterval(postToUi, 1000);
    for await (const dataObj of newlineDelimtedJsonStream(response)) {
      if (meta == null) {
        meta = dataObj;
        postToUi();
      } else {
        builder.addFileEntry(dataObj);
      }
    }
    clearInterval(interval);

    postToUi({root: builder.build(), percent: 1});
  } catch (error) {
    if (interval != null) clearInterval(interval);
    console.error(error);
    postToUi({error});
  }
}

/** @type {{[action:string]: (id:number,data:any) => Promise<void>}} */
const actions = {
  load(id, data) {
    return buildTree(data, msg => {
      msg.id = id;
      // @ts-ignore
      self.postMessage(msg);
    });
  },
  async open(id, path) {
    const node = builder.find(path);
    const result = await formatNode(node);
    // @ts-ignore
    self.postMessage({id, result});
  },
};

/**
 * Assemble a tree when this worker receives a message.
 * @param {MessageEvent} event Event for when this worker receives a message.
 */
self.onmessage = async event => {
  const {id, action, data} = event.data;
  try {
    await actions[action](id, data);
  } catch (err) {
    // @ts-ignore
    self.postMessage({id, error: err.message});
    throw err;
  }
};
