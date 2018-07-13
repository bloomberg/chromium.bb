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
 * @typedef {object} Meta
 * @prop {string[]} components
 * @prop {number} total
 * @prop {boolean} diff_mode
 */
/**
 * @typedef {object} SymbolEntry JSON object representing a single symbol.
 * @prop {string} n Name of the symbol.
 * @prop {number} b Byte size of the symbol, divided by num_aliases.
 * @prop {string} t Single character string to indicate the symbol type.
 * @prop {number} [u] Count value indicating how many symbols this entry
 * represents. Negative value when removed in a diff.
 */
/**
 * @typedef {object} FileEntry JSON object representing a single file and its
 * symbols.
 * @prop {string} p Path to the file (source_path).
 * @prop {number} c Index of the file's component in meta (component_index).
 * @prop {SymbolEntry[]} s - Symbols belonging to this node. Array of objects.
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
 * Make a node with some default arguments
 * @param {Partial<TreeNode> & {shortName:string}} options
 * Values to use for the node. If a value is
 * omitted, a default will be used instead.
 * @returns {TreeNode}
 */
function createNode(options) {
  const {idPath, type, shortName, size = 0, childStats = {}} = options;
  return {
    children: [],
    parent: null,
    childStats,
    idPath,
    shortNameIndex: idPath.lastIndexOf(shortName),
    size,
    type,
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
   */
  constructor(options) {
    this._getPath = options.getPath;
    this._filterTest = options.filterTest;
    this._sep = options.sep || _PATH_SEP;

    this.rootNode = createNode({
      idPath: this._sep,
      shortName: this._sep,
      type: this._containerType(this._sep),
    });
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

    const additionalSize = node.size;
    const additionalStats = Object.entries(node.childStats);

    // Update the size and childStats of all ancestors
    while (node != null && node.parent != null) {
      const [containerType, lastBiggestType] = node.parent.type;
      let {size: lastBiggestSize = 0} =
        node.parent.childStats[lastBiggestType] || {};
      for (const [type, stat] of additionalStats) {
        const parentStat = node.parent.childStats[type] || {size: 0, count: 0};

        parentStat.size += stat.size;
        parentStat.count += stat.count;
        node.parent.childStats[type] = parentStat;

        const absSize = Math.abs(parentStat.size);
        if (absSize > lastBiggestSize) {
          node.parent.type = `${containerType}${type}`;
          lastBiggestSize = absSize;
        }
      }

      node.parent.size += additionalSize;
      node = node.parent;
    }
  }

  /**
   * Merges dex method symbols such as "Controller#get" and "Controller#set"
   * into containers, based on the class of the dex methods.
   * @param {TreeNode} node
   */
  static _joinDexMethodClasses(node) {
    const hasDexMethods = node.childStats[_DEX_METHOD_SYMBOL_TYPE] != null;
    if (!hasDexMethods || node.children == null) return node;

    if (node.type[0] === _CONTAINER_TYPES.FILE) {
      /** @type {Map<string, TreeNode>} */
      const javaClassContainers = new Map();
      /** @type {TreeNode[]} */
      const otherSymbols = [];

      // Place all dex methods into buckets
      for (const childNode of node.children) {
        // Java classes are denoted with a "#", such as "LogoView#onDraw"
        const splitIndex = childNode.idPath.lastIndexOf('#');

        const isDexMethodWithClass =
            childNode.type === _DEX_METHOD_SYMBOL_TYPE &&
            splitIndex > childNode.shortNameIndex;

        if (isDexMethodWithClass) {
          // Get the idPath of the class
          const classIdPath = childNode.idPath.slice(0, splitIndex);

          let classNode = javaClassContainers.get(classIdPath);
          if (classNode == null) {
            classNode = createNode({
              idPath: classIdPath,
              shortName: classIdPath.slice(childNode.shortNameIndex),
              type: _CONTAINER_TYPES.JAVA_CLASS,
            });
            javaClassContainers.set(classIdPath, classNode);
          }

          // Adjust the dex method's short name so it starts after the "#"
          childNode.shortNameIndex = splitIndex + 1;
          TreeBuilder._attachToParent(childNode, classNode);
        } else {
          otherSymbols.push(childNode);
        }
      }

      node.children = otherSymbols;
      for (const containerNode of javaClassContainers.values()) {
        // Delay setting the parent until here so that `_attachToParent`
        // doesn't add method stats twice
        containerNode.parent = node;
        node.children.push(containerNode);
      }
    } else {
      node.children.forEach(TreeBuilder._joinDexMethodClasses);
    }
    return node;
  }

  /**
   * Formats a tree node by removing references to its desendants and ancestors.
   * This reduces how much data is sent to the UI thread at once. For large
   * trees, serialization and deserialization of the entire tree can take ~7s.
   *
   * Only children up to `depth` will be kept, and deeper children will be
   * replaced with `null` to indicate that there were children by they were
   * removed.
   *
   * Leaves with no children will always have an empty children array.
   * If a tree has only 1 child, it is kept as the UI will expand chains of
   * single children in the tree.
   *
   * Additionally sorts the formatted portion of the tree.
   * @param {TreeNode} node Node to format
   * @param {number} depth How many levels of children to keep.
   * @returns {TreeNode}
   */
  static formatNode(node, depth = 1) {
    const childDepth = depth - 1;
    // `null` represents that the children have not been loaded yet
    let children = null;
    if (depth > 0 || node.children.length <= 1) {
      // If depth is larger than 0, include the children.
      // If there are 0 children, include the empty array to indicate the node
      // is a leaf.
      // If there is 1 child, include it so the UI doesn't need to make a
      // roundtrip in order to expand the chain.
      children = node.children
          .map(n => TreeBuilder.formatNode(n, childDepth))
          .sort(_compareFunc);
    }

    return TreeBuilder._joinDexMethodClasses({
      ...node,
      children,
      parent: null,
    });
  }

  /**
   * Returns the container type for a parent node.
   * @param {string} childIdPath
   * @private
   */
  _containerType(childIdPath) {
    const useAlternateType =
      childIdPath.lastIndexOf(this._sep) > childIdPath.lastIndexOf(_PATH_SEP);
    if (useAlternateType) {
      return _CONTAINER_TYPES.COMPONENT;
    } else {
      return _CONTAINER_TYPES.DIRECTORY;
    }
  }

  /**
   * Helper to return the parent of the given node. The parent is determined
   * based in the idPath and the path seperator. If the parent doesn't yet
   * exist, one is created and stored in the parents map.
   * @param {TreeNode} childNode
   * @private
   */
  _getOrMakeParentNode(childNode) {
    // Get idPath of this node's parent.
    let parentPath;
    if (childNode.idPath === '') parentPath = _NO_NAME;
    else parentPath = dirname(childNode.idPath, this._sep);

    // check if parent exists
    let parentNode;
    if (parentPath === '') {
      // parent is root node if dirname is ''
      parentNode = this.rootNode;
    } else {
      // get parent from cache if it exists, otherwise create it
      parentNode = this._parents.get(parentPath);
      if (parentNode == null) {
        parentNode = createNode({
          idPath: parentPath,
          shortName: basename(parentPath, this._sep),
          type: this._containerType(childNode.idPath),
        });
        this._parents.set(parentPath, parentNode);
      }
    }

    // attach node to the newly found parent
    TreeBuilder._attachToParent(childNode, parentNode);
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
    // insert zero-width spaces after certain characters to indicate to the
    // browser it could add a line break there on small screen sizes.
    const idPath = this._getPath(fileEntry);
    // make node for this
    const fileNode = createNode({
      idPath,
      shortName: basename(filePath, this._sep),
      type: _CONTAINER_TYPES.FILE,
    });
    // build child nodes for this file's symbols and attach to self
    for (const symbol of fileEntry[_KEYS.FILE_SYMBOLS]) {
      const size = symbol[_KEYS.SIZE];
      const type = symbol[_KEYS.TYPE];
      const count = symbol[_KEYS.COUNT] || 1;
      const symbolNode = createNode({
        // Join file path to symbol name with a ":"
        idPath: `${idPath}:${symbol[_KEYS.SYMBOL_NAME]}`,
        shortName: symbol[_KEYS.SYMBOL_NAME],
        size,
        type: symbol[_KEYS.TYPE],
        childStats: {[type]: {size, count}},
      });

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
 * Wrapper around fetch for requesting the same resource multiple times.
 */
class DataFetcher {
  /**
   * @param {string} url URL to the resource you want to fetch.
   */
  constructor(url) {
    this._controller = new AbortController();
    this._url = url;
  }

  /**
   * Starts a new request and aborts the previous one.
   */
  fetch() {
    this._controller.abort();
    this._controller = new AbortController();
    return fetch(this._url, {
      credentials: 'same-origin',
      signal: this._controller.signal,
    });
  }

  /**
   * Transforms a binary stream into a newline delimited JSON (.ndjson) stream.
   * Each yielded value corresponds to one line in the stream.
   * @returns {AsyncIterable<Meta | FileEntry>}
   */
  async *newlineDelimtedJsonStream() {
    const response = await this.fetch();
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
        // Split the chunk base on newlines,
        // and turn each complete line into JSON
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
  if (methodCountMode) {
    typeFilter = new Set(_DEX_METHOD_SYMBOL_TYPE);
  } else {
    typeFilter = new Set(types(params.getAll(_TYPE_STATE_KEY)));
    if (typeFilter.size === 0) {
      typeFilter = new Set(_SYMBOL_TYPE_SET);
      typeFilter.delete('b');
    }
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

  return {groupBy, filterTest};
}

/** @type {TreeBuilder | null} */
let builder = null;
const fetcher = new DataFetcher('data.ndjson');

/**
 * Assemble a tree when this worker receives a message.
 * @param {string} options Query string containing options for the builder.
 * @param {(msg: TreeProgress) => void} onProgress
 */
async function buildTree(options, onProgress) {
  const {groupBy, filterTest} = parseOptions(options);

  /** @type {Meta | null} Object from the first line of the data file */
  let meta = null;

  /** @type {{ [gropyBy: string]: (fileEntry: FileEntry) => string }} */
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
    getPath: getPathMap[groupBy],
    filterTest,
  });

  /**
   * Creates data to post to the UI thread. Defaults will be used for the root
   * and percent values if not specified.
   * @param {{root?:TreeNode,percent?:number,error?:Error}} data Default data
   * values to post.
   */
  function createProgressMessage(data = {}) {
    let {percent} = data;
    if (percent == null) {
      if (meta == null) {
        percent = 0;
      } else {
        percent = Math.max(builder.rootNode.size / meta.total, 0.1);
      }
    }

    const message = {
      root: TreeBuilder.formatNode(data.root || builder.rootNode),
      percent,
      diffMode: meta && meta.diff_mode,
    };
    if (data.error) {
      message.error = data.error.message;
    }
    return message;
  }

  /**
   * Post data to the UI thread. Defaults will be used for the root and percent
   * values if not specified.
   */
  function postToUi() {
    const message = createProgressMessage();
    message.id = 0;
    onProgress(message);
  }

  /** @type {number} ID from setInterval */
  let interval = null;
  try {
    // Post partial state every second
    interval = setInterval(postToUi, 1000);
    for await (const dataObj of fetcher.newlineDelimtedJsonStream()) {
      if (meta == null) {
        // First line of data is used to store meta information.
        meta = /** @type {Meta} */ (dataObj);
        postToUi();
      } else {
        builder.addFileEntry(/** @type {FileEntry} */ (dataObj));
      }
    }
    clearInterval(interval);

    return createProgressMessage({
      root: builder.build(),
      percent: 1,
    });
  } catch (error) {
    if (interval != null) clearInterval(interval);
    if (error.name === 'AbortError') {
      console.info(error.message);
    } else {
      console.error(error);
      return createProgressMessage({error});
    }
  }
}

/** @type {{[action:string]: (data:any) => Promise<any>}} */
const actions = {
  load(data) {
    return buildTree(data, progress => {
      // @ts-ignore
      self.postMessage(progress);
    });
  },
  async open(path) {
    if (!builder) throw new Error('Called open before load');
    const node = builder.find(path);
    return TreeBuilder.formatNode(node);
  },
};

/**
 * Call the requested action function with the given data. If an error is thrown
 * or rejected, post the error message to the UI thread.
 * @param {MessageEvent} event Event for when this worker receives a message.
 */
self.onmessage = async event => {
  const {id, action, data} = event.data;
  try {
    const result = await actions[action](data);
    // @ts-ignore
    self.postMessage({id, result});
  } catch (err) {
    // @ts-ignore
    self.postMessage({id, error: err.message});
    throw err;
  }
};
