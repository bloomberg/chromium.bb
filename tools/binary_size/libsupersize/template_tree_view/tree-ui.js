// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

/**
 * @fileoverview
 * UI classes and methods for the Tree View in the
 * Binary Size Analysis HTML report.
 */

{
  /** Capture one of: "::", "../", "./", "/", "#" */
  const _SPECIAL_CHAR_REGEX = /(::|(?:\.*\/)+|#)/g;
  /** Insert zero-width space after capture group */
  const _ZERO_WIDTH_SPACE = '$&\u200b';

  // Templates for tree nodes in the UI.
  /** @type {HTMLTemplateElement} Template for leaves in the tree */
  const _leafTemplate = document.getElementById('treenode-symbol');
  /** @type {HTMLTemplateElement} Template for trees */
  const _treeTemplate = document.getElementById('treenode-container');

  const _symbolTree = document.getElementById('symboltree');

  /**
   * @type {HTMLCollectionOf<HTMLAnchorElement | HTMLSpanElement>}
   * HTMLCollection of all nodes. Updates itself automatically.
   */
  const _liveNodeList = document.getElementsByClassName('node');
  /**
   * @type {HTMLCollectionOf<HTMLSpanElement>}
   * HTMLCollection of all size elements. Updates itself automatically.
   */
  const _liveSizeSpanList = document.getElementsByClassName('size');

  /**
   * @type {WeakMap<HTMLElement, Readonly<TreeNode>>}
   * Associates UI nodes with the corresponding tree data object
   * so that event listeners and other methods can
   * query the original data.
   */
  const _uiNodeData = new WeakMap();

  /**
   * Create the contents for the size element of a tree node.
   * If in method count mode, size instead represents the amount of methods in
   * the node. In this case, don't append a unit at the end.
   * @param {TreeNode} node Node whose childStats will be polled to find the
   * number of methods to use for the count text
   * @returns {GetSizeResult} Object with hover text title and
   * size element body. Can be consumed by `_applySizeFunc()`
   */
  function _getMethodCountContents(node) {
    const {count: methodCount = 0} =
      node.childStats[_DEX_METHOD_SYMBOL_TYPE] || {};
    const methodStr = methodCount.toLocaleString(_LOCALE, {
      useGrouping: true,
    });

    return {
      element: document.createTextNode(methodStr),
      description: `${methodStr} method${methodCount === 1 ? '' : 's'}`,
      value: methodCount,
    };
  }

  /**
   * Create the contents for the size element of a tree node.
   * The unit to use is selected from the current state, and the original number
   * of bytes will be displayed as hover text over the element.
   * @param {TreeNode} node Node whose size is the number of bytes to use for
   * the size text
   * @param {string} unit Byte unit to use, such as 'MiB'.
   * @returns {GetSizeResult} Object with hover text title and
   * size element body. Can be consumed by `_applySizeFunc()`
   */
  function _getSizeContents(node, unit) {
    const bytes = node.size;
    // Format the bytes as a number with 2 digits after the decimal point
    const text = (bytes / _BYTE_UNITS[unit]).toLocaleString(_LOCALE, {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    });
    const textNode = document.createTextNode(`${text} `);

    // Display the suffix with a smaller font
    const suffixElement = dom.textElement('small', unit);

    const bytesGrouped = bytes.toLocaleString(_LOCALE, {useGrouping: true});

    return {
      element: dom.createFragment([textNode, suffixElement]),
      description: `${bytesGrouped} bytes`,
      value: bytes,
    };
  }

  /**
   * Replace the contexts of the size element for a tree node, using a
   * predefined function which returns a title string and DOM element.
   * @param {GetSize} sizeFunc
   * @param {HTMLElement} sizeElement Element that should display the size
   * @param {TreeNode} node
   */
  function _applySizeFunc(sizeFunc, sizeElement, node, unit) {
    const {description, element, value} = sizeFunc(node, unit);

    // Replace the contents of '.size' and change its title
    dom.replace(sizeElement, element);
    sizeElement.title = description;

    if (value < 0) {
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
      // Update DOM
      lastFocused.tabIndex = -1;
    }
    const element = typeof el === 'number' ? _liveNodeList[el] : el;
    if (element != null) {
      // Update DOM
      element.tabIndex = 0;
      element.focus();
    }
  }

  /**
   * Click event handler to expand or close the child group of a tree.
   * @param {Event} event
   */
  async function _toggleTreeElement(event) {
    event.preventDefault();

    /** @type {HTMLAnchorElement | HTMLSpanElement} */
    const link = event.currentTarget;
    const element = link.parentElement;
    const group = link.nextElementSibling;

    const isExpanded = element.getAttribute('aria-expanded') === 'true';
    if (isExpanded) {
      // Update DOM
      element.setAttribute('aria-expanded', 'false');
      dom.replace(group, null);
    } else {
      element.setAttribute('aria-expanded', 'true');

      let data = _uiNodeData.get(link);
      if (data == null || data.children == null) {
        const idPath = link.querySelector('.symbol-name').title;
        data = await worker.openNode(idPath);
        _uiNodeData.set(link, data);
      }

      const newElements = data.children.map(child => newTreeElement(child));
      if (newElements.length === 1) {
        // Open the inner element if it only has a single child.
        // Ensures nodes like "java"->"com"->"google" are opened all at once.
        newElements[0].querySelector('.node').click();
      }
      const newElementsFragment = dom.createFragment(newElements);

      // Update DOM
      requestAnimationFrame(() => {
        group.appendChild(newElementsFragment);
      });
    }
  }

  /**
   * Keydown event handler to move focus for the given element
   * @param {KeyboardEvent} event
   */
  function _handleKeyNavigation(event) {
    /** @type {HTMLAnchorElement | HTMLSpanElement} */
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

    /**
     * Focus the tree element at `index` if it starts with `char`
     * @param {string} char
     * @param {number} index
     */
    function _focusIfStartsWith(char, index) {
      const data = _uiNodeData.get(_liveNodeList[index]);
      if (shortName(data).startsWith(char)) {
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
      case 'ArrowRight': {
        const data = _uiNodeData.get(link);
        if (!data.children || data.children.length !== 0) {
          const isExpanded =
            link.parentElement.getAttribute('aria-expanded') === 'true';
          if (isExpanded) {
            _focusNext();
          } else {
            _toggle();
          }
        }
        break;
      }
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
      // Expand all sibling nodes
      case '*':
        const groupList = link.parentElement.parentElement;
        if (groupList.getAttribute('role') === 'group') {
          event.preventDefault();
          for (const li of groupList.children) {
            if (li.getAttribute('aria-expanded') !== 'true') {
              li.querySelector('.node').click();
            }
          }
        }
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
   * @returns {DocumentFragment}
   */
  function newTreeElement(data) {
    const isLeaf = data.children && data.children.length === 0;
    const template = isLeaf ? _leafTemplate : _treeTemplate;
    const element = document.importNode(template.content, true);

    // Associate clickable node & tree data
    /** @type {HTMLAnchorElement | HTMLSpanElement} */
    const link = element.querySelector('.node');
    _uiNodeData.set(link, Object.freeze(data));

    // Icons are predefined in the HTML through hidden SVG elements
    const type = data.type[0];
    const icon = getIconTemplate(type);
    if (_CONTAINER_TYPE_SET.has(type)) {
      const symbolStyle = getIconStyle(data.type[1]);
      icon.setAttribute('fill', symbolStyle.color);
    }
    // Insert an SVG icon at the start of the link to represent type
    link.insertBefore(icon, link.firstElementChild);

    // Set the symbol name and hover text
    /** @type {HTMLSpanElement} */
    const symbolName = element.querySelector('.symbol-name');
    symbolName.textContent = shortName(data).replace(
      _SPECIAL_CHAR_REGEX,
      _ZERO_WIDTH_SPACE
    );
    symbolName.title = data.idPath;

    // Set the byte size and hover text
    const _getSizeLabels = state.has('method_count')
      ? _getMethodCountContents
      : _getSizeContents;
    _applySizeFunc(
      _getSizeLabels,
      element.querySelector('.size'),
      data,
      state.get('byteunit', {default: 'MiB', valid: _BYTE_UNITS_SET})
    );

    link.addEventListener('mouseover', event =>
      displayInfocard(_uiNodeData.get(event.currentTarget), _getSizeLabels)
    );
    if (!isLeaf) {
      link.addEventListener('click', _toggleTreeElement);
    }

    return element;
  }

  // When the `byteunit` state changes, update all .size elements in the page
  form.elements.namedItem('byteunit').addEventListener('change', event => {
    const unit = event.currentTarget.value;
    // Update existing size elements with the new unit
    for (const sizeElement of _liveSizeSpanList) {
      const data = _uiNodeData.get(sizeElement.parentElement);
      _applySizeFunc(_getSizeContents, sizeElement, data, unit);
    }
  });

  _symbolTree.addEventListener('keydown', _handleKeyNavigation);
  _symbolTree.addEventListener('focusin', event => {
    displayInfocard(
      _uiNodeData.get(event.target),
      state.has('method_count') ? _getMethodCountContents : _getSizeContents
    );
    event.currentTarget.parentElement.classList.add('focused');
  });
  _symbolTree.addEventListener('focusout', event =>
    event.currentTarget.parentElement.classList.remove('focused')
  );

  self.newTreeElement = newTreeElement;
}

{
  /** @type {HTMLUListElement} */
  const _symbolTree = document.getElementById('symboltree');
  /** @type {HTMLProgressElement} */
  const _progress = document.getElementById('progress');

  /**
   * Displays the given data as a tree view
   * @param {TreeProgress} param0
   */
  function displayTree({root, percent, diffMode, error}) {
    /** @type {DocumentFragment | null} */
    let rootElement = null;
    if (root) {
      rootElement = newTreeElement(root);
      /** @type {HTMLAnchorElement} */
      const link = rootElement.querySelector('.node');
      // Expand the root UI node
      link.click();
      link.tabIndex = 0;
    }

    requestAnimationFrame(() => {
      _progress.value = percent;
      if (error) {
        document.body.classList.add('error');
      } else {
        document.body.classList.remove('error');
      }
      if (diffMode) {
        document.body.classList.add('diff');
      } else {
        document.body.classList.remove('diff');
      }

      dom.replace(_symbolTree, rootElement);
    });
  }

  treeReady.then(displayTree);
  worker.setOnProgressHandler(displayTree);

  form.addEventListener('change', event => {
    if (event.target.dataset.dynamic == null) {
      _progress.value = 0;
      worker.loadTree().then(displayTree);
    }
  });
  form.addEventListener('submit', event => {
    event.preventDefault();
    _progress.value = 0;
    worker.loadTree().then(displayTree);
  });
}
