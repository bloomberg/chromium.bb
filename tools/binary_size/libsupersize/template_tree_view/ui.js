// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * UI classes and methods for the Tree View in the
 * Binary Size Analysis HTML report.
 */

{
  /**
   * @enum {number} Various byte units and the corresponding amount of bytes
   * that one unit represents.
   */
  const _BYTE_UNITS = {
    GiB: 1024 ** 3,
    MiB: 1024 ** 2,
    KiB: 1024 ** 1,
    B: 1024 ** 0,
  };
  /** Set of all byte units */
  const _BYTE_UNITS_SET = new Set(Object.keys(_BYTE_UNITS));

  const _icons = document.getElementById('icons');
  /**
   * @enum {SVGSVGElement} Icon elements that correspond to each symbol type.
   */
  const _SYMBOL_ICONS = {
    D: _icons.querySelector('.foldericon'),
    F: _icons.querySelector('.fileicon'),
    b: _icons.querySelector('.bssicon'),
    d: _icons.querySelector('.dataicon'),
    r: _icons.querySelector('.readonlyicon'),
    t: _icons.querySelector('.codeicon'),
    v: _icons.querySelector('.vtableicon'),
    '*': _icons.querySelector('.generatedicon'),
    x: _icons.querySelector('.dexicon'),
    m: _icons.querySelector('.dexmethodicon'),
    p: _icons.querySelector('.localpakicon'),
    P: _icons.querySelector('.nonlocalpakicon'),
    o: _icons.querySelector('.othericon'), // used as default icon
  };

  // Templates for tree nodes in the UI.
  const _leafTemplate = document.getElementById('treeitem');
  const _treeTemplate = document.getElementById('treefolder');

  const _symbolTree = document.getElementById('symboltree');

  /** HTMLCollection of all nodes. Updates itself automatically. */
  const _liveNodeList = document.getElementsByClassName('node');

  /**
   * @type {WeakMap<HTMLElement, Readonly<TreeNode>>}
   * Associates UI nodes with the corresponding tree data object
   * so that event listeners and other methods can
   * query the original data.
   */
  const _uiNodeData = new WeakMap();

  /**
   * Replace the contexts of the size element for a tree node.
   * If in method count mode, size instead represents the amount of methods in
   * the node. In this case, don't append a unit at the end.
   * @param {HTMLElement} sizeElement Element that should display the count
   * @param {number} methodCount Number of methods to use for the count text
   */
  function _setMethodCountContents(sizeElement, methodCount) {
    const methodStr = methodCount.toLocaleString(undefined, {
      useGrouping: true,
    });

    const textNode = document.createTextNode(methodStr);
    dom.replace(sizeElement, textNode);
    sizeElement.title = `${methodStr} methods`;
  }

  /**
   * Replace the contexts of the size element for a tree node.
   * The unit to use is selected from the current state,
   * and the original number of bytes will be displayed as
   * hover text over the element.
   * @param {HTMLElement} sizeElement Element that should display the size
   * @param {number} bytes Number of bytes to use for the size text
   */
  function _setSizeContents(sizeElement, bytes) {
    // Get unit from query string, will fallback for invalid query
    const suffix = state.get('byteunit', {
      default: 'MiB',
      valid: _BYTE_UNITS_SET,
    });
    const value = _BYTE_UNITS[suffix];

    // Format the bytes as a number with 2 digits after the decimal point
    const text = (bytes / value).toLocaleString(undefined, {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    });
    const textNode = document.createTextNode(`${text} `);

    // Display the suffix with a smaller font
    const suffixElement = document.createElement('small');
    suffixElement.textContent = suffix;

    // Replace the contents of '.size' and change its title
    dom.replace(sizeElement, dom.createFragment([textNode, suffixElement]));
    sizeElement.title =
      bytes.toLocaleString(undefined, {useGrouping: true}) + ' bytes';

    if (bytes < 0) {
      sizeElement.classList.add('negative');
    } else {
      sizeElement.classList.remove('negative');
    }
  }

  /**
   * Sets focus to a new tree element while updating the element that last had
   * focus. The tabindex property is used to avoid needing to tab through every
   * single tree item in the page to reach other areas.
   * @param {number | HTMLElement} el Index of tree node in `_liveNodeList`
   */
  function _focusTreeElement(el) {
    const lastFocused = document.activeElement;
    if (_uiNodeData.has(lastFocused)) {
      lastFocused.tabIndex = -1;
    }
    const element = typeof el === 'number' ? _liveNodeList[el] : el;
    if (element != null) {
      element.tabIndex = 0;
      element.focus();
    }
  }

  /**
   * Click event handler to expand or close the child group of a tree.
   * @param {Event} event
   */
  function _toggleTreeElement(event) {
    event.preventDefault();

    const link = event.currentTarget;
    const element = link.parentElement;
    const group = link.nextElementSibling;

    const isExpanded = element.getAttribute('aria-expanded') === 'true';
    if (isExpanded) {
      element.setAttribute('aria-expanded', 'false');
      dom.replace(group, null);
    } else {
      const data = _uiNodeData.get(link);
      group.appendChild(
        dom.createFragment(data.children.map(child => newTreeElement(child)))
      );
      element.setAttribute('aria-expanded', 'true');
    }
  }

  /**
   * Keydown event handler to move focus for the given element
   * @param {KeyboardEvent} event
   */
  function _handleKeyNavigation(event) {
    const link = event.target;
    const focusIndex = Array.prototype.indexOf.call(_liveNodeList, link);

    /** Focus the tree element immediately following this one */
    function _focusNext() {
      if (focusIndex > -1 && focusIndex < _liveNodeList.length - 1) {
        event.preventDefault();
        _focusTreeElement(focusIndex + 1);
      }
    }

    /** Open or close the tree element */
    function _toggle() {
      event.preventDefault();
      link.click();
    }

    /** Focus the tree element at `index` if it starts with `char` */
    function _focusIfStartsWith(char, index) {
      const data = _uiNodeData.get(_liveNodeList[index]);
      if (data.shortName.startsWith(char)) {
        event.preventDefault();
        _focusTreeElement(index);
        return true;
      } else {
        return false;
      }
    }

    switch (event.key) {
      // Space should act like clicking or pressing enter & toggle the tree.
      case ' ':
        _toggle();
        break;
      // Move to previous focusable node
      case 'ArrowUp':
        if (focusIndex > 0) {
          event.preventDefault();
          _focusTreeElement(focusIndex - 1);
        }
        break;
      // Move to next focusable node
      case 'ArrowDown':
        _focusNext();
        break;
      // If closed tree, open tree. Otherwise, move to first child.
      case 'ArrowRight':
        if (_uiNodeData.get(link).children.length !== 0) {
          const isExpanded =
            link.parentElement.getAttribute('aria-expanded') === 'true';
          if (isExpanded) {
            _focusNext();
          } else {
            _toggle();
          }
        }
        break;
      // If opened tree, close tree. Otherwise, move to parent.
      case 'ArrowLeft':
        {
          const isExpanded =
            link.parentElement.getAttribute('aria-expanded') === 'true';
          if (isExpanded) {
            _toggle();
          } else {
            const groupList = link.parentElement.parentElement;
            if (groupList.getAttribute('role') === 'group') {
              event.preventDefault();
              _focusTreeElement(groupList.previousElementSibling);
            }
          }
        }
        break;
      // Focus first node
      case 'Home':
        event.preventDefault();
        _focusTreeElement(0);
        break;
      // Focus last node on screen
      case 'End':
        event.preventDefault();
        _focusTreeElement(_liveNodeList.length - 1);
        break;
      // If a letter was pressed, find a node starting with that character.
      default:
        if (event.key.length === 1 && event.key.match(/\S/)) {
          for (let i = focusIndex + 1; i < _liveNodeList.length; i++) {
            if (_focusIfStartsWith(event.key, i)) return;
          }
          for (let i = 0; i < focusIndex; i++) {
            if (_focusIfStartsWith(event.key, i)) return;
          }
        }
        break;
    }
  }

  /**
   * Inflate a template to create an element that represents one tree node.
   * The element will represent a tree or a leaf, depending on if the tree
   * node object has any children. Trees use a slightly different template
   * and have click event listeners attached.
   * @param {TreeNode} data Data to use for the UI.
   * @returns {HTMLElement}
   */
  function newTreeElement(data) {
    const isLeaf = data.children.length === 0;
    const template = isLeaf ? _leafTemplate : _treeTemplate;
    const element = document.importNode(template.content, true);

    // Associate clickable node & tree data
    const link = element.querySelector('.node');
    _uiNodeData.set(link, Object.freeze(data));

    // Icons are predefined in the HTML through hidden SVG elements
    const iconTemplate = _SYMBOL_ICONS[data.type] || _SYMBOL_ICONS.o;
    const icon = iconTemplate.cloneNode(true);
    // Insert an SVG icon at the start of the link to represent type
    link.insertBefore(icon, link.firstElementChild);

    // Set the symbol name and hover text
    const symbolName = element.querySelector('.symbol-name');
    symbolName.textContent = data.shortName;
    symbolName.title = data.idPath;

    // Set the byte size and hover text
    const _setSize = state.has('method_count')
      ? _setMethodCountContents
      : _setSizeContents;
    _setSize(element.querySelector('.size'), data.size);

    if (!isLeaf) {
      link.addEventListener('click', _toggleTreeElement);
    }

    return element;
  }

  // When the `byteunit` state changes, update all .size elements in the page
  form.byteunit.addEventListener('change', event => {
    // Update existing size elements with the new unit
    for (const sizeElement of document.getElementsByClassName('size')) {
      const data = _uiNodeData.get(sizeElement.parentElement);
      _setSizeContents(sizeElement, data.size);
    }
  });

  _symbolTree.addEventListener('keydown', _handleKeyNavigation);

  self.newTreeElement = newTreeElement;
  self._symbolTree = _symbolTree;
}

{
  const blob = new Blob([
    `
    --INSERT_WORKER_CODE--
    `,
  ]);
  // We use a worker to keep large tree creation logic off the UI thread
  const worker = new Worker(URL.createObjectURL(blob));

  /**
   * Displays the given data as a tree view
   * @param {{data:{root:TreeNode,meta:object}}} param0
   */
  worker.onmessage = ({data}) => {
    const root = newTreeElement(data);
    const node = root.querySelector('.node');
    // Expand the root UI node
    node.click();
    node.tabIndex = 0;

    dom.replace(_symbolTree, root);
  };

  /**
   * Loads the tree data given on a worker thread and replaces the tree view in
   * the UI once complete. Uses query string as state for the options.
   * @param {string} treeData JSON string to be parsed on the worker thread.
   */
  function loadTree(treeData) {
    // Post as a JSON string for better performance
    worker.postMessage(
      `{"tree":${treeData}, "options":"${location.search.slice(1)}"}`
    );
  }

  self.loadTree = loadTree;
}
